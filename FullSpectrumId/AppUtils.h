#ifndef AppUtils_h
#define AppUtils_h

/* FullSpectrum: a command-line and web interface to the GADRAS Full Spectrum
 Isotope ID algorithm.  Lee Harding and Will Johnson, SNL.
 
 Copyright 2021 National Technology & Engineering Solutions of Sandia, LLC
 (NTESS). Under the terms of Contract DE-NA0003525 with NTESS, the U.S.
 Government retains certain rights in this software.
 For questions contact William Johnson via email at wcjohns@sandia.gov, or
 alternative email of full-spectrum@sandia.gov.
 
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "FullSpectrumId_config.h"

#include <tuple>
#include <string>
#include <vector>

/** This namespace contains functions used to configure and run the application.
 
 */
namespace AppUtils
{

/** An enum to help specify how the invocation of the application is supposed to be used. */
enum class AppUseMode
{
  Server, CommandLine
};

/** Configures application based on command line arguments, and returns whether is being used in command-line mode, or
 web/server mode, as well as the "command line" arguments to pass to Wt for configuring its server.
 
 Will throw exception on error.
 */
std::tuple<AppUseMode,std::vector<std::string>> init_app_config( const int argc, char **argv );


/** Starts the web-server.
 
 Note: not for use with isapi or fcgi connectors.
 
 Will throw exception on error.
 */
void start_server( const std::string &applicationPath, const std::vector<std::string> &args );

/** Stops the server. */
void kill_server();

/** Returns if the server is running or not. */
bool is_server_running();

/** Will block until server is finished, and then cleans up the server (destroys it) and stuff.
 
 Returns the signal that server finished with (SIGKILL, SIGHUP, etc)
 */
int wait_for_server_to_finish();

/** Returns the port the app is being served on.
 
 Will throw exception if not currently being served.
 */
int port_being_served_on();

/** Returns the local URL being served on.

 Example value returned: "http://127.0.0.1:7234"
 
 Will throw exception if not currently being served.
*/
std::string url_being_served_on();


/** Searches for a file or directory, both relative to CWD, as well as executables directory (or absolute path).
 
 the executables directory is determined from argv[0] id non-null and argc>=1.
 
 Returns true if file was found, in which case the filename argument will be modified.
 Returns false if file was not found, in which case the filename argument will not be modified.
 */
bool locate_file( std::string &filename, const bool is_dir, const int argc, char **argv );


#ifdef _WIN32
/** Get command line arguments encoded as UTF-8.
 On windows the main( int argc, char **argv ) function receives its argv entries in local code point, and
 also I'm not sure if they are actually separated as we want and stuff.
 So this function instead modifies argv to be UTF-8 encoded variables that, as we expect (there is probably
 better ways to do this, but whatever for the moment).
 
 Note: This function just leaks the allocated argv memory
 
 Returns false on failure (which probably wont ever really happen)
 */
bool get_utf8_program_args( int &argc, char ** &argv );
#endif
}//namespace AppUtils

#endif //AppUtils_h
