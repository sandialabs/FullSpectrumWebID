#ifndef RestResources_h
#define RestResources_h

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
#include <string>

#include <Wt/WResource.h>
#include <Wt/Json/Object.h>

namespace Wt{ namespace Http {
  class Request;
  class Response;
} }

/** Some resources that define a REST API for analysis.
 
 Some example requests from the command line are:
  curl 127.0.0.1:8080/api/v1/info
  curl -v  -f "options={\"drf\": \"IdentiFINDER-NGH\"}" -F "foreground=@./foreground.n42" -F "background=@./background.n42"  127.0.0.1:8080/api/v1/analysis
  curl -v  -F "foreground=@./foreground.n42" -F "background=@./background.n42"  127.0.0.1:8080/api/v1/analysis?drf=IdentiFINDER-NGH
  curl -v  -F "filesomething=@./foregroundWithBackground.n42" 127.0.0.1:8080/api/v1/analysis?drf=IdentiFINDER-NGH
  curl -v  -F "file=@./someFile.n42" 127.0.0.1:8080/api/v1/analysis
 */
namespace RestResources
{

/** Gives information about required options, gadras version, etc in JSON format. */
class InfoResource : public Wt::WResource
{
public:
  InfoResource();
  
  virtual void handleRequest( const Wt::Http::Request &request, Wt::Http::Response &response );
  
protected:
  const std::string m_gadras_version;
  const std::vector<std::string> m_drfs;
  
  Wt::Json::Object m_result;
};//class InfoResource


class AnalysisResource : public Wt::WResource
{
public:
  AnalysisResource();
  
  virtual void handleRequest( const Wt::Http::Request &request, Wt::Http::Response &response );
  
protected:
  const std::vector<std::string> m_drfs;
};//class InfoResource

}//namespace RestResources

#endif //RestResources_h
