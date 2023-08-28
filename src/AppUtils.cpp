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

#include <mutex>
#include <string>
#include <vector>
#include <fstream>
#include <signal.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#elif( defined(_WIN32) )
#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <windows.h>
#include <libloaderapi.h>
#endif


#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include <Wt/WServer.h>
#include <Wt/WLogger.h>

#include "SpecUtils/SerialToDetectorModel.h"

#include "SpecUtils/Filesystem.h"
#include "SpecUtils/StringAlgo.h"

#include "FullSpectrumId/Analysis.h"
#include "FullSpectrumId/AppUtils.h"
#include "FullSpectrumId/RestResources.h"
#include "FullSpectrumId/FullSpectrumApp.h"


using namespace std;
using namespace Wt;


namespace
{

std::mutex ns_optionsmutex;
bool ns_enable_rest_api = false;


/* A Mutex to protect the rest of the variables in this namespace.
 
 Not that the mutex protects ns_server, sm_rest_info, and sm_rest_ana variables, but DOES NOT
 protect what the variables point at.
 */
std::mutex ns_servermutex;

int sm_port_served_on = -1;
std::string sm_url_served_on = "";


/* Making server a shared_ptr so #wait_for_server_to_finish doesnt need to have #ns_servermutex
 locked while waiting for server to finish.
*/
std::shared_ptr<WServer> ns_server;
std::unique_ptr<RestResources::InfoResource> ns_rest_info;
std::unique_ptr<RestResources::AnalysisResource> ns_rest_ana;


}// namespace


namespace AppUtils
{

tuple<AppUseMode,vector<string>> init_app_config( const int argc, char **argv )
{
  namespace po = boost::program_options;
  
  
  vector<string> args_for_app;
  
#if( FOR_WEB_DEPLOYMENT )
  string config_filename = "config/app_config_web.ini";
#else
  string config_filename = "config/app_config_local.ini";
#endif
  
  // Declare options that will be allowed only on command line
  string fore_path, back_path, drf;
  po::options_description cmdline_only_options( "Command line only options" );
  cmdline_only_options.add_options()
  ("appconfig",
   po::value<string>(&config_filename)->default_value(config_filename),
   "Name of app config file - note that this is separate from the Wt config file" )
  ( "version,v", "Print executable version and exit" )
  ( "help,h", "produce help message" )
  ;
  
  
  po::options_description cmdline_ana_options( "Command line analysis mode options" );
  cmdline_ana_options.add_options()
  ( "foreground,f", po::value<string>(&fore_path), "Foreground spectrum file to analyze.")
  ( "background,b", po::value<string>(&back_path), "Background spectrum file to analyze.")
  ( "drf,d", po::value<string>(&drf), "The detector response function to use.")
  ( "out-format", po::value<string>(), "Format of command-line mode analysis output; can be 'brief', 'standard' (default if not specified), 'json'" )
  ( "drfs", "Show the available DRFs, and exit.  Can be combined with --out-format=json.")
  //TODO: add in output format shortcuts of --brief-output, --json-output --standard--output
  //TODO: add in --cl-options
  ;
  
  
  // Declare options allowed both on command line and in config file
#if( ENABLE_SESSION_DETAIL_LOGGING )
  string datadir;
  bool save_uploaded_files = false;
#endif
  
  bool enable_rest_api, command_line = false;
  string detserial, gadras_run_dir, gadras_lib_path, execution_mode;
  
  po::options_description cmdline_or_file_options("Application execution options");
  cmdline_or_file_options.add_options()
#if( ENABLE_SESSION_DETAIL_LOGGING )
  ( "DataDir", po::value<string>(&datadir),
   "Directory to save user data too." )
  ( "SaveUploadedFiles", po::value<bool>(&save_uploaded_files),
   "Whether to save uploaded files" )
#endif
  ( "DetectorSerialToModelCsv", po::value<string>(&detserial)->default_value( "config/OUO_detective_serial_to_model.csv" ),
   "File of detective_serial_to_model.csv - for ORTEC Detective model identification" )
  ( "GadrasRunDirectory", po::value<string>(&gadras_run_dir)->default_value( "gadras_isotope_id_run_directory" ),
   "GADRAS app directory; contains necessary GADRAS files, and also a \"drfs\" directory with the detector response functions" )
#if( !STATICALLY_LINK_TO_GADRAS )
  ( "GadrasLibPath", po::value<string>(&gadras_lib_path),
   "Path to the GADRAS shared library to load." )
#endif
  ( "EnableRestApi", po::value<bool>(&enable_rest_api)->default_value(false),
   "Enable rest API for analysis (e.g., POST'ing to /api/v1/analysis)" )
#if( FOR_WEB_DEPLOYMENT )
  ( "mode", po::value<string>(&execution_mode)->default_value("web-server"),
    "Execution mode, can be 'command-line' (or equivalently 'cl'), 'web-server' (or equivalently 'web' or 'server')" )
#else
  ( "mode,m", po::value<string>(&execution_mode)->default_value("command-line"),
   "Execution mode, can be 'command-line' (or equivalently 'cl'), 'web-server' (or equivalently 'web' or 'server')" )
#endif
  ( "command-line", "Equivalent of specifying --mode=command-line" )
  ( "cl", "Equivalent of specifying --mode=command-line" )
  ( "web-server", "Equivalent of specifying --mode=web-server" )
  ( "server", "Equivalent of specifying --mode=web-server" )
  ( "web", "Equivalent of specifying --mode=web-server" )
  ;
  
  // Now define values you can supply on the command line or in the appconfig file to pass onto
  //  Wt.
  const vector<string> hidden_args = { "config", "docroot", "accesslog", "http-listen",
    "http-address", "http-port", "threads", "servername", "resources-dir", "approot", "errroot",
    "no-compression", "deploy-path", "session-id-prefix", "pid-file", "max-memory-request-size",
    "gdb"
    // could add a bunch more ssl options
  };
  
  po::options_description hidden_options("Wt Options");
  hidden_options.add_options()
#if( FOR_WEB_DEPLOYMENT )
  ("config,c",po::value<string>()->default_value("/var/opt/app_ubuntu_16/config/wt_4.5.0_config_web.xml"))
  ("docroot",po::value<string>()->default_value("/var/opt/app_ubuntu_16/web_assets/"))
  ("accesslog",po::value<string>()->default_value("/mnt/logs/wt_access_log.txt") )
#else
  ("config,c",po::value<string>()->default_value("config/wt_config_local_dev.xml"))
  ("docroot",po::value<string>()->default_value("web_assets"))
  ("accesslog",po::value<string>()->default_value("-"))
#endif
  ("http-listen",po::value<string>())
  ("http-address",po::value<string>())
  ("http-port",po::value<string>())
  ("threads,t",po::value<string>())
  ("servername",po::value<string>())
  ("resources-dir",po::value<string>())
  ("approot",po::value<string>())
  ("errroot",po::value<string>())
  ("no-compression",po::value<string>()->default_value("1"))
  ("deploy-path",po::value<string>())
  ("session-id-prefix",po::value<string>())
  ("pid-file,p",po::value<string>())
  ("max-memory-request-size",po::value<string>())
  ("gdb",po::value<string>())
#if( !ENABLE_SESSION_DETAIL_LOGGING )
  ("DataDir",po::value<string>())
  ("SaveUploadedFiles",po::value<string>())
#endif
  ;
  
  
  // A development test to make sure 'hidden_options' and 'hidden_args' are in sync
  //  (at least in one direction)
  for( const auto &arg : hidden_args )
  {
    assert( hidden_options.find_nothrow(arg, true) );
  }
  
  
  po::variables_map cl_vm;
  
  
  try
  {
    auto cmdline_options = cmdline_only_options;
    cmdline_options.add( cmdline_ana_options );
    
    
    po::parsed_options parsed_opts
    = po::command_line_parser(argc,argv)
    .allow_unregistered()
    .options(cmdline_options)
    .run();
    
    /*
     for( const auto &opt : parsed_opts.options )
     {
     if( opt.unregistered )
     {
     if( (std::find( begin(hidden_args), end(hidden_args), opt.string_key ) != end(hidden_args))
     || SpecUtils::istarts_with(opt.string_key, "ssl") )
     continue;
     
     cerr << "Warning, command line argument '" << opt.string_key << "' with ";
     if( opt.value.empty() )
     cerr << "no value";
     else if( opt.value.size() <= 1 )
     cerr << "value=";
     else
     cerr << "values={";
     for( size_t i = 0; i < opt.value.size(); ++i )
     cerr << (i?", ":"") << "'" << opt.value[i] << "'";
     cerr << (opt.value.size() > 1 ? "}" : "") << " was not recognized as a valid argument - ignoring" << endl;
     }
     }//for( const auto &opt : parsed_opts.options )
     */
    
    po::store( parsed_opts, cl_vm );
    po::notify( cl_vm );
    
    // TODO: remove this next check if --help is specified, and patch things over, of course
    if( config_filename.empty() || !locate_file(config_filename, false, argc, argv) )
      throw runtime_error( "App config file specified ('" + config_filename + "') does not exist" );
  }catch( std::exception &e )
  {
    cerr << "Error parsing command line arguments: " << string(e.what()) << endl;
    exit( EXIT_FAILURE );
  }//try catch
  
  
  if( cl_vm.count("version") )
  {
    cout << "FullSpectrumID: Lee Harding and Will Johnson, Sandia National Laboratories."
    << " Build date " << __DATE__ << endl;
    exit( EXIT_SUCCESS );
  }//if( show_version )
  
  
  po::variables_map config_vm, cl_only_config_vm;
  
  try
  {
    ifstream input( config_filename.c_str() );
    if( !input )
      throw runtime_error( "Could not open app config file '" + config_filename + "'" );
    
    //if( mode == AppUseMode::Server )
    cmdline_or_file_options.add( hidden_options );
    
    // Parse config file
    const bool allow_unregistered = false; //(mode == AppUseMode::CommandLine);
    const po::parsed_options config_file_parsed_opts = po::parse_config_file(input, cmdline_or_file_options, allow_unregistered );
    
    // Parse command line
    const po::parsed_options cl_parsed_opts = po::command_line_parser(argc,argv)
    .allow_unregistered()
    .options(cmdline_or_file_options)
    .run();
    
    // Store command line arguments first so they take priority
    po::store( cl_parsed_opts, config_vm );
    po::store( cl_parsed_opts, cl_only_config_vm );  //does not change non-defaulted options already set
    po::store( config_file_parsed_opts, config_vm ); //does not change non-defaulted options already set
    
    // We need to run notify on `cl_only_config_vm` first, because when we run notify the second
    //  time it will put values back to deafaulted in they arent specified in the variable-map,
    //  and `config_vm` is the superset of `cl_only_config_vm`.
    po::notify( cl_only_config_vm );
    po::notify( config_vm );
    args_for_app = po::collect_unrecognized( cl_parsed_opts.options, po::collect_unrecognized_mode::include_positional );
  }catch( std::exception &e )
  {
    cerr << "Error parsing options: " << e.what() << endl;
    exit( EXIT_FAILURE );
  }//try / catch
  
  
  // Begin deciding if we are being ran in server mode, or command line mode
  //  The logic is a bit tortured since the user can specify either
  //  --mode={command-line|cl|web-server|server|web},
  //  or --{command-line|cl|web-server|server|web}
  const string possible_cl_txt[] = { "command-line", "cl" };
  const string possible_server_txt[] = { "web-server", "web", "server" };
  
  bool cl_mode = std::count( begin(possible_cl_txt), end(possible_cl_txt), execution_mode );
  bool server_mode = std::count( begin(possible_server_txt), end(possible_server_txt), execution_mode );
  string mode_shortcut;
  bool cl_shortcut = false, server_shortcut = false;
  for( const auto &s : possible_cl_txt )
  {
    if( config_vm.count(s) )
    {
      mode_shortcut = ("--" + s) + (mode_shortcut.size() ? " " : "") + mode_shortcut;
      cl_shortcut = true;
    }
  }
  
  for( const auto &s : possible_server_txt )
  {
    if( config_vm.count(s) )
    {
      mode_shortcut = ("--" + s) + (mode_shortcut.size() ? " " : "") + mode_shortcut;
      server_shortcut = true;
    }
  }
  
  if( cl_shortcut && server_shortcut )
  {
    cerr << "You cannot specify to use both command line and web-server mode (error in specifying '"
         << mode_shortcut << "')" << endl;
    exit( EXIT_FAILURE );
  }
  
  
  if( cl_only_config_vm.count("mode") && !cl_only_config_vm["mode"].defaulted() )
  {
    if( !cl_mode && !server_mode )
    {
      cerr << "Invalid 'mode' argument specified ('" << execution_mode << "'); must be one of:\n\t";
      for( const auto &i : possible_cl_txt  )
        cerr << i << ", ";
      for( const auto &i : possible_server_txt  )
        cerr << i << ", ";
      cerr << endl;
      exit( EXIT_FAILURE );
    }//if( an invalid mode was specified )
    
    if( (cl_mode && server_shortcut) || (server_mode && cl_shortcut) )
    {
      cerr << "Option 'mode' was specified as '" << execution_mode << "', but '" << mode_shortcut
          << "' was also specified" << endl;
      exit( EXIT_FAILURE );
    }
  }else if( cl_shortcut || server_shortcut )
  {
    cl_mode = cl_shortcut;
    server_mode = server_shortcut;
  }//if( config_vm.count("mode") )
  

  if( cl_mode == server_mode )
  {
    cerr << "You may specify '--mode' (or equiv '-m') to only be one of: 'command-line', 'cl',"
    << " 'web-server', 'web', 'server'.  You specified '" << execution_mode << "'" << endl;
    exit( EXIT_FAILURE );
  }//if( cl_mode == server )
  // End deciding if we are being ran in server mode, or command line mode
  
  if( cl_vm.count("help") || (cl_mode && (argc <= 1)) )
  {
    const char * const this_mode = (cl_mode ? "command-line" : "web-server");
    const char * const other_mode = (cl_mode ? "web-server" : "command-line");
    
    cout << "FullSpectrumID: Lee Harding and Will Johnson, Sandia National Laboratories.\n"
         << "\tBuild date " << __DATE__ << endl
         << "\tExecutable licensed under the LGPL v2.1 open-source license, see:\n"
         << "\t\tthttps://www.gnu.org/licenses/old-licenses/lgpl-2.1.en.html"
         << "\tThe GADRAS analysis library is separately licensed and distributed."
         << endl;
      
    cout << "\n" << endl;
    cout << "You can specify options either on the command line, or via an 'appconfig' INI file and"
         << " if specified both places, the command line will take precedent.\n"
         << endl;
    cout << "This program can be executed either as a web-server, or to analyze spectra from the"
         << " command-line; the following options are applicable to the " << this_mode
         << " execution mode; to see options for the " << other_mode << " mode, specify "
         << "'--mode=" << other_mode << "' on the command line."
         << endl;
    
    if( cl_mode )
    {
      cmdline_only_options.print(cout);
      cmdline_ana_options.print(cout);
     
      cout << "If only a single file is being specified, or the names of the file make it"
           << " unambiguous (ex., foreground.n42, ioi.n42, interest.pcf, background.spe, etc),"
           << " or the countrate of the foreground is 25% larger than background, then"
           << " the '--foreground' and/or '--background' indicators can be omitted." << endl;
      
      if( !locate_file(gadras_run_dir, true, argc, argv) )
      {
        cout << "The GADRAS run directory '" << gadras_run_dir << "' could not be located,"
             << " so can-not list available DRFs." << endl;
      }else
      {
        Analysis::set_gadras_app_dir( gadras_run_dir );
      
        cout << endl << "Available DRFs: ";
        const vector<string> drfs = Analysis::available_drfs();
        for( size_t i = 0; i < drfs.size(); ++i )
          cout << (i ? ", " : "") << "'" << drfs[i] << "'";
        cout << endl;
      }
      
      const string exe_name = ((argc >= 1) && argv) ? SpecUtils::filename( argv[0] )
                                                    : string("FullSpectrumId");
      
      cout << "\n\nExample uses:\n"
           << "\t" << exe_name << " foreground.n42 background.spc" << endl
           << "\t" << exe_name << " --foreground file1.n42 --background=background.spc" << endl
           << "\t" << exe_name << " -f file1.n42 -b background.pcf" << endl
           << "\t" << exe_name << " foreground_and_background.n42" << endl
           << "\t" << exe_name << " portal_or_search_data.n42" << endl
           << "\t" << exe_name << " --out-format=json portal_or_search_data.n42" << endl
           << "\t" << exe_name << " --drf Detective-X file.n42" << endl
           << "\t" << exe_name << " --mode=command-line --drf Detective-X foreground.n42 background.n42" << endl
           << endl;
    }else
    {
      cmdline_only_options.print(cout);
      cmdline_or_file_options.print(cout);
    }
    
    //return make_tuple(AppUseMode::Server, vector<string>{"--help"});
    
    exit( EXIT_SUCCESS );
  }//if( show_version )
  
  
  
  
  if( server_mode
     && (!fore_path.empty() || !back_path.empty() || !drf.empty()
          || config_vm.count("out-format") || config_vm.count("drfs") ) )
  {
    cerr << "You can not specify 'foreground', 'background', 'drf', 'drfs', or 'out-format' when"
         << " execution mode is 'web-server'" << endl;
    exit( EXIT_FAILURE );
  }//if( server mode, but specified a command line options )
  
  
  if( cl_mode )
  {
    // For command line use, dont print out info or debug messages, but do printout errors.
    //
    // Note: using something like Wt::log("error:app") in Analysis.cpp, and then configuring
    //       "error:app" here does not work (we dont see anything to stdout); this is likely
    //       a Wt bug, but I havent investigated.
    Wt::WLogger &wtlog = Wt::logInstance();
    wtlog.configure( "error" );  //"configuring "error:app"
    
    // args_for_app should have everything in it
    //if( !fore_path.empty() )
    //  args_for_app.push_back( "--foreground='" + fore_path + "'" );
    //if( !back_path.empty() )
    //  args_for_app.push_back( "--background='" + back_path + "'" );
    //if( !drf.empty() )
    //  args_for_app.push_back( "--drf='" + drf + "'" );
    //if( config_vm.count("out-format") )
    //  args_for_app.push_back( "--out-format='" + config_vm["out-format"].as<string>() + "'" );
    
    
    for( const auto &arg : hidden_args )
    {
      if( cl_vm.count(arg) )
      {
        cerr << "Argument '" << arg << "' was specified, which is a server-mode argument, and"
        << " can not be specified along with the 'foreground' option or a trailing file path"
        << " not matched to another option. (e.g., you cant mix arguments for starting a"
        << " server with arguments to do analysis from command line)" << endl;
        exit( EXIT_FAILURE );
      }//if( cl_vm.count(arg) )
    }//for( const auto &arg : hidden_args )
  }//if( cl_mode )
  

  if( server_mode )
  {
    for( const string &arg : hidden_args )
    {
      if( config_vm.count(arg) )
      {
        string value = config_vm[arg].as<string>();
        //cout << "Will propagate '" + arg + "' with value '" + value + "' to Wt" << endl;
        
#if( !FOR_WEB_DEPLOYMENT )
        // If when we arent compiling for the server, lets be a little more forgiving about how
        //  the Wt config file and docroot are specified.
        //  We wont do this if compiled for web development mode, just to be sure
        if( (arg == "config") && !locate_file(value, false, argc, argv) )
          throw runtime_error( "couldnt find config file '" + arg + "'" );
        
        if( (arg == "docroot") && !locate_file(value, true, argc, argv) )
          throw runtime_error( "couldnt find docroot directory '" + arg + "'" );
#endif

        args_for_app.push_back( "--" + arg );
        args_for_app.push_back( value );
      }//if( config_vm.count(arg) )
    }//for( const string &arg : hidden_args )
    
    
    if( !config_vm.count("http-listen") )
    {
      if( !config_vm.count("http-address") )
      {
        args_for_app.push_back( "--http-address" );
#if( FOR_WEB_DEPLOYMENT )
        args_for_app.push_back( "0.0.0.0" );
#else
        args_for_app.push_back( "127.0.0.1" );
#endif
      }
      
      if( !config_vm.count("http-port") )
      {
        args_for_app.push_back( "--http-port" );
#if( FOR_WEB_DEPLOYMENT )
        args_for_app.push_back( "8085" );
#else
        args_for_app.push_back( "8082" );
#endif
      }
    }//if( !config_vm.count("http-listen") )
  }//if( server_mode )
  
  
  // A debug printout
  //for( const auto &option_pair : config_vm )
  //{
  //  if( !option_pair.second.defaulted() )
  //  {
  //    string val = "<other type>";
  //    try{ val = to_string(option_pair.second.as<bool>()); }catch( ... ){}
  //    try{ val = to_string(option_pair.second.as<int>()); }catch( ... ){}
  //    try{ val = option_pair.second.as<string>(); }catch( ... ){}
  //    cout << "OPTION: " << option_pair.first << ": " << val << endl;
  //  }
  //}//for( const auto &option_pair : config_vm )
  
  //for( auto a : args_for_app )
  //  cout << "Wt arg: " << a << endl;
  
#if( STATICALLY_LINK_TO_GADRAS )
  for( const auto &val : args_for_app )
  {
    if( SpecUtils::icontains(val, "GadrasLibPath") )
    {
      cerr << "This executable was statically linked to GADRAS; you can not specify 'GadrasLibPath'"
      << " on either the command line or appconfig file." << endl;
      exit( EXIT_FAILURE );
    }
  }//for( const auto &val : args_for_app )
  
#else  //STATICALLY_LINK_TO_GADRAS
  
#ifdef _WIN32
  if( gadras_lib_path.empty() )
    gadras_lib_path = "libgadrasiid.dll";
#elif( defined(__APPLE__) )
  // macOS 10.15 and needs full path to library
  if( gadras_lib_path.empty() )
    gadras_lib_path = "libgadrasiid.dylib";
#else
  // Linux
  if( gadras_lib_path.empty() )
    gadras_lib_path = "libgadrasiid.so";
#endif

  if( locate_file(gadras_lib_path, false, argc, argv) )
  {
#ifndef _WIN32
    // Apple requires full, absolute path to libraries when you load them
    // and actually for linux if we just specify the filename because its in the CWD, it wont
    // load because the CWD may not be in LD_LIBRARY_PATH.
    // And to avoid accidentally loading some random library, we'll just always get the absolute
    // path and use it.
    try
    {
      gadras_lib_path = boost::filesystem::absolute(gadras_lib_path).string<string>();
    }catch( std::exception &e )
    {
      cerr << "Fatal: could not make path '" << gadras_lib_path << "' into an absolute path."
           << endl;
      exit( EXIT_FAILURE );
    }
#endif //_WIN32
  }//if( locate_file(gadras_lib_path, false, argc, argv) )
  
  
  if( !Analysis::load_gadras_lib( gadras_lib_path ) )
  {
    cerr << "Fatal: couldn't load '" << gadras_lib_path << "'" << endl;
    exit( EXIT_FAILURE );
  }
#endif  //if( STATICALLY_LINK_TO_GADRAS ) / else
  
  
  if( !locate_file(gadras_run_dir, true, argc, argv) )
  {
    cerr << "The GADRAS run directory '" << gadras_run_dir << "' could not be located." << endl;
    exit( EXIT_FAILURE );
  }
  
  Analysis::set_gadras_app_dir( gadras_run_dir );
  Wt::log("debug:app") << "Using GADRAS app directory '" << gadras_run_dir << "'";
  
  if( server_mode )
  {
#if( ENABLE_SESSION_DETAIL_LOGGING )
    if( save_uploaded_files && datadir.empty() )
    {
      cerr << "Saving of uploaded files was specified, but no data directory given." << endl;
      exit( EXIT_FAILURE );
    }
    
    if( !datadir.empty() )
    {
      if( !locate_file(datadir, true, argc, argv) )
      {
        cerr << "Data directory ('" << datadir << "') is invalid." << endl;
        exit( EXIT_FAILURE );
      }
      
      FullSpectrumApp::set_data_directory( datadir, save_uploaded_files );
      
      Wt::log("debug:app") << "Will save user-uploaded files in base-directory '" << datadir << "'";
    }else
    {
      Wt::log("debug:app") << "Will not save user-uploaded files";
    }//if( save_uploaded_files )
    
#else //ENABLE_SESSION_DETAIL_LOGGING
    
    if( config_vm.count("SaveUploadedFiles")
       && config_vm["SaveUploadedFiles"].as<bool>() )
    {
      cerr << "This executable was not compiled with support for saving user uploaded files;"
      " either re-compile with the CMake 'ENABLE_SESSION_DETAIL_LOGGING' option set to 'ON', or "
      " set the 'SaveUploadedFiles' runtime option (e.g., command line or config file) to"
      " false."
      << endl;
      exit( EXIT_FAILURE );
    }//if( mode == AppUseMode::Server )
#endif //if( ENABLE_SESSION_DETAIL_LOGGING ) / else
  }//if( mode == AppUseMode::Server )
  
  // Try to load the detector to serial number mapping, but just print a warning if it fails.
  if( locate_file(detserial, false, argc, argv) )
    SerialToDetectorModel::set_detector_model_input_csv( detserial );
  else if( detserial.empty() )
    Wt::log("debug:app") << "Will not load a detective serial to model mapping file.";
  else
    Wt::log("error:app") << "Could not load detective serial to model mapping file '" << detserial << "'";
  
  if( server_mode )
  {
    std::lock_guard<std::mutex> lock( ns_optionsmutex );
    ns_enable_rest_api = enable_rest_api;
  }
  
  const AppUseMode mode = server_mode ? AppUseMode::Server : AppUseMode::CommandLine;
  
  return make_tuple( mode, args_for_app );
}//init_app_config(...)


/** Starts the web-server.
 
 Will throw exception on error.
 */
void start_server( const std::string &applicationPath, const std::vector<std::string> &args )
{
  // Taken from wt-4.4.0/src/http/WServer.C
  //  Re-implemented here to allow a more generalized configuration and running setup.
  
  // Note: that if we use isapi or fcgi connectors, this code is not correct, and shoudl use
  //  Wt::WRun(...)
  
  bool enable_rest_api = false;
  {// begin lock on ns_optionsmutex
    std::lock_guard<std::mutex> lock( ns_optionsmutex );
    enable_rest_api = ns_enable_rest_api;
  }// end lock on ns_optionsmutex
  
  
  { // begin lock on ns_servermutex
    std::lock_guard<std::mutex> lock( ns_servermutex );
    
  
    if( ns_server )
      throw runtime_error( "start_server: server already started!" );
  
    try
    {
      ns_server = make_shared<WServer>( applicationPath, "" );
    }catch( Wt::WServer::Exception &e )
    {
      cerr << "\nfatal, WServer::Exception: " << e.what() << endl;
      ns_server.reset();
      throw runtime_error( "fatal, WServer::Exception setting up server: " + string(e.what()) );
    }catch( std::exception &e )
    {
      cerr << "\nfatal, std::exception: " << e.what() << endl;
      ns_server.reset();
      throw runtime_error( "fatal, std::exception setting up server: " + string(e.what()) );
    }// try / catch / catch create server
    
    try
    {
      if( enable_rest_api )
      {
        ns_rest_info = make_unique<RestResources::InfoResource>();
        ns_rest_ana = make_unique<RestResources::AnalysisResource>();
      }//if( enable_rest_api )
    }catch( std::exception &e )
    {
      cerr << "\nfatal, std::exception setting up REST resources: " << e.what() << endl;
      
      //ns_server->stop(); //we havent started the server yet, so I dont think we need to stop it
      ns_server.reset();
      ns_rest_info.reset();
      ns_rest_ana.reset();
      
      throw runtime_error( "fatal, std::exception setting up REST resources: " + string(e.what()) );
    }// try / catch setup REST resources
    
    
    try
    {
      //Normally in Wt's code 'serverConfigFile' below uses WTHTTP_CONFIGURATION, but this usually
      //  points to junk, and instead we get everything from config/wt_config_local_dev.xml, so
      //  we'll just set to empty to not use this.
      const string serverConfigFile = "";
      
      ns_server->setServerConfiguration( applicationPath, args, serverConfigFile );
      ns_server->addEntryPoint( EntryPointType::Application, [](const Wt::WEnvironment& env) {
        return std::make_unique<FullSpectrumApp>(env);
      } );
      
      assert( enable_rest_api == !!ns_rest_info );
      if( enable_rest_api && ns_rest_info )
        ns_server->addResource( ns_rest_info.get(), "api/v1/info" ); // TODO: change this to, or split into two from "api/v1/options"
      
      assert( enable_rest_api == !!ns_rest_ana );
      if( enable_rest_api && ns_rest_ana )
        ns_server->addResource( ns_rest_ana.get(), "api/v1/analysis" );
      
      
      // TODO: maybe add privacy, license, and use instructions information to static REST API endpoints
      
      if( !ns_server->start() )
        throw runtime_error( "Server failed to start." );
      
      sm_port_served_on = ns_server->httpPort();
      
      // TODO: Figure out actual http-address we are listening on; may be specified via
      //       "http-listen" as well.  Note call to
      //       ns_server->readConfigurationProperty("http-address", val ) // "http-listen"
      //       fails
      sm_url_served_on = "http://127.0.0.1:" + std::to_string(sm_port_served_on);
      
      
      cout << "\nPlease point your browser to " << sm_url_served_on << endl;
        
      bool isLocalOnly = false;
      for( const string &arg : args )
      {
        isLocalOnly = SpecUtils::icontains(arg, "127.0.0.1");
        if( isLocalOnly )
          break;
      }
        
      if( isLocalOnly )
        cout << "\t(only accessible on your computer)" << endl << endl;
      else
        cout << "\t(may be accessible on other computers on your network - be careful)" << endl << endl;
      
      //int sig = WServer::waitForShutdown();
      //server.stop();
//#ifndef WT_WIN32
//        if (sig == SIGHUP)
//          // Mac OSX: _NSGetEnviron()
//          WServer::restart(applicationPath, args);
//#endif // WIN32
    }catch( Wt::WServer::Exception &e )
    {
      cerr << "\nfatal, WServer::Exception: " << e.what() << endl;
      
      ns_server.reset();
      ns_rest_info.reset();
      ns_rest_ana.reset();
      sm_port_served_on = -1;
      sm_url_served_on = "";
      
      throw runtime_error( "fatal, WServer::Exception while starting server: " + string(e.what()) );
    }catch( std::exception &e )
    {
      cerr << "fatal std::exception while running: " << e.what() << endl;
      ns_server->log("info") << "fatal std::exception while running: " << e.what();
      
      ns_server.reset();
      ns_rest_info.reset();
      ns_rest_ana.reset();
      sm_port_served_on = -1;
      sm_url_served_on = "";
      
      throw runtime_error( "fatal std::exception while starting server: " + string(e.what()) );
    }// try / catch / catch (configure and start server )
  
    assert( ns_server->isRunning() );
    if( !ns_server->isRunning() )
    {
      ns_server.reset();
      ns_rest_info.reset();
      ns_rest_ana.reset();
      sm_port_served_on = -1;
      sm_url_served_on = "";
      
      throw runtime_error( "Somehow server is not running at the end of start_server(...)" );
    }
  }// end lock on ns_servermutex
}//void start_server(...)


void kill_server()
{
  {// begin lock on ns_servermutex
    std::lock_guard<std::mutex> lock( ns_servermutex );
    
    if( !ns_server )
      return;
    
    std::cerr << "About to stop server" << std::endl;
    ns_server->stop();
    
    ns_server.reset();
    ns_rest_info.reset();
    ns_rest_ana.reset();
    sm_port_served_on = -1;
    sm_url_served_on = "";
    
    std::cerr << "Stopped and killed server" << std::endl;
  }// end lock on ns_servermutex
}//void kill_server()



bool is_server_running()
{
  std::lock_guard<std::mutex> lock( ns_servermutex );
  
  if( !ns_server )
    return false;
  
  assert( ns_server->isRunning() );
  
  return ns_server->isRunning();
}//is_server_running()



int wait_for_server_to_finish()
{
  const int sig = WServer::waitForShutdown();
  
  cerr << "WServer shutdown (signal = " << sig << ")" << endl;
  
  {
    std::lock_guard<std::mutex> lock( ns_servermutex );
    if( ns_server )
      ns_server->log("info") << ": shutdown (signal = " << sig << ")";
  }
  
  kill_server();
  
  return sig;
}//wait_for_server_to_finish()



int port_being_served_on()
{
  
  std::lock_guard<std::mutex> lock( ns_servermutex );
  
  if( sm_port_served_on < 0 )
    throw runtime_error( "port_being_served_on(): Not currently being served." );
  
  return sm_port_served_on;
}//int port_being_served_on()



std::string url_being_served_on()
{
  std::lock_guard<std::mutex> lock( ns_servermutex );
  
  if( sm_port_served_on < 0 )
    throw runtime_error( "port_being_served_on(): Not currently being served." );
  
  return sm_url_served_on;
}//std::string url_being_served_on()



bool locate_file( string &filename, const bool is_dir, const int argc, char **argv )
{
  auto check_exists = [is_dir]( const string &name ) -> bool {
    return is_dir ? SpecUtils::is_directory(name) : SpecUtils::is_file(name);
  };//auto check_exists
  
  if( SpecUtils::is_absolute_path(filename) )
    return check_exists(filename);
  
  if( check_exists(filename) )
    return true;
  
  if( argc <= 0 )
    return false;
  
  
  const string exe_path = SpecUtils::parent_path( argv[0] );
  if( !exe_path.empty() && (exe_path != ".") && SpecUtils::is_directory(exe_path) )
  {
    const string trialpath = SpecUtils::append_path( exe_path, filename );
    //trialpath = SpecUtils::lexically_normalize_path(trialpath);
    
    if( check_exists(trialpath) )
    {
      // I'm not convinced there arent some file path limitations in GADRAS, or even in SpecUtils
      //  file path functions, so we'll use the shorter of either the absolute or relative path.
      const string relpath = SpecUtils::fs_relative( SpecUtils::get_working_path(), trialpath );
      
      filename = ((trialpath.length() <= relpath.length()) ? trialpath : relpath);
      return true;
    }
  }//if( EXE is anywhere else )
  
  // Maybe we symlinked the executables somewhere, so lets look relative to the original EXEs
  //  location
  try
  {
#ifdef __APPLE__
    char path_buffer[PATH_MAX + 1] = { '\0' };
    uint32_t size = PATH_MAX + 1;
    
    if (_NSGetExecutablePath(path_buffer, &size) != 0) {
      return false;
    }
    
    path_buffer[PATH_MAX] = '\0'; // JIC
    const string exe_path = path_buffer;
#elif( defined(_WIN32) )
    //static_assert( 0, "Need to test this EXE path stuff on Windows..." );
    wchar_t wbuffer[2*MAX_PATH];
    const DWORD len = GetModuleFileNameW( NULL, wbuffer, 2*MAX_PATH );
    if( len <= 0 )
      throw runtime_error( "Call to GetModuleFileName falied" );
    
    const string exe_path = SpecUtils::convert_from_utf16_to_utf8( wbuffer );
#else // if __APPLE__ / Win32 / else
    
    char path_buffer[PATH_MAX + 1] = { '\0' };
    const ssize_t ret = readlink("/proc/self/exe", path_buffer, PATH_MAX);
    
    if( (ret == -1) || (ret > PATH_MAX) )
      throw runtime_error( "Failed to read line" );
    
    assert( ret < PATH_MAX );
    path_buffer[ret] = '\0';
    path_buffer[PATH_MAX] = '\0'; // JIC
    const string exe_path = path_buffer;
#endif // else not __APPLE__
    
    string trial_parent_path = exe_path;
    if( !SpecUtils::make_canonical_path(trial_parent_path) )
      throw runtime_error( "Failed to make trial_parent_path canonical" );
    
    trial_parent_path = SpecUtils::parent_path(trial_parent_path);
    
    string trialpath = SpecUtils::append_path( trial_parent_path, filename );
    
    if( check_exists(trialpath) )
    {
      filename = trialpath;
      return true;
    }
    
    if( boost::filesystem::is_symlink(trialpath) )
    {
      trialpath = boost::filesystem::read_symlink(trialpath).string<string>();
      if( check_exists(trialpath) )
      {
        filename = trialpath;
        return true;
      }
    }//if( is symlink )
  }catch( std::exception &e )
  {
    //cerr << "Caught exception: " << e.what() << endl;
  }
  
  return false;
}//bool locate_file( ... )


#ifdef _WIN32
#include <shellapi.h>

#include "SpecUtils/StringAlgo.h"

/** Get command line arguments encoded as UTF-8.
 This function just leaks the memory
 
 Note that environment variables are not in UTF-8, we could account for this
 similar to:
 wchar_t *wenvstrings = GetEnvironmentStringsW();
 ...
 */
bool get_utf8_program_args( int &argc, char ** &argv )
{
  LPWSTR *argvw = CommandLineToArgvW( GetCommandLineW(), &argc );
  if( !argvw )
  {
    std::cerr << "get_Utf8_args: CommandLineToArgvW failed - will exit" << std::endl;
    return false;
  }
  
  argv = (char **)malloc(sizeof(char *)*argc);
  
  for( int i = 0; i < argc; ++i)
  {
    //printf("Argument: %d: %ws\n", i, argvw[i]);
    
    const std::string asutf8 = SpecUtils::convert_from_utf16_to_utf8( argvw[i] );
    argv[i] = (char *)malloc( sizeof(char)*(asutf8.size()+1) );
    strcpy( argv[i], asutf8.c_str() );
  }//for( int i = 0; i < argc; ++i)
  
  // Free memory allocated for CommandLineToArgvW arguments.
  LocalFree(argvw);
  
  return true;
}//void get_utf8_program_args()
#endif
}//namespace AppUtils
