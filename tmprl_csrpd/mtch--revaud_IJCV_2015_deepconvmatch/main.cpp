/*
Copyright (C) 2014 Jerome Revaud

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>
*/
#include "std.h"
#include "image.h"
#include "io.h"
#include "deep_matching.h"
#include <thread>

void usage()
{
  #define p(msg)  printf(msg "\n");
  p("usage:");
  p("./deepmatching image1 image2 [options]");
  p("./deepmatching -i video [options]");
  p("Compute the 'DeepMatching' between two images and print a list of")
  p("pair-wise point correspondences:")
  p("  x1 y1 x2 y2 score index ...")
  p("(index refers to the local maximum from which the match was retrieved)")
  p("Images must be in PPM, PNG or JPG format. Version 1.1");
  p("")
  p("Options:")
  p("    -h, --help                 print this message")
  p("  HOG parameters (low-level pixel descriptor):")
//p("    -png_settings              (auto) recommended for uncompressed images")
//p("    -jpg_settings              (auto) recommended for compressed images")
//p("   in more details: (for fine-tuning)")
  p("    -hog.presm <f=1.0>         prior image smoothing")
  p("    -hog.midsm <f=1.5>         intermediate HOG smoothing")
  p("    -hog.sig <f=0.2>           sigmoid strength")
  p("    -hog.postsm <f=1.0>        final HOG-smoothing")
  p("    -hog.ninth <f=0.3>         robustness to pixel noise (eg. JPEG artifacts)")
  p("")
  p("  Matching parameters:")
//p("    -iccv_settings             settings used for the ICCV paper")
//p("    -improved_settings         (default) supposedly improved settings")
//p("   in more details: (for fine-tuning)")
  p("    -downscale <n=1>           downscale the input images by 2^n")
//p("    -overlap <n=999>           use overlapping patches in image1 from level n")
//p("    -subref <n=0>              0: denser sampling or 1: not of image1 patches")
  p("    -ngh_rad <n=0>             restrict matching to a neighborhood of n pixels (0=no)")
  p("    -nlpow <f=1.6>             non-linear rectification x := x^f")
//p("    -maxima_mode <n=0>         0: from all top cells / 1: from local maxima")
//p("    -min_level <n=2>           skip maxima in levels [0, 1, ..., n-1]")
  p("    -mem <n=1>                 use optimization to reduce memory footprint if !=0")
//p("    -scoring_mode <n=1>        type of correspondence scoring mode (0/1)")
  p("")
  p("  Fully scale & rotation invariant DeepMatching:")
  p("    if either one of these options is used, then this mode is used:")
  p("    -max_scale <factor=5>         max scaling factor")
  p("    -rot_range <from=0> <to=360>  rotation range")
  p("")
  p("  Other parameters:")
  p("    -resize <width> <height>   works internally with resized images")
  p("    -v                         increase verbosity")
  p("    -nt <n>                    multi-threading with <n> threads")
  p("    -out <file_name>           output correspondences in a file")
  p("    -b                         writes a binary output file")
  p("    -k <# skip>                number of frames to skip while processing video")
  exit(1);
}

bool endswith(const char *str, const char *suffix)
{
    if(!str || !suffix)  return false;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if(lensuffix >  lenstr)  return false;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

image_t* rescale_image( image_t* im, int width, int height ) 
{
  image_t* res = image_new(width,height);
  image_resize_bilinear_newsize(res, im, width, height);
  image_delete(im);
  return res;
}


float_image* compute_match_ims(image_t *im1, image_t *im2, dm_params_t& params, scalerot_params_t& sr_params, bool& use_scalerot) 
{
  if( use_scalerot )
    assert( params.ngh_rad == 0 || !"max trans cannot be used in full scale and rotation mode");
  else {
    if( params.subsample_ref && (!ispowerof2(im1->width) || !ispowerof2(im1->height)) ) {
      fprintf(stderr, "WARNING: first image has dimension which are not power-of-2\n");
      fprintf(stderr, "For improved results, you should consider resizing the images with '-resize <w> <h>'\n");
    }
  }

  // compute deep matching
  float_image* corres = use_scalerot ? 
         deep_matching_scale_rot( im1, im2, &params, &sr_params ) : 
         deep_matching          ( im1, im2, &params, NULL );  // standard call
  
  return corres;
}

int main(int argc, const char ** argv)
{
  if( argc<=2 || !strcmp(argv[1],"-h") || !strcmp(argv[1],"--help") )  usage(); 
  
  // load images
  image_t *im1=NULL, *im2=NULL;
  int current_arg = 3;

  // set params to default
  dm_params_t params;
  set_default_dm_params(&params);
  scalerot_params_t sr_params;
  set_default_scalerot_params(&sr_params);
  bool use_scalerot = false;
  const char* out_filename = NULL;
  float fx=1, fy=1;
  bool do_resize = false;
  int rsz_width = -1, rsz_height = -1;
  bool write_binary_file = false;
  int skip_frames = 0;

  // if images given, change parameters
  if (strcmp(argv[1], "-i") != 0)
  {
    if( endswith(argv[1],"png") || endswith(argv[1],"PNG") )
      set_png_settings(&params.desc_params);  // set default
    if( endswith(argv[1],"ppm") || endswith(argv[1],"PPM") )
      set_png_settings(&params.desc_params);  // set default
    if( endswith(argv[1],"jpg") || endswith(argv[1],"JPG") )
      set_jpg_settings(&params.desc_params);  // set default
    if( endswith(argv[1],"jpeg") || endswith(argv[1],"JPEG") )
      set_jpg_settings(&params.desc_params);  // set default
  }

  // parse options
  while(current_arg < argc)
  {
    const char* a = argv[current_arg++];
    #define isarg(key)  !strcmp(a,key)
    
    if(isarg("-h") || isarg("--help") )    usage();
  // HOG and patch parameters
    else if(isarg("-hog.presm"))
      params.desc_params.presmooth_sigma = atof(argv[current_arg++]);
    else if(isarg("-hog.sig"))
      params.desc_params.hog_sigmoid = atof(argv[current_arg++]);
    else if(isarg("-hog.midsm"))
      params.desc_params.mid_smoothing = atof(argv[current_arg++]);
    else if(isarg("-hog.postsm"))
      params.desc_params.post_smoothing = atof(argv[current_arg++]);
    else if(isarg("-hog.ninth"))
      params.desc_params.ninth_dim = atof(argv[current_arg++]);
    else if(isarg("-hog.nrmpix"))
      params.desc_params.norm_pixels = atof(argv[current_arg++]);
    else if(isarg("-png_settings")) { 
      set_png_settings(&params.desc_params);
    } else if(isarg("-jpg_settings")) {
      set_jpg_settings(&params.desc_params);
  // matching parameters
    }else if(isarg("-downscale"))
      params.prior_img_downscale = atoi(argv[current_arg++]);
    //else if(isarg("-overlap"))
    //  params.overlap = atoi(argv[current_arg++]);
    //else if(isarg("-subref"))
    //  params.subsample_ref = atoi(argv[current_arg++]);
    else if(isarg("-nlpow"))
      params.nlpow = atof(argv[current_arg++]);
    else if(isarg("-ngh_rad"))
      params.ngh_rad = atoi(argv[current_arg++]);
  // maxima parameters
    //else if(isarg("-maxima_mode"))
    //  params.maxima_mode = atoi(argv[current_arg++]);
    else if(isarg("-mem")) {
      params.low_mem = atoi(argv[current_arg++]);
    //} else if(isarg("-min_level"))
    //  params.min_level = atoi(argv[current_arg++]);
    //else if(isarg("-scoring_mode"))
    //  params.scoring_mode = atoi(argv[current_arg++]);
    //else if(isarg("-iccv_settings")) {
    //  params.prior_img_downscale = 2;
    //  params.overlap = 0; // overlap from level 0
    //  params.subsample_ref = 1;
    //  params.nlpow = 1.6;
    //  params.maxima_mode = 1;
    //  params.low_mem = 0;
    //  params.min_level = 2;
    //  params.scoring_mode = 0;
    //} else if(isarg("-improved_settings")) {
    //  params.prior_img_downscale = 1; // less down-scale
    //  params.overlap = 999; // no overlap
    //  params.subsample_ref = 0; // dense patch sampling at every level in first image
    //  params.nlpow = 1.4;
    //  params.maxima_mode = 0;
    //  params.low_mem = 1;
    //  params.min_level = 2;
    //  params.scoring_mode = 1;  // improved scoring
    //} else if(isarg("-max_psize")) {
    //  params.max_psize = atoi(argv[current_arg++]);
  // scale & rot invariant version
    } else if(isarg("-scale") || isarg("-max_scale")) {
      use_scalerot = true;
      float scale = atof(argv[current_arg++]);
      sr_params.max_sc0 = sr_params.max_sc1 = int(1 + 2*log2(scale));
    } else if(isarg("-rot") || isarg("-rot_range")) {
      use_scalerot = true;
      int min_rot = atoi(argv[current_arg++]);
      int max_rot = atoi(argv[current_arg++]);
      while( min_rot < 0 ) {
        min_rot += 360;
        max_rot += 360;
      }
      sr_params.min_rot = int(floor(0.5 + min_rot/45.));
      sr_params.max_rot = int(floor(1.5 + max_rot/45.));
      while( sr_params.max_rot - sr_params.min_rot > 8 )  
        sr_params.max_rot--;
      assert( sr_params.min_rot < sr_params.max_rot );
  // other parameters
    }else if(isarg("-resize")) {
      do_resize = true;
      rsz_width = atoi(argv[current_arg++]);
      rsz_height = atoi(argv[current_arg++]);
    }else if(isarg("-v"))
      params.verbose++;
    else if(isarg("-nt")) {
      params.n_thread = atoi(argv[current_arg++]);
      if (params.n_thread==0)
        params.n_thread = std::thread::hardware_concurrency();
    } else if(isarg("-out"))
      out_filename = argv[current_arg++];
    else if(isarg("-b"))
      write_binary_file = true;
    else if(isarg("-k"))
      skip_frames = atoi(argv[current_arg++]);
    else {
      fprintf(stderr,"error: unexpected parameter '%s'", a);
      exit(-1);
    }
  }

  // write to stdout by default
  FILE* out_fd = stdout; 

  // if filename passed, create and open it
  if (out_filename) {
      // open file (in binary if needed)
      if (write_binary_file) {
        out_fd = fopen(out_filename,"wb");
      } else {
        out_fd = fopen(out_filename,"w");
      }
  }

  assert(out_fd != NULL && "Unable to open file for writing");

  // if passed two images
  if (strcmp(argv[1], "-i") != 0)
  {
    {
      color_image_t *cim1 = color_image_load(argv[1]);
      color_image_t *cim2 = color_image_load(argv[2]);

      im1 = image_gray_from_color(cim1);
      im2 = image_gray_from_color(cim2);
      color_image_delete(cim1);
      color_image_delete(cim2);
    }

    if (do_resize) {
      assert(im1->width==im2->width && im1->height==im2->height && 
             "Size of two images should be consistent if resizing");
      fx *= im1->width / float(rsz_width);
      fy *= im1->height / float(rsz_height);
      im1 = rescale_image(im1, rsz_width, rsz_height);
      im2 = rescale_image(im2, rsz_width, rsz_height);
    }

    float_image* corres = compute_match_ims(im1, im2, params, sr_params, use_scalerot);
    // save result
    output_correspondences( out_fd, (corres_t*)corres->pixels, corres->ty, fx, fy, write_binary_file );
    
    free_image(corres);
  } else {
    /* in case a video was passed as input */
    cv::Mat frame1, frame2;

    // try to open the video for reading
    cv::VideoCapture capture;
    capture.open(argv[2]);

    assert(capture.isOpened() && "Could not open the input sequence");

    // get number of frames
    const int num_frames = capture.get(CV_CAP_PROP_FRAME_COUNT);
    // write header (# frames, # skip frames) to file/stdout
    if (write_binary_file) {
      fwrite(&num_frames, sizeof(int), 1, out_fd);
      fwrite(&skip_frames, sizeof(int), 1, out_fd);
    } else {
      fprintf(out_fd, "%d %d\n", num_frames, skip_frames);
    }

    // read the first frame
    capture >> frame1;
    assert(!frame1.empty() && "Was not able to read any frames..\n");

    // convert the first frame to grayscale (also allocated mem for im1)
    im1 = cvmat_to_gray_im(frame1);

    // resize if needed
    if (do_resize) {
      fx *= im1->width / float(rsz_width);
      fy *= im1->height / float(rsz_height);
      // im1's memory would be reallocated
      im1 = rescale_image(im1, rsz_width, rsz_height);
    }

    // allocate memory for im2
    // if resizing, new memory is allocated on each iteration
    if (!do_resize) {
      im2 = image_new(frame1.cols, frame1.rows);
    }

    int frame_no = 1;
    bool keep_running = true;

    // loop till all the frames are process
    while (keep_running) {
      // skip frame as needed
      for (int i=0; i < skip_frames; ++i) {
        capture >> frame2;
        if(frame2.empty()) {
          keep_running = false;
          continue;
        }
        ++frame_no;
      }

      // read the next frames (and compute matches with previous frame)
      capture >> frame2;

      // quit when no more frames to read in video
      if(frame2.empty()) {
        keep_running = false;
        continue;
      }

      fprintf(stderr, "Matching: %d -> %d\n", frame_no-skip_frames-1, frame_no);

      // convert this frame to grayscale as well
      if (do_resize) {
        // if resizing, allocate new memory, because the old im2 has already 
        // been resized to a different sized memory
        im2 = cvmat_to_gray_im(frame2);
      } else {
        // reuse memory
        cvmat_to_gray_im(frame2, im2);
      }

      // resize if needed
      if (do_resize) {
        // im2's memory would be reallocated
        im2 = rescale_image(im2, rsz_width, rsz_height);
      }

      // compute correspendence with previous frame
      float_image* corres = compute_match_ims(im1, im2, params, sr_params, use_scalerot);

      // add frame number
      if (write_binary_file) {
        fwrite(&frame_no, sizeof(int), 1, out_fd);
      } else {
        fprintf(out_fd, "%d\n", frame_no);
      }

      // save result
      output_correspondences( out_fd, (corres_t*)corres->pixels, corres->ty, 
                              fx, fy, write_binary_file );
      
      // swap frames so that we don't need to allocate new memory in next iteration
      image_t *tmp = im1;
      im1 = im2;
      im2 = tmp;

      free_image(corres);

      ++frame_no;
    }

    assert(frame_no == num_frames &&
           "Number of frames read is not consistent with cv::VideoCapture");
  }

  // close file if one was created
  if(out_filename)
    fclose(out_fd);

  image_delete(im1);
  image_delete(im2);

  return 0;
}
