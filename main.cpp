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
#include <fstream>
#include <signal.h>

#include <Wt/WServer.h>
#include <Wt/WLogger.h>

#include "SpecUtils/Filesystem.h"
#include "SpecUtils/StringAlgo.h"

#include "FullSpectrumId/Analysis.h"
#include "FullSpectrumId/AppUtils.h"
#include "FullSpectrumId/CommandLineAna.h"


using namespace std;

int main( int argc, char **argv )
{
  int rval = EXIT_FAILURE;
  
#ifdef _WIN32
  if( !AppUtils::get_utf8_program_args( argc, argv ) )
    return rval;
#endif
  
  
  const auto app_configs = AppUtils::init_app_config( argc, argv );
  
  const AppUtils::AppUseMode use_mode = get<0>(app_configs);
  const vector<string> &command_args = get<1>(app_configs);
  
  Analysis::start_analysis_thread();
  
  switch( use_mode )
  {
    case AppUtils::AppUseMode::Server:
    {
      cout << "Will start web-server" << endl;
      
      const string appName = ((argc > 0) ? argv[0] : "FullSpectrum");
      
      try
      {
        // Note if we use isapi or fcgi connectors, then this next call is not correct, and would need
        //  to be modified
        AppUtils::start_server( appName, command_args );
      }catch( std::exception &e )
      {
        Analysis::stop_analysis_thread();
        
        cerr << "\n\nFailed to start server: " << e.what() << endl << endl;
        
        return EXIT_FAILURE;
      }//try to start server / catch
      
      rval = AppUtils::wait_for_server_to_finish();
      
#ifndef _WIN32
      // Note: in Wt::WRun(...), if it was SIGHUP that signaled the server to finish, then the server
      //  gets restarted - not sure if this is ever the case for us...
      if( rval == SIGHUP )
        cerr << "\n\nWServer stopped with rval=SIGHUP\n" << endl;
      //    WServer::restart(applicationPath, args);
#endif  //ifndef _WIN32
      
      break;
    } //case AppUtils::AppUseMode::Server:
      
      
    case AppUtils::AppUseMode::CommandLine:
    {
      rval = CommandLineAna::run_analysis( command_args );
      
      break;
    }//case AppUtils::AppUseMode::CommandLine:
  }//switch( use_mode )
      
  Analysis::stop_analysis_thread();
  
  return rval;
}//int main( int argc, char **argv )

