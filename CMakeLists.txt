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

cmake_minimum_required(VERSION 3.12)
project(GnastyPlasmaPatcher)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    set(CMAKE_CXX_FLAGS "/wd4251 ${CMAKE_CXX_FLAGS}")
endif()

# ===========================================================================

find_package(HSPlasma REQUIRED)
find_package(string_theory REQUIRED)

set(GPP_LIB_HEADERS
    src/lib/errors.hpp
    src/lib/patcher.hpp
)
set(GPP_LIB_SOURCES
    src/lib/patcher_collision.cpp
    src/lib/patcher_base.cpp
)

add_library(gpplib STATIC ${GPP_LIB_HEADERS} ${GPP_LIB_SOURCES})
target_include_directories(gpplib PUBLIC
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/lib>)
target_link_libraries(gpplib PRIVATE HSPlasma)
target_link_libraries(gpplib PUBLIC string_theory)

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
target_link_libraries(gppcli PRIVATE HSPlasma)