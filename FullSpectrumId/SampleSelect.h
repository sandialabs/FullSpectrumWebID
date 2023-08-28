#ifndef SampleSelect_h
#define SampleSelect_h

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

#include <vector>

#include <Wt/WContainerWidget.h>

namespace Wt
{
  class WText;
  class WSpinBox;
}

namespace SpecUtils
{
  class SpecFile;
  enum class SourceType : int;
}

/**
 
 */
class SampleSelect : public Wt::WContainerWidget
{
public:
  SampleSelect( const SpecUtils::SourceType type, const std::string &type_desc );
  
  void setSpecFile( std::shared_ptr<SpecUtils::SpecFile> spec );
  
  /** Returns the sample number corresponding to the users current input.
   
   Note that this is not simply the number entered by the user, but instead this class allows the user to select from 1 to the number of
   samples, all contiguous - where the spectrum file itself may be any order, or missing samples or whatever.  We also will filter out
   intrinsic and calibration samples by default, to avoid confusion by the user.
    
   Throws exception if current input is not valid, or no spectrum file is set.
   */
  int currentSample();
  
  Wt::Signal<int> &sampleChanged();
  
protected:
  void userChangedValue();
  void updateDescription();
  
  const SpecUtils::SourceType m_type;
  std::shared_ptr<SpecUtils::SpecFile> m_spec;
  std::vector<int> m_samples;
  
  Wt::Signal<int> m_sampleChangedSignal;
  
  Wt::WSpinBox *m_sampleSelect;
  Wt::WText *m_totalSamples;
  Wt::WText *m_desc;
};//class SampleSelect

#endif //SampleSelect_h
