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

#include <map>
#include <set>
#include <deque>
#include <limits>
#include <vector>
#include <numeric>
#include <cassert>
#include <sstream>
#include <iostream>
#include <stdexcept>

#define BOOST_UBLAS_TYPE_CHECK 0
#include <boost/numeric/ublas/lu.hpp>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/triangular.hpp>


#include "SpecUtils/SpecFile.h"
#include "FullSpectrumId/EnergyCal.h"
#include "SpecUtils/EnergyCalibration.h"

using namespace std;


namespace
{
template<class T>
bool matrix_invert( const boost::numeric::ublas::matrix<T>& input,
                   boost::numeric::ublas::matrix<T> &inverse )
{
  using namespace boost::numeric;
  ublas::matrix<T> A( input );
  ublas::permutation_matrix<std::size_t> pm( A.size1() );
  const size_t res = lu_factorize(A, pm);
  if( res != 0 )
    return false;
  inverse.assign( ublas::identity_matrix<T>( A.size1() ) );
  lu_substitute(A, pm, inverse);
  return true;
}//matrix_invert



vector<float> fit_for_poly_coefs( const vector<pair<double,double>> &channels_energies,
                            const int poly_terms )
{
  //Using variable names of section 15.4 of Numerical Recipes, 3rd edition
  //Implementation is quite inneficient
  using namespace boost::numeric;
  
  const size_t npoints = channels_energies.size();
  ublas::matrix<double> A( npoints, poly_terms );
  ublas::vector<double> b( npoints );
  
  for( size_t row = 0; row < npoints; ++row )
  {
    b(row) = channels_energies[row].second;
    for( int col = 0; col < poly_terms; ++col )
      A(row,col) = std::pow( channels_energies[row].first, double(col) );
  }//for( int col = 0; col < poly_terms; ++col )
  
  const ublas::matrix<double> A_transpose = ublas::trans( A );
  const ublas::matrix<double> alpha = prod( A_transpose, A );
  ublas::matrix<double> C( alpha.size1(), alpha.size2() );
  const bool success = matrix_invert( alpha, C );
  if( !success )
    throw runtime_error( "fit_for_poly_coefs(...): trouble inverting matrix" );
  
  const ublas::vector<double> beta = prod( A_transpose, b );
  const ublas::vector<double> a = prod( C, beta );
  
  vector<float> poly_coeffs( poly_terms );
  for( int coef = 0; coef < poly_terms; ++coef )
    poly_coeffs[coef] = static_cast<float>( a(coef) );
  
  return poly_coeffs;
}//void fit_for_poly_coefs(...)


vector<float> fit_for_fullrangefraction_coefs( const vector<pair<double,double>> &channels_energies,
                            const size_t nchannels, const int nterms )
{
  //Using variable names of section 15.4 of Numerical Recipes, 3rd edition
  //Implementation is quite inneficient
  using namespace boost::numeric;
  
  const int polyterms = std::min( nterms, 5 );
  const size_t npoints = channels_energies.size();
  
  ublas::matrix<double> A( npoints, polyterms );
  ublas::vector<double> b( npoints );
  
  for( size_t row = 0; row < npoints; ++row )
  {
    const double x = channels_energies[row].first / nchannels;
    
    b(row) = channels_energies[row].second;
    for( int col = 0; col < std::min(4,polyterms); ++col )
      A(row,col) = std::pow( x, static_cast<double>(col) );
    if( polyterms > 4 )
      A(row,5) = 1.0 / (1.0 + 60.0*x);
  }//for( int col = 0; col < poly_terms; ++col )
  
  const ublas::matrix<double> A_transpose = ublas::trans( A );
  const ublas::matrix<double> alpha = prod( A_transpose, A );
  ublas::matrix<double> C( alpha.size1(), alpha.size2() );
  const bool success = matrix_invert( alpha, C );
  if( !success )
    throw runtime_error( "fit_for_fullrangefraction_coefs(...): trouble inverting matrix" );
  
  const ublas::vector<double> beta = prod( A_transpose, b );
  const ublas::vector<double> a = prod( C, beta );
  
  vector<float> frf_coeffs( polyterms );
  for( int coef = 0; coef < polyterms; ++coef )
    frf_coeffs[coef] = static_cast<float>( a(coef) );
  
  return frf_coeffs;
}//void fit_for_fullrangefraction_coefs(...)


double poly_coef_fcn( size_t order, double channel, size_t nchannel )
{
  return pow( channel, static_cast<double>(order) );
}

double frf_coef_fcn( size_t order, double channel, size_t nchannel )
{
  const double x = channel / nchannel;
  if( order == 4 )
    return 1.0 / (1.0 + 60.0*x);
  return pow( x, static_cast<double>(order) );
}


double fit_energy_cal_imp( const std::vector<EnergyCal::RecalPeakInfo> &peakinfos,
                                      const std::vector<bool> &fitfor,
                                      const size_t nchannels,
                                      const std::vector<std::pair<float,float>> &dev_pairs,
                                      std::vector<float> &coefs,
                                      std::vector<float> &coefs_uncert,
                                      std::function<double(size_t,double,size_t)> coeffcn )
{
  assert( coeffcn );
  
  const size_t npeaks = peakinfos.size();
  const size_t nparsfit = static_cast<size_t>( std::count(begin(fitfor),end(fitfor),true) );
  
  if( npeaks < 1 )
    throw runtime_error( "Must have at least one peak" );
  
  if( nparsfit < 1 )
    throw runtime_error( "Must fit for at least one coefficient" );
  
  if( nparsfit > npeaks )
    throw runtime_error( "Must have at least as many peaks as coefficients fitting for" );
  
  if( (nparsfit != fitfor.size()) && (coefs.size() != fitfor.size()) )
    throw runtime_error( "You must supply input coefficient when any of the coefficients are fixed" );
  
  //Energy = P0 + P1*x + P2*x^2 + P3*x^3, where x is bin number
  //  However, some of the coeffeicents may not be being fit for.
  vector<float> mean_bin( npeaks ), true_energies( npeaks ), energy_uncerts( npeaks );
  for( size_t i = 0; i < npeaks; ++i )
  {
    mean_bin[i] = peakinfos[i].peakMeanBinNumber;
    true_energies[i] = peakinfos[i].photopeakEnergy;
    energy_uncerts[i] = true_energies[i] * peakinfos[i].peakMeanUncert / std::max(peakinfos[i].peakMean,1.0);
  }
  
  //General Linear Least Squares fit
  //Using variable names of section 15.4 of Numerical Recipes, 3rd edition
  //Implementation is quite inneficient
  //Energy_i = P0 + P1*pow(i,1) + P2*pow(i,2) + P3*pow(i,3)  (for polynomial)
  using namespace boost::numeric;
  
  ublas::matrix<double> A( npeaks, nparsfit );
  ublas::vector<double> b( npeaks );
  
  for( size_t row = 0; row < npeaks; ++row )
  {
    double data_y = true_energies[row];
    const double data_y_uncert = fabs( energy_uncerts[row] );
    
    data_y -= SpecUtils::correction_due_to_dev_pairs( true_energies[row], dev_pairs );
    
    for( size_t col = 0, coef_index = 0; coef_index < fitfor.size(); ++coef_index )
    {
      if( fitfor[coef_index] )
      {
        assert( col < nparsfit );
        A(row,col) = coeffcn( coef_index, mean_bin[row], nchannels ) / data_y_uncert; //std::pow( mean_bin[row], double(coef_index)) / data_y_uncert;
        ++col;
      }else
      {
        data_y -= coefs[coef_index] * coeffcn( coef_index, mean_bin[row], nchannels);
      }
    }//
    
    b(row) = data_y / data_y_uncert;
  }//for( int col = 0; col < order; ++col )
  
  const ublas::matrix<double> A_transpose = ublas::trans( A );
  const ublas::matrix<double> alpha = prod( A_transpose, A );
  ublas::matrix<double> C( alpha.size1(), alpha.size2() );
  const bool success = matrix_invert( alpha, C );
  if( !success )
    throw runtime_error( "Trouble inverting least linear squares matrix" );
  
  const ublas::vector<double> beta = prod( A_transpose, b );
  const ublas::vector<double> a = prod( C, beta );
  
  coefs.resize( fitfor.size(), 0.0 );
  coefs_uncert.resize( fitfor.size(), 0.0 );
  
  for( size_t col = 0, coef_index = 0; coef_index < fitfor.size(); ++coef_index )
  {
    if( fitfor[coef_index] )
    {
      assert( col < nparsfit );
      coefs[coef_index] = static_cast<float>( a(col) );
      coefs_uncert[coef_index] = static_cast<float>( std::sqrt( C(col,col) ) );
      ++col;
    }else
    {
      coefs_uncert[coef_index] = 0.0;
    }
  }//for( int coef = 0; coef < order; ++coef )
  
  double chi2 = 0;
  for( size_t bin = 0; bin < npeaks; ++bin )
  {
    double y_pred = 0.0;
    for( size_t i = 0; i < fitfor.size(); ++i )
      y_pred += coefs[i] * coeffcn( i, mean_bin[bin], nchannels );
    y_pred += SpecUtils::deviation_pair_correction( y_pred, dev_pairs );
    chi2 += std::pow( (y_pred - true_energies[bin]) / energy_uncerts[bin], 2.0 );
  }//for( int bin = 0; bin < nbin; ++bin )
  
  return chi2;
}//double fit_energy_cal_imp


double fit_from_channel_energies_imp( const size_t ncoeffs, const vector<float> &channel_energies,
                                      std::function<double(size_t,double,size_t)> coeffcn,
                                      vector<float> &coefs )
{
  if( ncoeffs < 2 )
    throw runtime_error( "fit_from_channel_energies_imp: You must request at least two coefficients" );
    
  //For polynomial this isnt a programming or math limitation, just a sanity limitation; FRF should
  //  have max of 5 coefficients
  if( ncoeffs >= 6 )
    throw runtime_error( "fit_from_channel_energies_imp: You must request less than 6 coefficients" );
  
  const size_t nenergies = channel_energies.size();
  if( nenergies <= 6 )
    throw runtime_error( "fit_from_channel_energies_imp: Input energies must have at least 6 entries" );
  
  const size_t nchannel = nenergies - 1;
  
  for( size_t i = 1; i < nenergies; ++i )
    if( channel_energies[i-1] >= channel_energies[i] )
      throw runtime_error( "fit_from_channel_energies_imp: Input energies must be stricktly increasing" );
  
  //General Linear Least Squares fit
  //Using variable names of section 15.4 of Numerical Recipes, 3rd edition
  //Implementation is quite inneficient
  using namespace boost::numeric;
  
  ublas::matrix<double> A( nenergies, ncoeffs );
  ublas::vector<double> b( nenergies );
  
  for( size_t row = 0; row < nenergies; ++row )
  {
    //Energy_i = P0 + P1*pow(i,1) + P2*pow(i,2) + P3*pow(i,3)  //for polynomial
    const double uncert = 1.0;
    for( size_t col = 0; col < ncoeffs; ++col )
      A(row,col) = coeffcn(row, col, nchannel) / uncert;
    b(row) = channel_energies[row] / uncert;
  }//for( int col = 0; col < order; ++col )
  
  const ublas::matrix<double> A_transpose = ublas::trans( A );
  const ublas::matrix<double> alpha = prod( A_transpose, A ); //alpha is ncoeffs x ncoeffs matrix
  ublas::matrix<double> C( alpha.size1(), alpha.size2() );
  const bool success = matrix_invert( alpha, C );
  if( !success )
    throw runtime_error( "Trouble inverting least linear squares matrix" );
  
  const ublas::vector<double> beta = prod( A_transpose, b );
  const ublas::vector<double> a = prod( C, beta );
  
  coefs.resize( ncoeffs, 0.0 );
  for( size_t col = 0; col < ncoeffs; ++col )
    coefs[col] = static_cast<float>( a(col) );
  
  double avrgdiffs = 0.0;
  for( size_t i = 0; i < nenergies; ++i )
  {
    double energy = 0.0;
    for( size_t col = 0; col < ncoeffs; ++col )
      energy += coefs[col] * coeffcn(i, col, nchannel);
    avrgdiffs += fabs( energy - channel_energies[i] );
  }
  
  return avrgdiffs / nenergies;
}//fit_from_channel_energies_imp

}//namespace





double EnergyCal::fit_energy_cal_frf( const std::vector<EnergyCal::RecalPeakInfo> &peaks,
                                      const std::vector<bool> &fitfor,
                                      const size_t nchannels,
                                      const std::vector<std::pair<float,float>> &dev_pairs,
                                      std::vector<float> &coefs,
                                      std::vector<float> &uncert )
{
  return fit_energy_cal_imp( peaks, fitfor, nchannels, dev_pairs, coefs, uncert, &frf_coef_fcn );
}


double EnergyCal::fit_energy_cal_poly( const std::vector<EnergyCal::RecalPeakInfo> &peaks,
                            const vector<bool> &fitfor,
                            const size_t nchannels,
                            const std::vector<std::pair<float,float>> &dev_pairs,
                            vector<float> &coefs,
                            vector<float> &uncert )
{
  return fit_energy_cal_imp( peaks, fitfor, nchannels, dev_pairs, coefs, uncert, &poly_coef_fcn );
}//double fit_energy_cal_poly(...)


double EnergyCal::fit_poly_from_channel_energies( const size_t ncoeffs,
                                             const std::vector<float> &channel_energies,
                                             std::vector<float> &coefs )
{
  return fit_from_channel_energies_imp( ncoeffs, channel_energies, &poly_coef_fcn, coefs );
}//fit_poly_from_channel_energies(...)



double EnergyCal::fit_full_range_fraction_from_channel_energies( const size_t ncoeffs,
                                                  const std::vector<float> &channel_energies,
                                                  std::vector<float> &coefs )
{
  if( ncoeffs >= 5 )
    throw runtime_error( "fit_full_range_fraction_from_channel_energies:"
                         " You must request less than 5 coefficients" );
  
  return fit_from_channel_energies_imp( ncoeffs, channel_energies, &frf_coef_fcn, coefs );
}//fit_full_range_fraction_from_channel_energies(...)




shared_ptr<const SpecUtils::EnergyCalibration>
EnergyCal::propogate_energy_cal_change( const shared_ptr<const SpecUtils::EnergyCalibration> &orig_cal,
                             const shared_ptr<const SpecUtils::EnergyCalibration> &new_cal,
                             const shared_ptr<const SpecUtils::EnergyCalibration> &other_cal )
{
  using namespace SpecUtils;
  
  if( !orig_cal || !new_cal || !other_cal
     || !orig_cal->valid() || !new_cal->valid() || !other_cal->valid()
     || (orig_cal->type() == EnergyCalType::LowerChannelEdge)
     || (new_cal->type() == EnergyCalType::LowerChannelEdge) )
    throw runtime_error( "EnergyCal::propogate_energy_cal_change invalid input" );
  
  if( orig_cal == new_cal )
    return other_cal;
  
  auto answer = make_shared<EnergyCalibration>();
  
  const vector<float> &prev_disp_coefs = orig_cal->coefficients();
  const vector<float> &new_disp_coefs = new_cal->coefficients();
  const vector<float> &other_coeffs = other_cal->coefficients();
  
  const size_t orig_num_channel = orig_cal->num_channels();
  const size_t new_num_channel = orig_cal->num_channels();
  const size_t other_num_channel = other_cal->num_channels();
  
  // \TODO: we currently arent using deviation pairs when converting between channel number and
  //        energy in this function.  This isnt actually correct; the aprehentions I have are:
  //        1) I havent tested that with deviation is numerically stable enough.
  //        2) What if its only deviation pairs that have changed, do we then want to correct for
  //           this using the coefficients?
  //        3) I'm not a hundred percent certain we actually want to correct for deviation pairs;
  //           needs more thought;
  const vector<pair<float,float>> prev_disp_devs; // = orig_cal->deviation_pairs();
  const vector<pair<float,float>> new_disp_devs;  // = new_cal->deviation_pairs();
  const vector<pair<float,float>> other_devs;     // = other_cal->deviation_pairs();
  
  // Deal with the easy case of other_cal being lower channel energies.
  if( other_cal->type() == EnergyCalType::LowerChannelEdge )
  {
    const size_t nchannel = other_cal->num_channels();
    const shared_ptr<const vector<float>> &old_lower_ptr = other_cal->channel_energies();
    assert( old_lower_ptr && !old_lower_ptr->empty() );
    
    const vector<float> &old_lower = *old_lower_ptr;
    assert( old_lower.size() >= (nchannel + 1) ); //actually should always be equal
    if( nchannel >= old_lower.size() )
      throw runtime_error( "EnergyCal::propogate_energy_cal_change: really unexpected programing error" );
    
    vector<float> new_lower( nchannel + 1 );
    for( size_t i = 0; i <= nchannel; ++i )
    {
      const double equiv_channel = orig_cal->channel_for_energy( old_lower[i] );
      new_lower[i] = new_cal->energy_for_channel( equiv_channel );
    }
    
    answer->set_lower_channel_energy( nchannel, std::move(new_lower) );
    return answer;
  }//if( other_cal->type() == EnergyCalType::LowerChannelEdge )

  
  assert( (orig_cal->type() == EnergyCalType::FullRangeFraction)
          || (orig_cal->type() == EnergyCalType::Polynomial)
          || (orig_cal->type() == EnergyCalType::UnspecifiedUsingDefaultPolynomial) );
  assert( (new_cal->type() == EnergyCalType::FullRangeFraction)
          || (new_cal->type() == EnergyCalType::Polynomial)
          || (new_cal->type() == EnergyCalType::UnspecifiedUsingDefaultPolynomial) );
  assert( (other_cal->type() == EnergyCalType::FullRangeFraction)
          || (other_cal->type() == EnergyCalType::Polynomial)
          || (other_cal->type() == EnergyCalType::UnspecifiedUsingDefaultPolynomial) );

  const double accuracy = 0.00001;
  const size_t order = std::max( other_coeffs.size(),
                                 std::max(prev_disp_coefs.size(), new_disp_coefs.size()) );
  
  vector<pair<double,double>> channels_energies;  //this gives <channel number,energy it should be>
  for( size_t i = 0; i < order; ++i )
  {
    const size_t display_channel = ((order - i - 1) * orig_num_channel) / (order - 1);
     
    double old_disp_energy = std::numeric_limits<double>::quiet_NaN(),
           new_disp_energy = std::numeric_limits<double>::quiet_NaN(),
           other_channel = std::numeric_limits<double>::quiet_NaN();
    
    switch( orig_cal->type() )
    {
      case EnergyCalType::FullRangeFraction:
      {
        old_disp_energy = fullrangefraction_energy( display_channel, prev_disp_coefs, orig_num_channel, prev_disp_devs );
        new_disp_energy = fullrangefraction_energy( display_channel, new_disp_coefs, new_num_channel, new_disp_devs );
        break;
      }//case orig_cal was FRF
        
      case EnergyCalType::Polynomial:
      case EnergyCalType::UnspecifiedUsingDefaultPolynomial:
      {
        old_disp_energy = polynomial_energy( display_channel, prev_disp_coefs, prev_disp_devs );
        new_disp_energy = polynomial_energy( display_channel, new_disp_coefs, new_disp_devs );
        break;
      }//case: orig_cal was Poly
        
      case EnergyCalType::LowerChannelEdge:
      case EnergyCalType::InvalidEquationType:
        assert( 0 );
        break;
    }//switch( orig_cal->type() )
    
    assert( !IsNan(old_disp_energy) && !IsNan(new_disp_energy) );
    
    double check_energy = std::numeric_limits<double>::quiet_NaN();  //Just for development test
    switch( other_cal->type() )
    {
      case EnergyCalType::Polynomial:
      case EnergyCalType::UnspecifiedUsingDefaultPolynomial:
        other_channel = find_polynomial_channel( old_disp_energy, other_coeffs, other_num_channel,
                                                 other_devs, accuracy );
        check_energy = polynomial_energy( other_channel, other_coeffs, other_devs );
        break;
        
      case EnergyCalType::FullRangeFraction:
        other_channel = find_fullrangefraction_channel( old_disp_energy, other_coeffs,
                                                       other_num_channel, other_devs, accuracy );
        check_energy = fullrangefraction_energy( other_channel, other_coeffs, other_num_channel,
                                                 other_devs );
        break;
        
      case EnergyCalType::LowerChannelEdge:
      case EnergyCalType::InvalidEquationType:
        assert( 0 );
        break;
    }//switch( other_cal->type() )
    
    assert( !IsNan(other_channel) && !IsNan(check_energy) );
    
    channels_energies.push_back( {other_channel, new_disp_energy} );
  
#if( PERFORM_DEVELOPER_CHECKS )
    const double diff = fabs(check_energy - old_disp_energy);
    const double max_energy = std::max( fabs(check_energy), fabs(old_disp_energy) );
    
    if( (diff > (max_energy * 1.0E-5)) && (diff > 0.00001) )
    {
      char buffer[256];
      snprintf( buffer, sizeof(buffer), "Found case going from energy-->channel-->energy"
               " gave seconf energy too different than initial energy by %f with check_energy=%f"
               " and old_disp_energy=%f", diff, check_energy, old_disp_energy );
      log_developer_error( __func__, buffer );
    }//if( diff is larger than we wanted )

    assert( diff <= max_energy*1.0E-6 || (diff < 0.00001) );
#endif
  }//for( size_t i = 0; i < order; ++i )
  
  const auto &dev_pairs = other_cal->deviation_pairs();
  switch( other_cal->type() )
  {
    case SpecUtils::EnergyCalType::Polynomial:
    case SpecUtils::EnergyCalType::UnspecifiedUsingDefaultPolynomial:
    {
      const vector<float> new_other_coefs = fit_for_poly_coefs( channels_energies, order );
      answer->set_polynomial( other_num_channel, new_other_coefs, dev_pairs );
      break;
    }
      
    case SpecUtils::EnergyCalType::FullRangeFraction:
    {
      const vector<float> new_other_coefs = fit_for_fullrangefraction_coefs( channels_energies,
                                                                        other_num_channel, order );
      answer->set_full_range_fraction( other_num_channel, new_other_coefs, dev_pairs );
      break;
    }
      
    case SpecUtils::EnergyCalType::LowerChannelEdge:
    case SpecUtils::EnergyCalType::InvalidEquationType:
      assert( 0 );
      break;
  }//switch( other_cal->type() )
  
  assert( answer->valid() );
  
  return answer;
}//propogate_energy_cal_change(...)
