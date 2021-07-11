# This file is part of GnastyPlasmaPatcher.
#
# GnastyPlasmaPatcher is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# GnastyPlasmaPatcher is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GnastyPlasmaPatcher.  If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.13)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "" FORCE)
if(DEFINED ENV{VCPKG_DEFAULT_TRIPLET} AND NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "$ENV{VCPKG_DEFAULT_TRIPLET}" CACHE STRING "")
endif()

project(GnastyPlasmaPatcher)

include(CheckIPOSupported)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    set(CMAKE_CXX_FLAGS "/wd4251 ${CMAKE_CXX_FLAGS}")
endif()
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

check_ipo_supported(RESULT IPO_SUPPORTED)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_DEBUG OFF CACHE BOOL "")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ${IPO_SUPPORTED} CACHE BOOL "")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ${IPO_SUPPORTED} CACHE BOOL "")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ${IPO_SUPPORTED} CACHE BOOL "")

#  ===========================================================================

add_custom_target(buildinfo ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/_buildinfo.cpp)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/buildinfo.cpp
                          ${CMAKE_CURRENT_BINARY_DIR}/_buildinfo.cpp
                   COMMAND ${CMAKE_COMMAND} -P
                           ${CMAKE_MODULE_PATH}/buildinfo.cmake)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/buildinfo.cpp PROPERTIES GENERATED TRUE)
add_library(buildinfoobj STATIC ${CMAKE_CURRENT_BINARY_DIR}/buildinfo.cpp)
add_dependencies(buildinfoobj buildinfo)

# ===========================================================================

find_package(HSPlasma REQUIRED)
find_package(string_theory REQUIRED)

set(GPP_LIB_HEADERS
    src/lib/buildinfo.hpp
    src/lib/errors.hpp
    src/lib/patcher.hpp
)
set(GPP_LIB_SOURCES
    src/lib/buildinfo.cpp
    src/lib/patcher_collision.cpp
    src/lib/patcher_base.cpp
)

add_library(gpplib STATIC ${GPP_LIB_HEADERS} ${GPP_LIB_SOURCES})
target_include_directories(gpplib PUBLIC
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/lib>)
target_link_libraries(gpplib PRIVATE buildinfoobj)
target_link_libraries(gpplib PUBLIC HSPlasma)
target_link_libraries(gpplib PUBLIC string_theory)

# Stupid CMake won't install imported targets. Read and weep:
install(FILES
        $<$<NOT:$<CONFIG:DEBUG>>:$<TARGET_PROPERTY:HSPlasma,IMPORTED_LOCATION_RELEASE>>;$<$<CONFIG:DEBUG>:$<TARGET_PROPERTY:HSPlasma,IMPORTED_LOCATION_DEBUG>>
        TYPE BIN
)

# ===========================================================================

find_package(cxxopts REQUIRED)

set(GPP_CLI_HEADERS
    src/cli/log2stdio.hpp
)
set(GPP_CLI_SOURCES
    src/cli/log2stdio.cpp
    src/cli/main.cpp
)

add_executable(gppcli ${GPP_CLI_HEADERS} ${GPP_CLI_SOURCES})
target_include_directories(gppcli PRIVATE
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/cli>)
target_link_libraries(gppcli PRIVATE gpplib)
install(TARGETS gppcli)

# ===========================================================================

find_package(Qt5 COMPONENTS Core Concurrent Widgets)
if(Qt5_FOUND)
    set(GPP_GUI_HEADERS
        src/gui/key_finder.hpp
        src/gui/log2gui.hpp
        src/gui/main.hpp
    )
    set(GPP_GUI_SOURCES
        src/gui/key_finder.cpp
        src/gui/log2gui.cpp
        src/gui/main.cpp
    )

    add_executable(gppgui WIN32 ${GPP_GUI_HEADERS} ${GPP_GUI_SOURCES})
    set_target_properties(gppgui PROPERTIES AUTOMOC ON)
    target_include_directories(gppgui PRIVATE
                               $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/gui>)
    target_link_libraries(gppgui PRIVATE gpplib)
    target_link_libraries(gppgui PUBLIC Qt5::Core Qt5::Concurrent Qt5::Widgets)
    install(TARGETS gppgui)

    # Hunt down that rat-bastard windeployqt
    # Based on https://stackoverflow.com/a/41199492
    if(WIN32 AND TARGET Qt5::qmake AND NOT TARGET Qt5::windeployqt)
        get_target_property(_qt5_qmake_location Qt5::qmake IMPORTED_LOCATION)

        execute_process(
            COMMAND "${_qt5_qmake_location}" -query QT_INSTALL_PREFIX
            RESULT_VARIABLE return_code
            OUTPUT_VARIABLE qt5_install_prefix
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        set(imported_location "${qt5_install_prefix}/bin/windeployqt.exe")
        if(EXISTS ${imported_location})
            add_executable(Qt5::windeployqt IMPORTED)

            set_target_properties(Qt5::windeployqt PROPERTIES
                IMPORTED_LOCATION ${imported_location}
            )
        endif()
    endif()

    if(TARGET Qt5::windeployqt)
        add_custom_command(TARGET gppgui POST_BUILD
                           COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/qt"
                           COMMAND set PATH=%PATH%$<SEMICOLON>${qt5_install_prefix}/bin
                           COMMAND Qt5::windeployqt --dir "${CMAKE_CURRENT_BINARY_DIR}/qt" "$<TARGET_FILE_DIR:gppgui>/$<TARGET_FILE_NAME:gppgui>"
        )
        install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qt/"
                DESTINATION "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}"
        )
    endif()
else()
    message(STATUS "Qt5 was not found -- not building gppgui!")
endif()
