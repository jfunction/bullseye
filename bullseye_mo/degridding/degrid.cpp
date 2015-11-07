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
#include <string>
#include <cstdio>
#include <casa/Quanta/Quantum.h>
#include <numeric>
#include <fftw3.h>
#include <typeinfo>
#include <thread>
#include <future>

#include "omp.h"

#include "wrapper.h"
#include "timer.h"
#include "uvw_coord.h"
//#include "templated_gridder.h"
#include "fft_and_repacking_routines.h"
extern "C" {
    void degrid(gridding_parameters & params) {
        printf("Degridding data... Hooray...\n");
//        for (size_t row = 0; row < params.row_count; ++row) {
//            printf(params.gridded_data);
//        }
    }
}
