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
#include "gridding_parameters.h"
#include "cu_common.h"
#include "cu_vec.h"
#include "cu_basic_complex.h"
#include "uvw_coord.h"

namespace imaging {
  class disable_faceting_phase_shift {};
  class enable_faceting_phase_shift {};
  struct lmn_coord {uvw_base_type _l; uvw_base_type _m; uvw_base_type _n;};
  template <typename T>
  class phase_transform_policy{
  public:
    __device__ __host__ static void read_facet_ra_dec(const gridding_parameters & params, size_t facet_index, uvw_base_type & facet_ra, uvw_base_type & facet_dec);
    __device__ __host__ static void compute_delta_lmn(uvw_base_type old_phase_centre_ra, uvw_base_type old_phase_centre_dec,
						      uvw_base_type new_phase_centre_ra, uvw_base_type new_phase_centre_dec,
						      lmn_coord & result);
    __device__ __host__ static void apply_phase_transform(const lmn_coord & delta_lmn, const uvw_coord<uvw_base_type> & uvw, vec1< basic_complex<visibility_base_type> > & single_correlation);
    __device__ __host__ static void apply_phase_transform(const lmn_coord & delta_lmn, const uvw_coord<uvw_base_type> & uvw, vec2< basic_complex<visibility_base_type> > & duel_correlation);
    __device__ __host__ static void apply_phase_transform(const lmn_coord & delta_lmn, const uvw_coord<uvw_base_type> & uvw, vec4< basic_complex<visibility_base_type> > & quad_correlation);
  };
  template <>
  class phase_transform_policy<disable_faceting_phase_shift>{
  public:
    __device__ __host__ static void read_facet_ra_dec(const gridding_parameters & params, size_t facet_index, uvw_base_type & facet_ra, uvw_base_type & facet_dec){
      facet_ra = params.phase_centre_ra;
      facet_dec = params.phase_centre_dec;
    }
    __device__ __host__ static void compute_delta_lmn(uvw_base_type old_phase_centre_ra, uvw_base_type old_phase_centre_dec,
						      uvw_base_type new_phase_centre_ra, uvw_base_type new_phase_centre_dec,
						      lmn_coord & result){
      //Do nothing, this should get optimized out
    }
    __device__ __host__ static void apply_phase_transform(const lmn_coord & delta_lmn, const uvw_coord<uvw_base_type> & uvw, vec1< basic_complex<visibility_base_type> > & single_correlation){
      //Do nothing, this should get optimized out
    }
    __device__ __host__ static void apply_phase_transform(const lmn_coord & delta_lmn, const uvw_coord<uvw_base_type> & uvw, vec2< basic_complex<visibility_base_type> > & duel_correlation){
      //Do nothing, this should get optimized out
    }
    __device__ __host__ static void apply_phase_transform(const lmn_coord & delta_lmn, const uvw_coord<uvw_base_type> & uvw, vec4< basic_complex<visibility_base_type> > & quad_correlation){
      //Do nothing, this should get optimized out
    }
  };
  template <>
  class phase_transform_policy<enable_faceting_phase_shift>{
  public:
    __device__ __host__ static void read_facet_ra_dec(const gridding_parameters & params, size_t facet_index, uvw_base_type & facet_ra, uvw_base_type & facet_dec){
      size_t facet_centre_index = facet_index << 1;
      facet_ra = params.facet_centres[facet_centre_index];
      facet_dec = params.facet_centres[facet_centre_index + 1];
    }
    __device__ __host__ static void compute_delta_lmn(uvw_base_type old_phase_centre_ra, uvw_base_type old_phase_centre_dec,
						      uvw_base_type new_phase_centre_ra, uvw_base_type new_phase_centre_dec,
						      lmn_coord & result){
      //see phase_transform_policies.h in cpu code for explanation
      uvw_base_type d_ra = (new_phase_centre_ra - old_phase_centre_ra)* ARCSEC_TO_RAD,
		    d_dec = (new_phase_centre_dec - old_phase_centre_dec)* ARCSEC_TO_RAD,
		    c_d_dec = cos(d_dec),
		    s_d_dec = sin(d_dec),
		    s_d_ra = sin(d_ra),
		    c_d_ra = cos(d_ra);
      result._l = -c_d_dec*s_d_ra;
      result._m = -s_d_dec;
      result._n = 1-c_d_dec*c_d_ra;
    }
    __device__ __host__ static void apply_phase_transform(const lmn_coord & delta_lmn, const uvw_coord<uvw_base_type> & uvw, vec1< basic_complex<visibility_base_type> > & single_correlation){
      uvw_base_type x = 2 * M_PI * (uvw._u * delta_lmn._l + uvw._v * delta_lmn._m + uvw._w * delta_lmn._n); //as in Perley & Cornwell (1992)
      basic_complex<visibility_base_type> phase_shift_term(cos(x),sin(x)); //by Euler's identity
      single_correlation._x *= phase_shift_term;      
    }
    __device__ __host__ static void apply_phase_transform(const lmn_coord & delta_lmn, const uvw_coord<uvw_base_type> & uvw, vec2< basic_complex<visibility_base_type> >& duel_correlation){
      uvw_base_type x = 2 * M_PI * (uvw._u * delta_lmn._l + uvw._v * delta_lmn._m + uvw._w * delta_lmn._n); //as in Perley & Cornwell (1992)
      basic_complex<visibility_base_type> phase_shift_term(cos(x),sin(x)); //by Euler's identity
      duel_correlation._x *= phase_shift_term;
      duel_correlation._y *= phase_shift_term;
    }
    __device__ __host__ static void apply_phase_transform(const lmn_coord & delta_lmn, const uvw_coord<uvw_base_type> & uvw, vec4< basic_complex<visibility_base_type> > & quad_correlation){
      uvw_base_type x = 2 * M_PI * (uvw._u * delta_lmn._l + uvw._v * delta_lmn._m + uvw._w * delta_lmn._n); //as in Perley & Cornwell (1992)
      basic_complex<visibility_base_type> phase_shift_term(cos(x),sin(x)); //by Euler's identity
      quad_correlation._x *= phase_shift_term;
      quad_correlation._y *= phase_shift_term;
      quad_correlation._z *= phase_shift_term;
      quad_correlation._w *= phase_shift_term;
    }
  };
}