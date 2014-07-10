#!/usr/bin/python
import sys
import argparse
import numpy as np
import pylab
import re
from pyrap.quanta import quantity

from helpers import data_set_loader
from helpers import fft_utils
from helpers import convolution_filter
from helpers import fits_export
import ctypes
libimaging = ctypes.pydll.LoadLibrary("build/algorithms/libimaging.so")

	   
def coords(s):  
    try:
	sT = s.strip()
        ra, dec = map(float, sT[1:len(sT)-1].split(','))
        return ra, dec
    except:
        raise argparse.ArgumentTypeError("Coordinates must be ra,dec tupples")

if __name__ == "__main__":
  parser = argparse.ArgumentParser(description='Bullseye: An implementation of targetted facet-based synthesis imaging in radio astronomy.')
  pol_options = {'I': 4,'Q': 5,'U': 6,'V': 7,'XX': 0,'XY': 1,'YX': 2,'YY': 3} #if the user selects XX ,XY, YX or YY we only have to compute a single grid
  parser.add_argument('input_ms', help='Name of the measurement set to read', type=str)
  parser.add_argument('output_prefix', help='Prefix for the output FITS images. Facets will be indexed as [prefix_1.fits ... prefix_n.fits]', type=str)
  parser.add_argument('--facet_centres', help='List of coordinate tupples indicating facet centres (RA,DEC). If none present default pointing centre will be used', type=coords, nargs='+', default=None)
  parser.add_argument('--npix_l', help='Number of facet pixels in l', type=int, default=256)
  parser.add_argument('--npix_m', help='Number of facet pixels in m', type=int, default=256)
  parser.add_argument('--cell_l', help='Size of a pixel in l (arcsecond)', type=int, default=1)
  parser.add_argument('--cell_m', help='Size of a pixel in l (arcsecond)', type=int, default=1)
  parser.add_argument('--pol', help='Specify image polarization', choices=pol_options.keys(), default="XX")
  parser.add_argument('--conv', help='Specify gridding convolution function type', choices=['gausian','keiser bessel'], default='keiser_bessel')
  parser.add_argument('--conv_sup', help='Specify gridding convolution function support area (number of grid cells)', type=int, default=1)
  parser.add_argument('--conv_oversamp', help='Specify gridding convolution function oversampling multiplier', type=int, default=1)
  parser.add_argument('--output_format', help='Specify image output format', choices=["fits","png"], default="fits")
  parser_args = vars(parser.parse_args())
  data = data_set_loader.data_set_loader(parser_args['input_ms'])
  #some sanity checks:
  if pol_options[parser_args['pol']] > 3 and not data._no_polarization_correlations == 4:
    raise argparse.ArgumentTypeError("Cannot image polarization '%s'. Option only avaliable when data contains 4 correlation terms per visibility" % parser_args[pol])
    
  
  conv = convolution_filter.convolution_filter(parser_args['conv_sup'],parser_args['conv_sup'],
					       parser_args['conv_oversamp'],parser_args['npix_l'],
					       parser_args['npix_m'],parser_args['conv'])
  facet_centres = None
  
  num_facet_centres = 0
  if (parser_args['facet_centres'] != None):
    num_facet_centres = len(parser_args['facet_centres'])
    facet_centres = np.array(parser_args['facet_centres']).astype(np.float32)
  gridded_vis = None
  if pol_options[parser_args['pol']] < 3: 
    num_grids = 1 if (num_facet_centres == 0) else num_facet_centres
    g = np.zeros([num_grids,1,parser_args['npix_l'],parser_args['npix_m']],dtype=np.complex64)
    #no need to grid more than one of the correlations if the user isn't interrested in imaging one of the stokes terms (I,Q,U,V):
    libimaging.grid_single_pol(data._arr_data.ctypes.data_as(ctypes.c_void_p),
			       data._arr_uvw.ctypes.data_as(ctypes.c_void_p),
			       ctypes.c_size_t(data._no_timestamps),ctypes.c_size_t(data._no_baselines),
			       ctypes.c_size_t(data._no_channels),ctypes.c_size_t(data._no_polarization_correlations),
			       data._chan_wavelengths.ctypes.data_as(ctypes.c_void_p),
			       data._arr_flaged.ctypes.data_as(ctypes.c_void_p),
			       data._arr_flagged_rows.ctypes.data_as(ctypes.c_void_p),
			       data._arr_weights.ctypes.data_as(ctypes.c_void_p),
			       ctypes.c_size_t(parser_args['npix_l']),
			       ctypes.c_size_t(parser_args['npix_m']),
			       ctypes.c_float(parser_args['cell_l']),
			       ctypes.c_float(parser_args['cell_m']),
			       ctypes.c_float(data._phase_centre[0,0]),
			       ctypes.c_float(data._phase_centre[0,1]),
			       facet_centres.ctypes.data_as(ctypes.c_void_p) if (num_facet_centres != 0) else None, 
			       ctypes.c_size_t(num_facet_centres), 
			       conv._conv_FIR.ctypes.data_as(ctypes.c_void_p),
			       ctypes.c_size_t(parser_args['conv_sup']),
			       ctypes.c_size_t(parser_args['conv_oversamp']),
			       ctypes.c_size_t(pol_options[parser_args['pol']]),
			       g.ctypes.data_as(ctypes.c_void_p))
    gridded_vis = g[:,0,:,:]
  else:
    num_polarized_grids = 1 if (num_facet_centres == 0) else num_facet_centres
    g = np.zeros([num_polarized_grids,4,parser_args['npix_l'],parser_args['npix_m']],dtype=np.complex64)
   
    libimaging.grid_4_cor(data._arr_data.ctypes.data_as(ctypes.c_void_p),
			  data._arr_uvw.ctypes.data_as(ctypes.c_void_p),
			  ctypes.c_size_t(data._no_timestamps),ctypes.c_size_t(data._no_baselines),
			  ctypes.c_size_t(data._no_channels),ctypes.c_size_t(data._no_polarization_correlations),
			  data._chan_wavelengths.ctypes.data_as(ctypes.c_void_p),
			  data._arr_flaged.ctypes.data_as(ctypes.c_void_p),
			  data._arr_flagged_rows.ctypes.data_as(ctypes.c_void_p),
			  data._arr_weights.ctypes.data_as(ctypes.c_void_p),
			  ctypes.c_size_t(parser_args['npix_l']),
			  ctypes.c_size_t(parser_args['npix_m']),
			  ctypes.c_float(parser_args['cell_l']),
			  ctypes.c_float(parser_args['cell_m']),
			  ctypes.c_float(data._phase_centre[0,0]),
			  ctypes.c_float(data._phase_centre[0,1]),
			  facet_centres.ctypes.data_as(ctypes.c_void_p) if (num_facet_centres != 0) else None, 
			  ctypes.c_size_t(num_facet_centres), 
			  conv._conv_FIR.ctypes.data_as(ctypes.c_void_p),
			  ctypes.c_size_t(parser_args['conv_sup']),
			  ctypes.c_size_t(parser_args['conv_oversamp']),
			  g.ctypes.data_as(ctypes.c_void_p))  
    '''
    See Smirnov I for description on conversion between correlation terms and stokes params for linearly polarized feeds
    '''
    if parser_args['pol'] == "I":
      gridded_vis = ((g[:,0,:,:] + g[:,3,:,:])).reshape(num_polarized_grids,parser_args['npix_l'],parser_args['npix_m'])
    elif parser_args['pol'] == "V":
      gridded_vis = ((g[:,1,:,:] - g[:,2,:,:])).reshape(num_polarized_grids,parser_args['npix_l'],parser_args['npix_m'])
    elif parser_args['pol'] == "Q":
      gridded_vis = ((g[:,0,:,:] - g[:,3,:,:])/1.0j).reshape(num_polarized_grids,parser_args['npix_l'],parser_args['npix_m'])
    elif parser_args['pol'] == "U":
      gridded_vis = ((g[:,1,:,:] - g[:,2,:,:])).reshape(num_polarized_grids,parser_args['npix_l'],parser_args['npix_m'])
  #now invert, detaper and write out all the facets to disk:  
  if parser_args['facet_centres'] == None:
    dirty = np.real(fft_utils.ifft2(gridded_vis[0,:,:]))/conv._F_detaper
    if parser_args['output_format'] == 'png':
      i = pylab.imshow(dirty[::-1,:],interpolation='nearest',cmap = pylab.get_cmap('hot'),
		       extent=[0, parser_args['npix_l']-1, 0, parser_args['npix_m']-1])
      i.write_png(parser_args['output_prefix']+'.png',noscale=True)
      pylab.close('all')
    else:
      fits_export.save_to_fits_image(parser_args['output_prefix']+'.fits',
				     parser_args['npix_l'],parser_args['npix_m'],
				     quantity(parser_args['cell_l'],'arcsec'),quantity(parser_args['cell_m'],'arcsec'),
				     quantity(data._phase_centre[0,0],'arcsec'),quantity(data._phase_centre[0,1],'arcsec'),
				     parser_args['pol'],
				     float(data._epoch[1:]) if not data._epoch[0] in range(ord('0'),ord('9')) else float(data._epoch),
				     dirty)
  else:
    for f in range(0, num_facet_centres):
      dirty = (np.real(fft_utils.ifft2(gridded_vis[f,:,:]))/conv._F_detaper).reshape(parser_args['npix_l'],parser_args['npix_m'])
      if parser_args['output_format'] == 'png':
	i = pylab.imshow(dirty[::-1,:],interpolation='nearest',cmap = pylab.get_cmap('hot'),
		       extent=[0, parser_args['npix_l']-1, 0, parser_args['npix_m']-1])
	i.write_png(parser_args['output_prefix']+str(f)+'.png',noscale=True)
	pylab.close('all')
      else:
	fits_export.save_to_fits_image(parser_args['output_prefix']+str(f)+'.fits',
				       parser_args['npix_l'],parser_args['npix_m'],
				       quantity(parser_args['cell_l'],'arcsec'),quantity(parser_args['cell_m'],'arcsec'),
				       quantity(facet_centres[0,0],'arcsec'),quantity(facet_centres[0,1],'arcsec'),
				       parser_args['pol'],
				       float(data._epoch[1:]) if not data._epoch[0] in range(ord('0'),ord('9')) else float(data._epoch),
				       dirty)