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
#include <condition_variable>

#include <Wt/Json/Array.h>
#include <Wt/Json/Value.h>
#include <Wt/Json/Object.h>
#include <Wt/Json/Serializer.h>

#include <boost/program_options.hpp>

#include <Wt/WLogger.h>

#include "SpecUtils/SerialToDetectorModel.h"

#include "SpecUtils/SpecFile.h"
#include "SpecUtils/Filesystem.h"
#include "SpecUtils/StringAlgo.h"

#include "FullSpectrumId/Analysis.h"
#include "FullSpectrumId/AppUtils.h"
#include "FullSpectrumId/CommandLineAna.h"
#include "FullSpectrumId/AnalysisFromFiles.h"


using namespace std;
using namespace Wt;

namespace CommandLineAna
{

int run_analysis( vector<string> args )
{
  namespace po = boost::program_options;
  
  vector<string> positional_spec_files;
  string fore_path, back_path, drf, output;
  po::options_description desc( "Command line options" );
  desc.add_options()
  ( "foreground,f", po::value<string>(&fore_path), "Foreground spectrum file to analyze; if specified will not start webserver.")
  ( "background,b", po::value<string>(&back_path), "Background spectrum file to analyze; if specified will not start webserver.")
  ( "spectrum-file", po::value<std::vector<std::string>>()->multitoken()->zero_tokens()->composing(),
   "Spectrum files...will guess first is foreground and second specified is background, but countrates..." )
  ( "drf,d", po::value<string>(&drf), "The detector response function to use.")
  ( "out-format", po::value<string>(&output)->default_value("standard"),
   "Format command-line mode analysis of output; can be 'brief', 'standard' (default if not specified), 'json'" )
  ( "drfs", "Show the available DRFs, and exit.  Can be combined with --out-format=json.")
  ( "help,h", "produce help message" )
  ;
  
  po::positional_options_description pos_desc;
  pos_desc.add("spectrum-file", 2);
  
  
  po::variables_map cl_vm;
  
  
  try
  {
    po::command_line_parser parser{ args };
    parser.options(desc).positional(pos_desc).allow_unregistered();
    po::parsed_options parsed_options = parser.run();
    
    store( parsed_options, cl_vm );
    notify( cl_vm );
  }catch( std::exception &e )
  {
    cerr << "Error parsing arguments from command line: " << string(e.what()) << endl;
    return EXIT_FAILURE;
  }//try catch
  
  if( cl_vm.count("help") )
  {
    cout << "FullSpectrumID: Lee Harding and Will Johnson, Sandia National Laboratories."
    << " Build date " << __DATE__ << endl;
    cout << "\n" << endl;
    cout << "blah blah blah - LGPL-v2.1 - blah blah blah" << endl;
    cout << "blah blah blah - If you want to see options for server mode, specify '--mode=web-server' or '--server' - blah blah blah" << endl;
    
    desc.print(cout);
    
    return EXIT_SUCCESS;
  }//if( show_version )
  
  if( !SpecUtils::iequals_ascii(output, "brief")
     && !SpecUtils::iequals_ascii(output, "standard")
     && !SpecUtils::iequals_ascii(output, "json") )
  {
    cerr << "Invalid 'out-format' specified, must be either not specified, or 'brief', 'standard', or 'json'" << endl;
    return EXIT_FAILURE;
  }
  
  
  if( cl_vm.count("drfs") )
  {
    const bool json = (output == "json");
    const vector<string> drfs = Analysis::available_drfs();
    
    if( json )
      cout << "[";
    else
      cout << "Available DRFs: ";
    
    for( size_t i = 0; i < drfs.size(); ++i )
      cout << (i ? ", " : "") << "'" << drfs[i] << "'";
    
    if( json )
      cout << "]";
    
    cout << endl;
    
    return EXIT_SUCCESS;
  }//if( cl_vm.count("drfs") )
  
  
  if( cl_vm.count("spectrum-file") )
    positional_spec_files = cl_vm["spectrum-file"].as<vector<string>>();
  
  
  if( !positional_spec_files.empty() || !fore_path.empty() || !back_path.empty() || !drf.empty() )
  {
    if( !cl_vm.count("foreground") && positional_spec_files.empty() )
    {
      cerr << "No foreground spectrum file was specified" << endl;
      return EXIT_FAILURE;
    }
    
    const size_t nfiles = positional_spec_files.size() + (!fore_path.empty()) + (!back_path.empty());
    
    if( nfiles == 0 )
    {
      cerr << "No input spectrum files specified on the command line." << endl;
      return EXIT_FAILURE;
    }
    
    if( nfiles > 2 )
    {
      cerr << "You can only specify a maximum of two spectrum files on the command line." << endl;
      return EXIT_FAILURE;
    }
    
    if( !fore_path.empty() && !SpecUtils::is_file(fore_path) )
    {
      cerr << "Foreground '" << fore_path << "' doesnt look to be a file." << endl;
      return EXIT_FAILURE;
    }
    
    if( !back_path.empty() && !SpecUtils::is_file(back_path) )
    {
      cerr << "Background '" << back_path << "' doesnt look to be a file." << endl;
      return EXIT_FAILURE;
    }
    
    for( const string filename : positional_spec_files )
    {
      if( !SpecUtils::is_file(filename) )
      {
        cerr << "File specified, '" << filename << "', doesnt look to be a file." << endl;
        return EXIT_FAILURE;
      }
    }//for( const string filename : positional_spec_files )
  }//if( cl_vm.count("foreground") || cl_vm.count("background") )
    
  
  
  tuple<AnalysisFromFiles::SpecClassType,string,string> input1;
  boost::optional<tuple<AnalysisFromFiles::SpecClassType,string,string>> input2;
  
  if( !fore_path.empty() )
  {
    input1 = make_tuple( AnalysisFromFiles::SpecClassType::Foreground, fore_path, fore_path );
    
    if( !back_path.empty() )
    {
      assert( positional_spec_files.empty() );
      input2 = make_tuple( AnalysisFromFiles::SpecClassType::Background, back_path, back_path );
    }else if( !positional_spec_files.empty() )
    {
      assert( positional_spec_files.size() == 1 );
      const string &name = positional_spec_files.front();
      input2 = make_tuple( AnalysisFromFiles::SpecClassType::Background, name, name );
    }
  }else
  {
    if( !back_path.empty() )
    {
      if( positional_spec_files.empty() )
      {
        cerr << "No foreground spectrum file specified." << endl;
        return EXIT_FAILURE;
      }
      
      const string &name = positional_spec_files.front();
      input1 = make_tuple( AnalysisFromFiles::SpecClassType::Foreground, name, name );
      input2 = make_tuple( AnalysisFromFiles::SpecClassType::Background, back_path, back_path );
    }else
    {
      if( positional_spec_files.empty() )
      {
        cerr << "No spectrum files specified on command line." << endl;
        return EXIT_FAILURE;
      }
      
      string name = positional_spec_files.front();
      input1 = make_tuple( AnalysisFromFiles::SpecClassType::SuspectForeground, name, name );
      if( positional_spec_files.size() > 1 )
      {
        assert( positional_spec_files.size() == 2 );
        name = positional_spec_files[1];
        input2 = make_tuple( AnalysisFromFiles::SpecClassType::SuspectBackground, name, name );
      }
    }// if( !back_path.empty() ) / else
  }//if( !fore_path.empty() ) / else
  
  
  shared_ptr<SpecUtils::SpecFile> inputspec;
  
  try
  {
    inputspec = AnalysisFromFiles::create_input( input1, input2 );
  }catch( std::exception &e )
  {
    if( SpecUtils::iequals_ascii(output, "json") )
    {
      Json::Object returnjson;
      returnjson["code"] = 3;
      returnjson["message"] = WString::fromUTF8(e.what());
      
      cout << Json::serialize(returnjson) << endl;
    }else
    {
      cerr << "Error formatting input to analysis: " << e.what() << endl;
    }
    
    return EXIT_FAILURE;
  }//try / catch to create input spectrum file
  
  assert( inputspec );
  
  
  
  if( drf.empty() || SpecUtils::iequals_ascii(drf,"auto") )
  {
    drf = Analysis::get_drf_name( inputspec );
    
    if( drf.empty() )
    {
      cerr << "Could not determine detection system type from the spectrum files - please specify"
           << " the detector response function to use via the 'drf' option." << endl;
      return EXIT_FAILURE;
    }
  }else
  {
    bool found = false;
    const vector<string> ana_drfs = Analysis::available_drfs();
    for( const string &d : ana_drfs )
    {
      if( SpecUtils::iequals_ascii(drf,d) )
      {
        drf = d;
        found = true;
        break;
      }
    }//for( const string &d : ana_drfs )
    
    if( !found )
    {
      cerr << "DRF '" << drf << "' is not valid; valid drfs are\n\t";
      for( size_t i = 0; i < ana_drfs.size(); ++i )
        cerr << (i ? ", '" : "'") << ana_drfs[i] << "'";
      cerr << endl;
      return EXIT_FAILURE;
    }//if( !found )
  }//if( !drf.empty() && !SpecUtils::iequals_ascii(drf,"auto") )
  
  
  Analysis::AnalysisInput anainput;
  anainput.ana_number = 0;//
  //anainput.wt_app_id = "";
  anainput.drf_folder = drf;
  //std::vector<std::string> anainput.input_warnings;
  
  if( inputspec->passthrough() )
  {
    const bool portal_data = AnalysisFromFiles::is_portal_data(inputspec);
    anainput.analysis_type = portal_data ? Analysis::AnalysisType::Portal : Analysis::AnalysisType::Search;
  }else
  {
    anainput.analysis_type = Analysis::AnalysisType::Simple;
  }
  anainput.input = inputspec;
  
  
  std::mutex ana_mutex;
  std::condition_variable ana_cv;
  Analysis::AnalysisOutput result;
  
  anainput.callback = [&ana_mutex,&ana_cv,&result]( Analysis::AnalysisOutput output ){
    {
      std::unique_lock<std::mutex> lock( ana_mutex );
      result = output;
    }
    ana_cv.notify_all();
  };// inputspec.callback definition
  
  {// begin lock on ana_mutex
    std::unique_lock<std::mutex> lock( ana_mutex );
    Analysis::post_analysis( anainput );
    ana_cv.wait( lock );
  }// end lock on ana_mutex
  
  
  // once we're here, the analysis should be done.
  if( SpecUtils::iequals_ascii(output, "brief") )
  {
    cout << result.briefTxtSummary() << endl;
  }else if( SpecUtils::iequals_ascii(output, "standard") )
  {
    cout << result.fullTxtSummary() << endl;
  }else if( SpecUtils::iequals_ascii(output, "json") )
  {
    cout << Json::serialize(result.toJson()) << endl;
  }else
  {
    assert( 0 );
  }
  
  
  if( (result.gadras_intialization_error < 0) || (result.gadras_analysis_error < 0) )
    return EXIT_FAILURE;
  
  return EXIT_SUCCESS;
}//int run_analysis( int argc, char **argv )

}//namespace CommandLineAna

