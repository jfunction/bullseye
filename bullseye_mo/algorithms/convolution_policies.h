/********************************************************************************************
Bullseye:
An accelerated targeted facet imager
Category: Radio Astronomy / Widefield synthesis imaging

Authors: Benjamin Hugo, Oleg Smirnov, Cyril Tasse, James Gain
Contact: hgxben001@myuct.ac.za

Copyright (C) 2014-2015 Rhodes Centre for Radio Astronomy Techniques and Technologies
Department of Physics and Electronics
Rhodes University
Artillery Road P O Box 94
Grahamstown
6140
Eastern Cape South Africa

Copyright (C) 2014-2015 Department of Computer Science
University of Cape Town
18 University Avenue
University of Cape Town
Rondebosch
Cape Town
South Africa

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
********************************************************************************************/
#pragma once
#include <stdexcept>
#include <complex>
#include "uvw_coord.h"
#include "fft_shift_utils.h"

#include <boost/math/special_functions/bessel.hpp>
#include <boost/math/special_functions/factorials.hpp>
namespace imaging {
  class convolution_precomputed_fir {};
  class convolution_on_the_fly_computed_fir {};
  class convolution_nn {};
  /**
   Reference convolution policy
   */
  template <typename convolution_base_type, typename uvw_base_type, typename grid_base_type,
	    typename gridding_policy_type, typename convolution_mode>
  class convolution_policy {
  public:
    convolution_policy() {
      throw std::runtime_error("Undefined behaviour");
    }
    /**
     Convolve will call a gridding function of the active gridding policy (this can be of the normal visibility or its conjungate)
     All gridding policies should therefore support gridding both visibility terms (see Synthesis Imaging II, pg. 25-26)
     
     All specializing policies must have a function conforming to the following function header:
     uvw_coord: reference to central uvw coord (in continious space)
     gridding_function: pointer to member function of active gridding policy
     
     Must return the total accumulated weight (accross all (2N+1)**2 filter taps)
     */
    inline convolution_base_type convolve(const uvw_coord<uvw_base_type> & __restrict__ uvw,
			 const typename gridding_policy_type::trait_type::pol_vis_type & __restrict__ vis,
			 std::size_t no_grids_to_offset) const __restrict__ {
      throw std::runtime_error("Undefined behaviour");
    }
  };

  /**
   * Debugging kernel which generates the convolution kernel weights on the fly
   */
  template <typename convolution_base_type, typename uvw_base_type, typename grid_base_type, typename gridding_policy_type>
  class convolution_policy <convolution_base_type, uvw_base_type, grid_base_type, gridding_policy_type, convolution_on_the_fly_computed_fir> {
  private:
    std::size_t _nx;
    std::size_t _ny;
    std::size_t _grid_size_in_pixels;
    uvw_base_type _grid_u_centre;
    uvw_base_type _grid_v_centre;
    std::size_t _convolution_support;
    std::size_t _oversampling_factor;
    const convolution_base_type * __restrict__ _conv;
    std::size_t _conv_dim_size;
    uvw_base_type _conv_centre_offset;
    gridding_policy_type & __restrict__ _active_gridding_policy;
    std::size_t _cube_chan_dim_step;
  public:
    /**
     conv: ignored in this policy
     conv_support: integral half support area for the function
     conv_oversample: ignored in this policy
     polarization_index: index of the polarization correlation term currently being gridded
    */
    convolution_policy(std::size_t nx, std::size_t ny, std::size_t no_polarizations, std::size_t convolution_support, std::size_t oversampling_factor, 
		       const convolution_base_type * conv, gridding_policy_type & active_gridding_policy):
			_nx(nx), _ny(ny), _grid_size_in_pixels(nx*ny), _grid_u_centre(nx / 2), _grid_v_centre(ny / 2),
			_convolution_support(convolution_support*2 + 1), _oversampling_factor(oversampling_factor), 
			_conv(conv), _conv_dim_size(_convolution_support * _oversampling_factor),
			_conv_centre_offset(convolution_support),
			_active_gridding_policy(active_gridding_policy),
			_cube_chan_dim_step(nx*ny*no_polarizations)
			{}
    inline convolution_base_type convolve(const uvw_coord<uvw_base_type> & __restrict__ uvw,
					  const typename gridding_policy_type::trait_type::pol_vis_type & __restrict__ vis,
					  std::size_t no_grids_to_offset,
					  size_t facet_id) const __restrict__ {
	auto convolve = [this](convolution_base_type x)->convolution_base_type{
// 	  convolution_base_type beta = 1.6789f * (this->_convolution_support+1) - 0.9644; //regression line
// 	  convolution_base_type sqr_term = (2 * x / (this->_convolution_support+1));
// 	  convolution_base_type sqrt_term = 1 - sqr_term * sqr_term;
// 	  return (convolution_base_type)boost::math::cyl_bessel_j<convolution_base_type>(0,beta * sqrt(sqrt_term))/(this->_convolution_support+1);

	  //two element cosine (this is a complete stuffup...)
// 	  convolution_base_type alpha = -0.1307 * (this->_convolution_support+1) + 0.9561;
// 	  return alpha + (1 - alpha) * cos(2*M_PI*x/(this->_convolution_support+1));

	  //gausian introduces tapering
// 	  convolution_base_type sigma =  0.0349* this->_convolution_support + 0.37175;
// 	  return exp(-0.5 * (x/sigma)*(x/sigma));
	  
	  //sinc works okay
	  if (x != 0){
	    convolution_base_type param = M_PI*(x);
	    return (convolution_base_type)sin(param) / param;
	  } else {
	    return (convolution_base_type)1; //remove discontinuity
	  }
	};
	
	static bool output_filter = false;
	#include <string.h>
	if (output_filter){
	  FILE * pFile;
	  pFile = fopen("/scratch/filter.txt","w");
	  for (int x = -_convolution_support*_oversampling_factor/2; x <= _convolution_support*_oversampling_factor/2; ++x){
	    fprintf(pFile,"%f",convolve(x/(float)_oversampling_factor));
	    if (x < _convolution_support*_oversampling_factor/2)
	      fprintf(pFile,",");
	  }
	  fclose(pFile);
	  output_filter = false;
	}
	
	std::size_t chan_offset = no_grids_to_offset * _cube_chan_dim_step;

	uvw_base_type translated_grid_u = uvw._u + _grid_u_centre - _conv_centre_offset;
	uvw_base_type translated_grid_v = uvw._v + _grid_v_centre - _conv_centre_offset;
	std::size_t  disc_grid_u = std::lrint(translated_grid_u);
	std::size_t  disc_grid_v = std::lrint(translated_grid_v);
	uvw_base_type frac_u = -translated_grid_u + (uvw_base_type)disc_grid_u;
	uvw_base_type frac_v = -translated_grid_v + (uvw_base_type)disc_grid_v;
	convolution_base_type accum = 0;
	if (disc_grid_v + _convolution_support >= _ny || disc_grid_u + _convolution_support  >= _nx ||
	  disc_grid_v >= _ny || disc_grid_u >= _nx) return 0;
	{
            for (std::size_t  sup_v = 0; sup_v < _convolution_support; ++sup_v) {
                std::size_t  convolved_grid_v = disc_grid_v + sup_v;
                uvw_base_type conv_v = (uvw_base_type)sup_v -_conv_centre_offset + frac_v;
                for (std::size_t sup_u = 0; sup_u < _convolution_support; ++sup_u) {
                    std::size_t convolved_grid_u = disc_grid_u + sup_u;
		    uvw_base_type conv_u = (uvw_base_type)sup_u -_conv_centre_offset + frac_u;
                    std::size_t grid_flat_index = convolved_grid_v*_ny + convolved_grid_u;

                    convolution_base_type conv_weight = convolve(conv_v) * convolve(conv_u);
                    _active_gridding_policy.grid_polarization_terms(chan_offset + grid_flat_index, 
								    vis, conv_weight);
		    accum += conv_weight;
                }
            }
	}
	return accum;
    }
  };
  
  /**
   * Default oversampled convolution using precomputed kernel
   */
  template <typename convolution_base_type, typename uvw_base_type, typename grid_base_type, typename gridding_policy_type>
  class convolution_policy <convolution_base_type, uvw_base_type, grid_base_type, gridding_policy_type, convolution_precomputed_fir> {
  private:
    std::size_t _nx;
    std::size_t _ny;
    std::size_t _grid_size_in_pixels;
    uvw_base_type _grid_u_centre;
    uvw_base_type _grid_v_centre;
    std::size_t _convolution_support;
    std::size_t _oversampling_factor;
    const convolution_base_type * __restrict__ _conv;
    std::size_t _conv_dim_size;
    uvw_base_type _conv_centre_offset;
    gridding_policy_type & __restrict__ _active_gridding_policy;
    std::size_t _cube_chan_dim_step;
  public:
    /**
     conv: precomputed convolution FIR of size (conv_support x 2) + 1 + 2 (we need the plus two here for the +/- fraction bits at either side of the support region)
     conv_support: integral half support area for the function
     conv_oversample: integral number of fractional steps per unit support area
     no_polarizations: number of correlations being gridded
     nx,ny: size of image in pixels
     facet_id: index of facet currently being gridded
    */
    convolution_policy(std::size_t nx, std::size_t ny, std::size_t no_polarizations, std::size_t convolution_support, std::size_t oversampling_factor, 
		       const convolution_base_type * conv, gridding_policy_type & active_gridding_policy):
			_nx(nx), _ny(ny), _grid_size_in_pixels(nx*ny), _grid_u_centre(nx / 2), _grid_v_centre(ny / 2),
			_convolution_support(convolution_support*2 + 1), _oversampling_factor(oversampling_factor), 
			_conv(conv), _conv_dim_size(_convolution_support * _oversampling_factor),
			_conv_centre_offset((_convolution_support + 2)/2.0),
			_active_gridding_policy(active_gridding_policy),
			_cube_chan_dim_step(nx*ny*no_polarizations)
			{}
    inline convolution_base_type convolve(const uvw_coord<uvw_base_type> & __restrict__ uvw,
					  const typename gridding_policy_type::trait_type::pol_vis_type & __restrict__ vis,
					  std::size_t no_grids_to_offset,
					  size_t facet_id) const __restrict__ {
	std::size_t chan_offset = no_grids_to_offset * _cube_chan_dim_step;

	uvw_base_type translated_grid_u = uvw._u + _grid_u_centre - _conv_centre_offset;
	uvw_base_type translated_grid_v = uvw._v + _grid_v_centre - _conv_centre_offset;
	std::size_t  disc_grid_u = std::lrint(translated_grid_u);
	std::size_t  disc_grid_v = std::lrint(translated_grid_v);
	//to reduce the interpolation error we need to take the offset from the grid centre into account when choosing a convolution weight
	uvw_base_type frac_u = -translated_grid_u + (uvw_base_type)disc_grid_u;
	uvw_base_type frac_v = -translated_grid_v + (uvw_base_type)disc_grid_v;
	//don't you dare go near the edge!
	if (disc_grid_v + _convolution_support  >= _ny || disc_grid_u + _convolution_support  >= _nx ||
	  disc_grid_v >= _ny || disc_grid_u >= _nx) return 0;
	convolution_base_type accum = 0;
        std::size_t conv_v = (frac_v + 1) * _oversampling_factor;
	std::size_t  convolved_grid_v = (disc_grid_v + 1)*_ny;
        for (std::size_t  sup_v = 1; sup_v <= _convolution_support; ++sup_v) { //remember we have a +/- frac at both ends of the filter
	  convolution_base_type conv_v_weight = _conv[conv_v];
	  std::size_t conv_u = (frac_u + 1) * _oversampling_factor;
	  for (std::size_t sup_u = 1; sup_u <= _convolution_support; ++sup_u) { //remember we have a +/- frac at both ends of the filter
	    std::size_t convolved_grid_u = disc_grid_u + sup_u;
	    convolution_base_type conv_u_weight = _conv[conv_u];
	    std::size_t grid_flat_index = convolved_grid_v + convolved_grid_u;

	    convolution_base_type conv_weight = conv_u_weight * conv_v_weight;
	    _active_gridding_policy.grid_polarization_terms(chan_offset + grid_flat_index, vis, 
							    conv_weight);
	    accum += conv_weight;
	    conv_u += _oversampling_factor;
	  }
	  conv_v += _oversampling_factor;
	  convolved_grid_v += _ny;
        }
        return accum;
    }
  };
  
  /**
   * Nearest neighbour gridding
   */
  template <typename convolution_base_type, typename uvw_base_type, typename grid_base_type, typename gridding_policy_type>
  class convolution_policy <convolution_base_type, uvw_base_type, grid_base_type, gridding_policy_type, convolution_nn> {
  private:
    std::size_t _nx;
    std::size_t _ny;
    std::size_t _grid_size_in_pixels;
    uvw_base_type _grid_u_centre;
    uvw_base_type _grid_v_centre;
    std::size_t _convolution_support;
    std::size_t _oversampling_factor;
    const convolution_base_type * __restrict__ _conv;
    std::size_t _conv_dim_size;
    uvw_base_type _conv_centre_offset;
    gridding_policy_type & __restrict__ _active_gridding_policy;
    std::size_t _cube_chan_dim_step;
  public:
    /**
     conv: not in use for this policy
     conv_support: not in use for this policy
     conv_oversample: not in use for this policy
     polarization_index: index of the polarization correlation term currently being gridded
    */
    convolution_policy(std::size_t nx, std::size_t ny, std::size_t no_polarizations, std::size_t convolution_support, std::size_t oversampling_factor, 
		       const convolution_base_type * conv, gridding_policy_type & active_gridding_policy, size_t facet_id):
			_nx(nx), _ny(ny), _grid_size_in_pixels(nx*ny), _grid_u_centre(nx / 2), _grid_v_centre(ny / 2),
			_convolution_support(convolution_support*2 + 1), _oversampling_factor(oversampling_factor), 
			_conv(conv), _conv_dim_size(_convolution_support * _oversampling_factor),
			_conv_centre_offset((_convolution_support + 2)/2.0),
			_active_gridding_policy(active_gridding_policy),
			_cube_chan_dim_step(nx*ny*no_polarizations)
			{}
    inline convolution_base_type convolve(const uvw_coord<uvw_base_type> & __restrict__ uvw,
					  const typename gridding_policy_type::trait_type::pol_vis_type & __restrict__ vis,
					  std::size_t no_grids_to_offset,
					  size_t facet_id) const __restrict__ {
	std::size_t chan_offset = no_grids_to_offset * _cube_chan_dim_step;

	uvw_base_type translated_grid_u = uvw._u + _grid_u_centre;
	uvw_base_type translated_grid_v = uvw._v + _grid_v_centre;
	std::size_t  disc_grid_u = std::lrint(translated_grid_u);
	std::size_t  disc_grid_v = std::lrint(translated_grid_v);
	//to reduce the interpolation error we need to take the offset from the grid centre into account when choosing a convolution weight
	uvw_base_type frac_u = -translated_grid_u + (uvw_base_type)disc_grid_u;
	uvw_base_type frac_v = -translated_grid_v + (uvw_base_type)disc_grid_v;
	//don't you dare go near the edge!
	if (disc_grid_v + _convolution_support  >= _ny || disc_grid_u + _convolution_support  >= _nx ||
	  disc_grid_v >= _ny || disc_grid_u >= _nx) return 0;
	
	std::size_t grid_flat_index = disc_grid_v*_ny + disc_grid_u;
	_active_gridding_policy.grid_polarization_terms(chan_offset + grid_flat_index, vis, 1);
	return 1;
    }
  };
}