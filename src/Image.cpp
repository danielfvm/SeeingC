#include "Image.hpp"
#include <fitsio.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <tiffio.h>

ErrorCode Image::open_tiff(const std::string &filepath) {
  TIFF *tif = TIFFOpen(filepath.c_str(), "r");
  if (tif == NULL) {
    return IMAGE_FILE_ERROR;
  }

  TIFFRGBAImage img;
  char emsg[1024];

  if (TIFFRGBAImageBegin(&img, tif, 0, emsg)) {
    size_t npixels;
    uint32_t *raster;

    m_height = img.height;
    m_width = img.width;

    npixels = img.width * img.height;
    raster = (uint32_t *)_TIFFmalloc(npixels * sizeof(uint32_t));
    if (raster != NULL) {
      if (TIFFRGBAImageGet(&img, raster, img.width, img.height)) {
        m_buffer = (uint8_t *)malloc(npixels);

        for (int x = 0; x < m_width; ++ x) {
          for (int y = 0; y < m_height; ++ y) {
            m_buffer[x+y*m_width] = to_grayscale(raster[x+(m_height-1-y)*m_width]); // copy + invert y
          }
        }
      }
      _TIFFfree(raster);
    } else {
      return IMAGE_ALLOC_ERROR;
    }
    TIFFRGBAImageEnd(&img);
  } else {
    TIFFError(filepath.c_str(), "%s", emsg);
    return IMAGE_OTHER_ERROR;
  }

  TIFFClose(tif);

  return IMAGE_SUCCESS;
}

ErrorCode Image::save_tiff(const std::string &filepath) const {
  TIFF *output_image;
  int bits;

  // Open the TIFF file
  if ((output_image = TIFFOpen(filepath.c_str(), "w")) == NULL) {
    std::cerr << "Unable to write tif file: " << filepath << std::endl;
    return IMAGE_FILE_ERROR;
  }

  // We need to set some values for basic tags before we can add any data
  TIFFSetField(output_image, TIFFTAG_IMAGEWIDTH, m_width);
  TIFFSetField(output_image, TIFFTAG_IMAGELENGTH, m_height);
  TIFFSetField(output_image, TIFFTAG_BITSPERSAMPLE, 8);
  TIFFSetField(output_image, TIFFTAG_SAMPLESPERPIXEL, 1);
  TIFFSetField(output_image, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

  TIFFSetField(output_image, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
  TIFFSetField(output_image, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);

  // Write the information to the file
  TIFFWriteEncodedStrip(output_image, 0, m_buffer, get_pixel_count());

  // Close the file
  TIFFClose(output_image);

  return IMAGE_SUCCESS;
}

uint8_t Image::to_grayscale(int rgba) const {
  int r = (rgba >> 16) & 0xFF;
  int g = (rgba >> 8) & 0xFF;
  int b = (rgba)&0xFF;

  return 0.2989 * r + 0.5870 * g + 0.1140 * b;
}

void Image::get_subarea(Image &img, int sx, int sy, int width,
                        int height) const {
  img.set(width, height);

  for (int x = 0; x < width; ++x) {
    for (int y = 0; y < height; ++y) {
      img.get_pixel(x, y) = get_pixel(sx + x, sy + y);
    }
  }
}

void Image::set(int width, int height) {
  if (m_buffer) {
    free(m_buffer);
  }

  m_width = width;
  m_height = height;
  m_buffer = (uint8_t *)malloc(get_pixel_count());
}

void Image::set(int width, int height, uint8_t *buffer) {
  if (m_buffer) {
    free(m_buffer);
  }

  m_width = width;
  m_height = height;
  m_buffer = buffer;
}

uint8_t Image::get_pixel(int x, int y) const {
  return m_buffer[y * m_width + x];
}

uint8_t &Image::get_pixel(int x, int y) { return m_buffer[y * m_width + x]; }

std::string Image::get_encoded_str() {
  m_build_buffer.clear();
  stbi_write_jpg_to_func((stbi_write_func *)write_func, this, m_width, m_height,
                         1, m_buffer, 100);
  return m_build_buffer;
}

void Image::write_func(Image *img, void *data, int size) {
  for (int i = 0; i < size; ++i) {
    img->m_build_buffer += ((char *)data)[i];
  }
}

void Image::save_fits(const char *filename) {
  fitsfile *fptr; /* pointer to the FITS file; defined in fitsio.h */
  int status, ii, jj;
  long fpixel = 1, naxis = 2, nelements, exposure;
  long naxes[2] = {m_width, m_height}; /* image is 300 pixels wide by 200 rows */
  //short* array = (short*)malloc(size_t size);
  status = 0; /* initialize status before calling fitsio routines */

  fits_create_file(&fptr, filename, &status); /* create new file
                                               */
  /* Create the primary array image (16-bit short integer pixels
   */
  fits_create_img(fptr, SHORT_IMG, naxis, naxes, &status);

  /* Write a keyword; must pass the ADDRESS of the value */
  // exposure = 1500.;
  // fits_update_key(fptr, TLONG, "EXPOSURE", &exposure, "Total Exposure Time ", &status);

  /* Initialize the values in the image with a linear ramp function */ 
  //for (jj = 0; jj < naxes[1]; jj++) for (ii = 0; ii < naxes[0]; ii++)
   // array[jj][ii] = m_buffer[jj * m_width + ii];

  nelements = naxes[0] * naxes[1]; /* number of pixels to write */

  /* Write the array of integers to the image */
  fits_write_img(fptr, TBYTE, fpixel, nelements, m_buffer, &status);
  fits_close_file(fptr, &status);    /* close the file */
  fits_report_error(stderr, status); /* print out any error messages */
}
