#ifndef FileAnalysis_h
#define FileAnalysis_h

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

#include <set>
#include <tuple>
#include <vector>
#include <string>

#include <boost/optional.hpp>

// Forward decelerations
namespace SpecUtils
{
class SpecFile;
class Measurement;
}//namespace SpecUtils


/** These functions facilitate creating analysis analysis input from files on the filesystem,
 for either the command line use, or the REST API use.
 */
namespace AnalysisFromFiles
{

/** Parses SpecFile file from file on disk.
 
 Use this function to parse all user-uploaded or specified spectrum files.
 
 Slightly limits the spectrum formats tried - may further restrict things in the future.
 
 @returns parsed file, or null if file did not parse.
 */
std::shared_ptr<SpecUtils::SpecFile> parse_file( const std::string &filepath,
                                                 const std::string &username );

/** Filters the energy calibration variants out, so after this cal the spectrum will just have the useful calibration types
 E.g., select "LinEnCal" over "CmpEnCal",  or "9MeV" (with more channels) vs "2.5MeV", etc 

 Throws std::exception on error; like if it cant decide, or invalid spectrum file or whatever.  The exception message is suitable for displaying
 to the user.
 */
void filter_energy_cal_variants( std::shared_ptr<SpecUtils::SpecFile> spec );

enum class SpecClassType
{
  Unknown,
  Foreground,
  Background,
  SuspectForeground,
  SuspectBackground,
  ForegroundAndBackground
};//enum SpecClassType

/** Will return a SpecFile with either a foreground and a background spectrum, ready to feed to Analysis, or will return a portal/search
 file.

 Takes as input, one or two pairs of information.
 
 TODO: finish documenting when I flush this out.
 
 Will through exception on error, with the message being appropriate for displaying to user.
 Otherwise will always return valid input to analysis.
 */
std::shared_ptr<SpecUtils::SpecFile> create_input( const std::tuple<SpecClassType,std::string,std::string> &input1,
                                                   boost::optional<std::tuple<SpecClassType,std::string,std::string>> input2 = boost::none );


bool maybe_foreground_from_filename( const std::string &name );
bool maybe_background_from_filename( const std::string &name );

/** @returns if you may potentially use the "derived" data to analyze, rather than the raw data.
 
 Note: Right now we are only using derived data from Verifinder detectors, since they will show
  up as searchmode data, but their derived data is what we would sum anyway.
  E.g., using derived data hasnt been tested for other systems!
 */
bool potentially_analyze_derived_data( std::shared_ptr<const SpecUtils::SpecFile> spec );

/** Retrieves the "derived" foreground and background measurements from a SpecFile.
 
 @param foreground "Derived" foreground Measurements will be placed here.
 @param foreground "Derived" background Measurements will be placed here.
 */
void get_derived_measurements( std::shared_ptr<const SpecUtils::SpecFile> spec,
                              std::set<std::shared_ptr<const SpecUtils::Measurement>> &foreground,
                              std::set<std::shared_ptr<const SpecUtils::Measurement>> &background );

/** Checks if the passed in looks like portal data */
bool is_portal_data( std::shared_ptr<const SpecUtils::SpecFile> spec );

}//namespace AnalysisFromFiles

#endif //FileAnalysis_h
