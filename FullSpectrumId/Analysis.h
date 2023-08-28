#ifndef FullSpectrum_Analysis_h
#define FullSpectrum_Analysis_h

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

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <functional>

#include <Wt/Json/Object.h>

//Forward declarations
//struct IsotopeIDResult;  //In GadrasIsotopeID.h

namespace SpecUtils
{
class SpecFile;
}

namespace Analysis
{

#if( !STATICALLY_LINK_TO_GADRAS )
// For windows we currently only have a DLL, not a .lib file, so I cant link at compile-time, so
//  we'll manually load the library and grab the function pointer.  And we'll do the same thing
//  for macOS, and if a CMake
bool load_gadras_lib( const std::string lib_name );
#endif

/** Set the GADRAS app directory.
 
 If you are going to call this function, you must call it before ever calling into GADRAS routines, or will throw exception.
 Will also throw exception if passed in directory is not a valid directory.
 
 Defaults to "gadras_isotope_id_run_directory"
 */
void set_gadras_app_dir( const std::string &dir );


std::vector<std::string> available_drfs();

/** Return the DRF pathname for a spectrum file.
 Returns empty string if couldnt determine.
 
 \TODO: Right now things are hard-coded and crap - need to make more better.
 \TODO: Need to improve SpecUtils to better detect more detector models
 */
std::string get_drf_name( const std::shared_ptr<SpecUtils::SpecFile> &spec );


// 10000*majorversion+100*minorversion+revision
int32_t gadras_version_number();

// e.x. "18.1.1"
std::string gadras_version_string();

/** Result for a simple analysis of single foreground and background */
struct AnalysisOutput
{
  /** Analysis ID provided by #AnalysisInput::ana_number */
  size_t ana_number;
  
  std::string drf_used;
  
  int gadras_intialization_error;
  int gadras_analysis_error;
  std::string error_message;
  
  std::vector<std::string> analysis_warnings;
  

  float stuff_of_interest;
  
  float rate_not_norm; //!< If negative, ignore
  
  
  /** The isotopes string provided by the analysis call into GADRAS.
   Will look something like:  "Cs137(H)", "Cs137(H)+Ba133(F)", "None", etc.
   
   For search-mode or portal data, this string is hacked together by our code.
   */
  std::string isotopes;
 
  // For fields below here and search-mode or portal data, we take the highest values for each
  //  isotope.
  //  TODO: figure out how to best list results for search-mode and portal data.
  //        - For simplicity we could probably just give the results for each (set-of) samples;
  //          if we do this we should then also take into account 'alarmBasisDuration' so multiple
  //          results may contribute to each sample
  
  float chi_sqr; //!< goodness of fit quantification; if negative ignore
  float alarm_basis_duration; //!< duration of foreground used for this result; if negative ignore
  
  // Currently under development, but these next arrays should all have the same length
  std::vector<std::string> isotope_names;
  std::vector<std::string> isotope_types;
  std::vector<float> isotope_count_rates; //!< If negative, ignore
  std::vector<float> isotope_confidences; //!< If negative, ignore
  std::vector<std::string> isotope_confidence_strs;
  
  /** The spectrum file used for the analysis.  This may either be the input spectrum file, in which case no need to update plot
   displayed to the user, or if the "raw" analysis fit for energy calibration, this will be a new spectrum file with the adjusted energy cal.
   */
  std::shared_ptr<SpecUtils::SpecFile> spec_file;
  
  AnalysisOutput();
  
  Wt::Json::Object toJson() const;
  
  std::string briefTxtSummary() const;
  
  std::string fullTxtSummary() const;
};//struct AnalysisOutput


enum class AnalysisType
{
  /** Analysis to perform isotope ID on a single foreground spectrum that has a background spectrum.
   
   Right now input must have exactly two records, one foreground, and one background.
   
   The GADRAS StaticIsotopeID function will be used to perform the analysis.
   
   \TODO: implement allowing multiple detectors
   */
  Simple,
  
  /** Data consists of consecutive time-slices of short duration (e.x., 0.1s, 0.5s, 1.0s) data, perhaps from multiple detectors, with
   no periods denoted as background or item of interest.
   */
  Search,
  
  /** Data consists of a well-defined background (of say at least 30 seconds or so), with at least 3, or so, consecutive time-slices of
   short-duration (ex., 0.1s, 0.5s, 1.0s) data, usually with multiple detectors.
   
   */
  Portal
};//enum class AnalysisType


struct AnalysisInput
{
  /** A unique analysis identifier to allow unambiguously matching results up to a request, in case user submits a new request
   while the previous one is being analyzed.
   */
  size_t ana_number;
  
  // If wt_app_id is non-empty, then #AnalysisInput::callback will be posted to the WApplication
  //  instance.  If it is empty, then #AnalysisInput::callback will be called immediately in the
  //  analysis thread.
  std::string wt_app_id;
  
  std::string drf_folder;
  
  std::vector<std::string> input_warnings;
  
  AnalysisType analysis_type;
  
  /** The number of entries in the file must be compatible with #analysis_type
   TODO: make this a const pointer
   */
  std::shared_ptr<SpecUtils::SpecFile> input;
  
  std::function<void(AnalysisOutput)> callback;
};//struct AnalysisInput



void start_analysis_thread();

void stop_analysis_thread();

void post_analysis( const AnalysisInput &input );

size_t analysis_queue_length();
}//namespace Analysis

#endif //FullSpectrum_Analysis_h
