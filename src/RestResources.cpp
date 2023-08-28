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

#include <tuple>
#include <iostream>

#include <Wt/WLogger.h>
#include <Wt/WResource.h>
#include <Wt/Json/Array.h>
#include <Wt/Json/Value.h>
#include <Wt/Json/Object.h>
#include <Wt/Json/Parser.h>
#include <Wt/Http/Request.h>
#include <Wt/Http/Response.h>
#include <Wt/Json/Serializer.h>


#include "SpecUtils/SpecFile.h"
#include "SpecUtils/Filesystem.h"
#include "SpecUtils/StringAlgo.h"

#include "FullSpectrumId/Analysis.h"
#include "FullSpectrumId/RestResources.h"
#include "FullSpectrumId/AnalysisFromFiles.h"

using namespace std;
using namespace Wt;

namespace RestResources
{


InfoResource::InfoResource()
  : WResource(),
    m_gadras_version( Analysis::gadras_version_string() ),
    m_drfs( Analysis::available_drfs() )
{
  m_result["versions"] = Json::Object();
  
  Json::Object &versions = m_result["versions"];
  versions["analysis"] = WString::fromUTF8("GADRAS " + m_gadras_version);
  versions["ApiInterface"] = "v1";
  versions["compileDate"] = WString::fromUTF8(__DATE__);
  
  m_result["Options"] = Json::Array();
  Json::Array &options = m_result["Options"];
  
  options.push_back( Json::Object() );
  Json::Object &drf = options.back();
  drf["name"] = "drf";
  drf["comment"] = "Optional name of the Detector Response Function to use in the analysis.\n"
  "If not provided, or a value of \"auto\" is provided, the DRF to use"
  " will be guessed, and if it cant be guessed, analysis will fail.\n"
  "Value provided must be from provided list of possible values.";
  drf["type"] = "Enumerated";
  drf["required"] = false;
  drf["possibleValues"] = Json::Array();
  Json::Array &possibleDrfs = drf["possibleValues"];
  
  possibleDrfs.push_back( WString::fromUTF8("auto") );
  for( const auto &s : m_drfs )
    possibleDrfs.push_back( WString::fromUTF8(s) );
  
  m_result["comment"] = "To make an analysis request, you must POST to /v1/Analysis "
                        "Using multipart/form-data."
  "You "
  "If two files are uploaded, and the 'name' attribute of each files multipart/form-data section"
  " is anything other than 'foreground' and 'background', then it is assumed the first file is foreground, and second is background, unless the count rate of one of the files is greater than 25% more than the other one."
  "blah blah blah"
  
  "An example request for analysis might look like:\n\t"
  "curl -v -f \"options={\\\"drf\\\": \\\"IdentiFINDER-NGH\\\"}\" -F \"foreground=@./foreground.n42\" -F \"background=@./background.n42\" https://fullspectrum.sandia.gov/api/v1/analysis"
  "\nOr you can specify the DRF to use as a query parameter in the url, for example\n\t"
  "curl -v -F \"foreground=@./specfile.n42\" -F \"background=@./background.n42\" -f fullspectrum.sandia.gov/api/v1/analysis?drf=IdentiFINDER-NGH"
  ;
  // Or should we include a header?
  
  
  // TODO: add in equivalent of WApplication::instance()->maximumRequestSize();
  //  WServer::instance()->configuration().maxRequestSize();
  //  or std::string *val = WServer::instance()->readConfigurationProperty( "max-memory-request-size", const std::string& value);
  
  
}//InfoResource()


void InfoResource::handleRequest( const Http::Request &request, Http::Response &response )
{
  // TODO: include date, or something..., maybe? Because otherwise why bother serializing JSON here?
  response.out() << Wt::Json::serialize(m_result);
}//void InfoResource::handleRequest(...)


AnalysisResource::AnalysisResource()
: WResource(),
  m_drfs( Analysis::available_drfs() )
{
  
}


void AnalysisResource::handleRequest( const Wt::Http::Request &request, Wt::Http::Response &response )
{
  try
  {
    Wt::log("debug:app") << "AnalysisResource::handleRequest";
    
    
    // Check to see if the currently pending analysis queue is really long.
    //
    // If we have the Wt server option max-request-size set at 20480 kiB, we can
    //  expect each session to take up at most ~80 MB, which is like 4 GB ram, if
    //  we allow 50 pending analysis.
    //  TODO: make run-time option for how many analysis can be queued
    //  TODO: see if its that hard to check memory resources, and queue processing times and use those to limit number of pending requests
    //        maybe queue can track memory usage and last time it took to get through, and we can use this instead of just queue length
    const size_t ana_queue_len = Analysis::analysis_queue_length();
    if( ana_queue_len > 50 )
    {
      response.setStatus(503); //Service Unavailable
      response.addHeader( "Retry-After", "5" );  //number of seconds to retry after.  If we knew how long jibs were taking to get through the queue, we could maybe give a better estimate?
      response.out() << "{\"code\": 4, \"message\": \"Analysis queue is currently full.\"}";
      return;
    }//if( ana_queue_len > 50 )
    
    
    string drf = "auto";
    
    const std::string *optionsstr = request.getParameter( "options" );
    if( optionsstr )
    {
      try
      {
        Json::Object options;
        Json::parse(*optionsstr, options );
        
        if( options.contains("drf") )
        {
          const Json::Value &drfopt = options.get("drf");
          if( drfopt.type() != Json::Type::String )
          {
            response.setStatus(400);
            response.out() << "{\"code\": 1, \"message\": \"Invalid drf specification format.\"}";
            return;
          }
          
          drf = (const string &)drfopt;
          cout << "Got DRF '" << drf << "' from options." << endl;
        }
        
        // TODO: warn about other options specified?
      }catch( Json::ParseError &e )
      {
        cerr << "Error parsing options parameter: " << e.what() << endl;
        response.setStatus(400);
        response.out() << "{\"code\": 1, \"message\": \"Invalid drf JSON format.\"}";
        return;
      }catch( WException &e )
      {
        // I dont think we would ever end up here, but JIC.
        cerr << "Error decoding options parameter: " << e.what() << endl;
        response.setStatus(400);
        response.out() << "{\"code\": 1, \"message\": \"Invalid drf type.\"}";
        return;
      }
    }//if( optionsstr )
    
    
    if( drf == "auto" )
    {
      const Http::ParameterMap &pars = request.getParameterMap();
      
      const auto iter = pars.find("drf");
      if( iter != end(pars) )
      {
        if( iter->second.size() == 1 )
        {
          drf = iter->second.at(0);
          cout << "url param drf=" << drf << endl;
        }else
        {
          response.setStatus(400);
          response.out() << "{\"code\": 1, \"message\": \"Invalid drf specification format.\"}";
          return;
        }
      }
    }else if( request.getParameterMap().count("drf") )
    {
      //TODO: warn if drf is specified in both places.
    }//if( we should try to get drf from URL parameters ) / else
    
    
    if( (drf != "auto") && (std::find(begin(m_drfs), end(m_drfs), drf) == end(m_drfs)) )
    {
      response.setStatus(400);
      response.out() << "{\"code\": 2, \"message\": \"Invalid drf value specified.\"}";
      return;
    }//if( an okay DRF value )
    
    
    const Http::UploadedFileMap &files = request.uploadedFiles();
    
    if( (files.size() != 1) && (files.size() != 2) )
    {
      response.setStatus(400);
      response.out() << "{\"code\": 3, \"message\": \"One or two files must be uploaded.\"}";
    }//if( not 1 or 2 files )
    
    assert( (files.size() == 1) || (files.size() == 2) );
      
    tuple<AnalysisFromFiles::SpecClassType,string,string> input1;
    boost::optional<tuple<AnalysisFromFiles::SpecClassType,string,string>> input2;
    
    size_t inputnum = 0;
    bool foreFromClientName = false, backFromClientName = false;
    for( const auto &uploaded : files )
    {
      const Http::UploadedFile &file = uploaded.second;
      
      const string &spoolName = file.spoolFileName();
      const string &clientName = file.clientFileName();
      
      //cout << "Uploaded file: " << uploaded.first << " clientName=" << clientName
      //     << ", contentType=" << file.contentType() << endl;
      
      AnalysisFromFiles::SpecClassType type = AnalysisFromFiles::SpecClassType::Unknown;
      if( files.size() == 1 )
      {
        type = AnalysisFromFiles::SpecClassType::ForegroundAndBackground;
        //"fore", "ipc", "ioi", "item", "primary", "interest", "concern", "unk"
      }else if( AnalysisFromFiles::maybe_foreground_from_filename( uploaded.first ) )
      {
        type = AnalysisFromFiles::SpecClassType::Foreground;
      }else if( AnalysisFromFiles::maybe_background_from_filename(uploaded.first) )
      {
        type = AnalysisFromFiles::SpecClassType::Background;
      }else if( AnalysisFromFiles::maybe_foreground_from_filename(clientName) )
      {
        foreFromClientName = true;
        type = AnalysisFromFiles::SpecClassType::SuspectForeground;
      }else if( AnalysisFromFiles::maybe_background_from_filename(clientName) )
      {
        backFromClientName = true;
        type = AnalysisFromFiles::SpecClassType::SuspectBackground;
      }else if( files.size() == 2 )
      {
        type = inputnum ? AnalysisFromFiles::SpecClassType::SuspectBackground : AnalysisFromFiles::SpecClassType::SuspectForeground;
      }
      
      if( inputnum == 0 )
        input1 = make_tuple( type, spoolName, clientName );
      else
        input2 = make_tuple( type, spoolName, clientName );
      
      ++inputnum;
    }//for( const auto &uploaded : files )
    
    assert( (inputnum == 1) || (inputnum == 2) );
    
    // If both files had client names that clearly labeled things, we will trust it even more.
    if( foreFromClientName && backFromClientName )
    {
      if( get<0>(input1) == AnalysisFromFiles::SpecClassType::SuspectForeground )
      {
        get<0>(input1) = AnalysisFromFiles::SpecClassType::Foreground;
        if( input2 )
          get<0>(*input2) = AnalysisFromFiles::SpecClassType::Background;
      }else
      {
        get<0>(input1) = AnalysisFromFiles::SpecClassType::Background;
        if( input2 )
          get<0>(*input2) = AnalysisFromFiles::SpecClassType::Foreground;
      }
    }//if( foreFromClientName && backFromClientName )
    
    
    shared_ptr<SpecUtils::SpecFile> inputspec;
    
    try
    {
      inputspec = AnalysisFromFiles::create_input( input1, input2 );
    }catch( std::exception &e )
    {
      response.setStatus(400);
      
      Json::Object returnjson;
      returnjson["code"] = 3;
      returnjson["message"] = WString::fromUTF8(e.what());
      
      response.out() << Json::serialize(returnjson);
      return;
    }//try / catch to create input spectrum file
    
    assert( inputspec );
    
    if( drf == "auto" )
    {
      drf = Analysis::get_drf_name( inputspec );
      
      if( drf.empty() )
      {
        response.setStatus(400);
        response.out() << "{\"code\": 5, \"message\": \"Could not determine detector response to use; please specify one.\"}";
      }//if( drf == "" )
    }//if( drf == "auto" )
    
    
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
    
    //result.spec_file; //std::shared_ptr<SpecUtils::SpecFile>
    
    // once we're here, the analysis should be done.
    const Wt::Json::Object resultjson = result.toJson();
    response.out() << Json::serialize(resultjson);
    
    if( (result.gadras_intialization_error < 0) || (result.gadras_analysis_error < 0) )
      response.setStatus(400);
  }catch( ... )
  {
    cerr << "AnalysisResource::handleRequest: Uncaught exception type!!!" << endl;
    response.out() << "{\"code\": 999, \"message\": \"Unknown error.\"}";
    response.setStatus(400);
  }
}//AnalysisResource::handleRequest(...)


}//namespace RestResources



