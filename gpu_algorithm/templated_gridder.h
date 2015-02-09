#pragma once

#include "cu_common.h"
#include "gridding_parameters.h"

namespace imaging {
	/*
		This is a gridding kernel following Romeins distribution stategy.
		This should be launched with 
			block dimensions: {THREADS_PER_BLOCK,1,1}
			blocks per grid: {minimum number of blocks required to run baselines*conv_support_size^^2 threads,
					  1,1}
	 */
	template <typename active_correlation_gridding_policy>
	__global__ void templated_gridder(gridding_parameters params){
		size_t tid = cu_indexing_schemes::getGlobalIdx_1D_1D(gridDim,blockIdx,blockDim,threadIdx);
		size_t conv_full_support = (params.conv_support << 1) + 1;
		size_t conv_full_support_sq = conv_full_support * conv_full_support;
		size_t padded_conv_full_support = conv_full_support + 2; //remember we need to reserve some of the support for +/- frac on both sides
		if (tid >= params.baseline_count * conv_full_support_sq) return;
		size_t my_baseline = tid / conv_full_support_sq;
		size_t conv_theadid_flat_index = tid % conv_full_support_sq;
		size_t my_conv_v = (conv_theadid_flat_index / conv_full_support) + 1;
		size_t my_conv_u = (conv_theadid_flat_index % conv_full_support) + 1;
		
		size_t starting_row_index = params.baseline_starting_indexes[my_baseline];
		//the starting index prescan must be n(n-1)/2 + n + 1 elements long since we need the length of the last baseline
		size_t baseline_num_timestamps = params.baseline_starting_indexes[my_baseline+1] - starting_row_index;
		
		//Scale the IFFT by the simularity theorem to the correct FOV
		uvw_base_type u_scale=params.nx*params.cell_size_x * ARCSEC_TO_RAD;
		uvw_base_type v_scale=-(params.ny*params.cell_size_y * ARCSEC_TO_RAD);
		
		uvw_base_type conv_offset = (padded_conv_full_support) / 2.0; 
		uvw_base_type grid_centre_offset_x = params.nx/2.0 - conv_offset + my_conv_u;
		uvw_base_type grid_centre_offset_y = params.ny/2.0 - conv_offset + my_conv_v;
		size_t grid_size_in_floats = params.nx * params.ny << 1;
		
		//load the convolution filter into shared memory
		extern __shared__ convolution_base_type shared_conv[];
		if (threadIdx.x == 0){
		  size_t fir_ubound = ((params.conv_oversample * padded_conv_full_support));
		  
		  for (size_t x = 0; x < fir_ubound; ++x){
		    shared_conv[x] = params.conv[x];
		  }
		}
		__syncthreads(); //wait for the first thread to put the entire filter into shared memory
		
// 		size_t channel_loop_ubound = params.channel_count >> 1;
// 		size_t channel_loop_rem_lbound = channel_loop_ubound << 1;
		//we must keep seperate accumulators per channel, so we need to bring these loops outward (contrary to Romein's paper)
		{
		    typename active_correlation_gridding_policy::active_trait::accumulator_type my_grid_accum;
		    for (size_t c = 0; c < params.channel_count; ++c){	
			my_grid_accum = active_correlation_gridding_policy::active_trait::vis_type::zero();
			size_t my_previous_u = 0;
			size_t my_previous_v = 0;
			size_t my_previous_spw = 0;
			for (size_t t = 0; t < baseline_num_timestamps; ++t){
				size_t row = starting_row_index + t;
				size_t spw = params.spw_index_array[row];
				//read all the stuff that is only dependent on the current spw and channel
				size_t flat_indexed_spw_channel = spw * params.channel_count + c;
				bool channel_enabled = params.enabled_channels[flat_indexed_spw_channel];
				size_t channel_grid_index = params.channel_grid_indicies[flat_indexed_spw_channel];
				reference_wavelengths_base_type ref_wavelength = 1 / params.reference_wavelengths[flat_indexed_spw_channel];
				//read all the data we need for gridding
				imaging::uvw_coord<uvw_base_type> uvw = params.uvw_coords[row];
				bool row_flagged = params.flagged_rows[row];
				bool row_is_in_field_being_imaged = (params.field_array[row] == params.imaging_field);
				typename active_correlation_gridding_policy::active_trait::vis_type vis;
				typename active_correlation_gridding_policy::active_trait::vis_weight_type vis_weight;
				typename active_correlation_gridding_policy::active_trait::vis_flag_type visibility_flagged;
				active_correlation_gridding_policy::read_corralation_data(params,row,spw,c,vis,visibility_flagged,vis_weight);
				//compute the weighted visibility and promote the flags to integers so that we don't have unnecessary branch diversion here
				typename active_correlation_gridding_policy::active_trait::vis_flag_type vis_flagged = !(visibility_flagged || row_flagged) && 
														       channel_enabled && row_is_in_field_being_imaged;
 				typename active_correlation_gridding_policy::active_trait::vis_weight_type combined_vis_weight = 
					 vis_weight * vector_promotion<int,visibility_base_type>(vector_promotion<bool,int>(vis_flagged));
				//scale the uv coordinates (measured in wavelengths) to the correct FOV by the fourier simularity theorem (pg 146-148 Synthesis Imaging in Radio Astronomy II)
				uvw._u *= u_scale * ref_wavelength; 
				uvw._v *= v_scale * ref_wavelength;
				//account for interpolation error (we select the closest sample from the oversampled convolution filter)
				uvw_base_type cont_current_u = uvw._u + grid_centre_offset_x;
				uvw_base_type cont_current_v = uvw._v + grid_centre_offset_y;
				size_t my_current_u = rintf(cont_current_u);
				size_t my_current_v = rintf(cont_current_v);
				size_t frac_u = (-cont_current_u + (uvw_base_type)my_current_u) * params.conv_oversample;
				size_t frac_v = (-cont_current_v + (uvw_base_type)my_current_v) * params.conv_oversample;
				//map the convolution memory access to a coalesced access (bundle #full_support number of fractions together, so that the memory addresses are contigious)
				size_t closest_conv_u = frac_u * padded_conv_full_support + my_conv_u;
				size_t closest_conv_v = frac_v * padded_conv_full_support + my_conv_v;
				//if this is the first timestamp for this baseline initialize previous_u and previous_v
				if (t == 0) {
					my_previous_u = my_current_u;
					my_previous_v = my_current_v;
					my_previous_spw = spw;
				}
				//if u and v have changed we must dump everything to memory at previous_u and previous_v and reset
				if ((my_current_u != my_previous_u || my_current_v != my_previous_v || my_previous_spw != spw) && channel_enabled){
					//don't you dare go off the grid:
					if (my_previous_v + conv_full_support  < params.ny && my_previous_u + conv_full_support  < params.nx &&
					    my_previous_v < params.ny && my_previous_u < params.nx){
						active_correlation_gridding_policy::grid_visibility((grid_base_type*)params.output_buffer,
												    grid_size_in_floats,
												    params.nx,
												    channel_grid_index,
												    params.number_of_polarization_terms_being_gridded,
												    my_previous_u,
												    my_previous_v,
												    my_grid_accum
												   );
					}
					my_grid_accum = active_correlation_gridding_policy::active_trait::vis_type::zero();
					my_previous_u = my_current_u;
					my_previous_v = my_current_v;
				}
				//Lets read the convolution weights from the the precomputed filter
				convolution_base_type conv_weight = shared_conv[closest_conv_u] * shared_conv[closest_conv_v];
				//then multiply-add into the accumulator 				
				my_grid_accum += vis * (combined_vis_weight * conv_weight);
				
				//Okay if this is the last channel then dump whatever has been accumulated since the last dump
				if (channel_enabled && t == baseline_num_timestamps-1){
				    if (my_previous_u + conv_full_support < params.ny && my_previous_u + conv_full_support  < params.nx &&
					my_previous_v < params.ny && my_previous_u < params.nx){
					active_correlation_gridding_policy::grid_visibility((grid_base_type*)params.output_buffer,
												    grid_size_in_floats,
												    params.nx,
												    channel_grid_index,
												    params.number_of_polarization_terms_being_gridded,
												    my_previous_u,
												    my_previous_v,
												    my_grid_accum
											   );
				    }
				}
			}
		    }
		}
	}
}