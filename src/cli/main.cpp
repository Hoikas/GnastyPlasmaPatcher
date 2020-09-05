/* This file is part of GnastyPlasmaPatcher.
 *
 * GnastyPlasmaPatcher is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnastyPlasmaPatcher is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GnastyPlasmaPatcher.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include <iostream>
#include <vector>

#include <buildinfo.hpp>
#include <errors.hpp>
#include "log2stdio.hpp"
#include <patcher.hpp>

#include <cxxopts.hpp>
#include <string_theory/iostream>

#include <Debug/plDebug.h>
#include <PRP/KeyedObject/plKey.h>
#include <ResManager/plFactory.h>

// ===========================================================================

constexpr int kReturnOK = 0;
constexpr int kReturnOptionsError = 1;
constexpr int kReturnPatcherError = 2;
constexpr int kReturnGenericError = 3;

// ===========================================================================

static plKey request_key(const plKey& srcKey, const std::vector<plKey>& keys)
{
    std::cout << std::endl;
    std::cout << "We were unable to map the key [" << plFactory::ClassName(srcKey->getType())
              << "] '" << srcKey->getName() << "' to a key in the destination page. Please "
                 "provide the name of the key in the destination page." << std::endl;

    // TODO: make this better by allowing things like tab completion.
    do {
        std::cout << "> " << std::flush;
        ST::string name;
        std::cin >> name;

        auto it = std::find_if(keys.begin(), keys.end(),
                               [&name](const plKey& i) {
                                   return name.compare_i(i->getName()) == 0;
                               }
        );
        if (it != keys.end())
            return *it;

        std::cout << std::endl << "That key was not found. Try again? [Y/N] " << std::flush;
        char again;
        std::cin.get(again);
        std::cout << std::endl;
        if (!(again == 'y' || again == 'Y')) {
            std::cout << "Aborting key search." << std::endl;
            break;
        }
    } while(1);

    // failure
    return plKey();
}

// ===========================================================================

int main(int argc, char** argv)
{
    std::cout << gpp::build_info() << std::endl;
    gpp::log::init_stdio();

    cxxopts::Options options("gppcli", "monkey patching utility for Plasma data files");
    options.add_options()
        ("source", "age or prp file to take objects from", cxxopts::value<std::filesystem::path>())
        ("destination", "age or prp file to patch objects into", cxxopts::value<std::filesystem::path>())

        ("h,help", "show help", cxxopts::value<bool>()->default_value("false"))
        ("no-colliders", "don't patch collision", cxxopts::value<bool>()->default_value("false"))
        ("q,quiet", "silence output", cxxopts::value<bool>()->default_value("false"))
    ;
    options.parse_positional({"source", "destination"});
    options.positional_help("<source age/PRP> <destination age/PRP>");

    try {
        auto results = options.parse(argc, argv);
        if (results["help"].as<bool>()) {
            std::cout << options.help() << std::endl;
            return kReturnOK;
        }
        if (results["quiet"].as<bool>())
            plDebug::Init(plDebug::kDLNone);

        std::filesystem::path source, destination;
        try {
            source = results["source"].as<decltype(source)>();
            destination = results["destination"].as<decltype(destination)>();
        } catch (const std::domain_error&) {
            std::cerr << "Error: A source and destination must be given." << std::endl;
            return kReturnOptionsError;
        }

        gpp::patcher patcher(source, destination);
        patcher.set_map_func(request_key);
        if (!results["no-colliders"].as<bool>())
            patcher.process_collision();
        patcher.save_damage(source, destination);
    } catch (const cxxopts::OptionParseException& ex) {
        std::cerr << "Fatal Error! Could not process arguments:" << std::endl;
        std::cerr << ex.what() << std::endl;;
        return kReturnOptionsError;
    } catch (const cxxopts::OptionSpecException& ex) {
        std::cerr << "Fatal Error! Stupid programmer error with the fucking options, dammit:" << std::endl;
        std::cerr << ex.what() << std::endl;;
        return kReturnOptionsError;
    } catch (const gpp::error& ex) {
        std::cerr << "Fatal Error! Could not patch:" << std::endl;
        std::cerr << ex.what() << std::endl;;
        return kReturnPatcherError;
#if !defined(_DEBUG) || defined(NDEBUG)
    } catch (const std::exception& ex) {
        std::cerr << "Fatal Error! Unhandled exception:" << std::endl;
        std::cerr << ex.what() << std::endl;;
        return kReturnGenericError;
    }
#else
    }
#endif

    return kReturnOK;
}
