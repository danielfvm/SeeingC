#ifndef UTIL_HPP
#define UTIL_HPP

#include "Image.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <stdarg.h>
#include <memory>
#include <ctime>

#include <tiffio.h>

/*
#include <eigen3/Eigen/Eigen>
#include <eigen3/Eigen/QR>
*/

struct StarInfo {
  int max_x, min_x;
  int max_y, min_y;
  int area;

  static StarInfo zero() { return StarInfo{0, 0, 0, 0, 0}; }

  void reset() {
    max_x = 0;
    min_x = 0;
    max_y = 0;
    min_y = 0;
    area = 0;
  }

  float radius() const { return std::sqrt(area / M_PI); }

  float diameter() const { return radius() * 2; }

  float x() const { return min_x + (max_x - min_x) / 2.0f; }

  float y() const { return min_y + (max_y - min_y) / 2.0f; }

  // The smaller this number, the more symmetrical is our star
  float circleness() const {
    return std::fabs((max_y - min_y) - (max_x - min_x));
  }
};

#define MAX_AREA 20000

inline void scanNeighbours(const Image& img, bool *blacklist, int x, int y, int threshold, StarInfo &info) {
  if (x < 0 || y < 0 || x >= img.get_width() || y >= img.get_height()) {
    return;
  }
  
  if (info.area > MAX_AREA) {
    return;
  }

  int i = y * img.get_width() + x;
  auto data = img.m_buffer[i];

  if (data < threshold || blacklist[i]) {
    return;
  }

  blacklist[i] = true;
  info.area += 1;

  if (info.max_x == 0 || x > info.max_x) {
    info.max_x = x;
  }
  if (info.min_x == 0 || x < info.min_x) {
    info.min_x = x;
  }

  if (info.max_y == 0 || y > info.max_y) {
    info.max_y = y;
  }
  if (info.min_y == 0 || y < info.min_y) {
    info.min_y = y;
  }

  scanNeighbours(img, blacklist, x + 1, y, threshold, info);
  scanNeighbours(img, blacklist, x - 1, y, threshold, info);
  scanNeighbours(img, blacklist, x, y + 1, threshold, info);
  scanNeighbours(img, blacklist, x, y - 1, threshold, info);
}

inline int findStars(const Image& img, std::vector<StarInfo> &stars, int threshold, int minsize) {
  StarInfo info = StarInfo::zero();

  // A temporary buffer used to ignore already scanned pixels
  bool *blacklist = (bool *)malloc(img.get_pixel_count());
  std::memset(blacklist, 0, img.get_pixel_count());

  // Go through all pixels in image
  for (int x = 0; x < img.get_width(); x += 1) {
    for (int y = 0; y < img.get_height(); y += 1) {
      scanNeighbours(img, blacklist, x, y, threshold, info);
      if (info.area >= minsize && info.area != MAX_AREA) {
        stars.push_back(StarInfo(info));
      }
      info.reset();
    }
  }

  free(blacklist);

  return stars.size();
}

typedef std::pair<std::vector<int>, std::vector<int>> Profil;

inline Profil calc_profile(const Image& img) {
  std::vector<int> horiz_profile(img.get_width()), vert_profile(img.get_height());

  for (int x = 0; x < img.get_width(); ++x) {
    for (int y = 0; y < img.get_height(); ++y) {
      int brightness = img.get_pixel(x, y);
      horiz_profile[x] += brightness;
      vert_profile[y] += brightness;
    }
  }

  return Profil(horiz_profile, vert_profile);
}

inline int save_tiff(const char *file, unsigned char *data, int width,
                     int height) {
  TIFF *output_image;

  // Open the TIFF file
  if ((output_image = TIFFOpen(file, "w")) == NULL) {
    fprintf(stderr, "Unable to write tif file: %s\n", file);
    return -1;
  }

  // We need to set some values for basic tags before we can add any data
  TIFFSetField(output_image, TIFFTAG_IMAGEWIDTH, width);
  TIFFSetField(output_image, TIFFTAG_IMAGELENGTH, height);
  TIFFSetField(output_image, TIFFTAG_BITSPERSAMPLE, 8);
  TIFFSetField(output_image, TIFFTAG_SAMPLESPERPIXEL, 1);
  TIFFSetField(output_image, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);

  TIFFSetField(output_image, TIFFTAG_COMPRESSION, COMPRESSION_DEFLATE);
  TIFFSetField(output_image, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);

  // Write the information to the file
  TIFFWriteEncodedStrip(output_image, 0, data, width * height);

  // Close the file
  TIFFClose(output_image);
  return 0;
}

inline int calculate_threshold(const Image &img) {
  int sum = 0;
  int max = 0;

  for (int i = 0; i < img.get_pixel_count(); ++i) {
    if (img.m_buffer[i] > max) {
      max = img.m_buffer[i];
    }
    sum += img.m_buffer[i];
  }

  int avg = sum / img.get_pixel_count();

  // No peaks found in the image
  if (max - avg < 10) {
    return 255;
  }

  return max * 0.3 + avg * 0.7;
}

inline void visualize_threshold(Image& img, int threshold) {
  for (int i = 0; i < img.get_pixel_count(); ++i) {
    if (img.m_buffer[i] < threshold) {
      img.m_buffer[i] = 0;
    }
  }
}

inline bool sort_stars(const StarInfo &i1, const StarInfo &i2) {
  return i1.area > i2.area;
}

inline int smooth_pixel(const Image &img, int x, int y) {
  const uint8_t *data = img.m_buffer;
  int size = img.get_width();

  int vCenter = data[y * size + x];

  int vEdges = data[y * size + x + 1] + data[y * size + x - 1] +
               data[y * size + x + size] + data[y * size + x - size];

  int vCorners = data[y * size + x + size + 1] + data[y * size + x + size - 1] +
                 data[y * size + x - size + 1] + data[y * size + x - size - 1];

  return 4 * vCenter + 2 * vEdges + vCorners;
}

inline double calculate_centroid(const Image &img, double sx, double sy, double size,
                                 double &_x, double &_y) {
  double peak_val = 0.0;
  int peak_x = 0;
  int peak_y = 0;

  // Search peak pixel
  for (int y = sy + 1; y < sy + size - 1; ++y) {
    for (int x = sx + 1; x < sx + size - 1; ++x) {
      int val = smooth_pixel(img, x, y);
      if (val > peak_val) {
        peak_val = val;
        peak_x = x;
        peak_y = y;
      }
    }
  }

  peak_val /= 16.0;

  // meaure noise in the annulus with inner radius A and outer radius B
  double A = 7;  // inner radius
  double B = 12; // outer radius
  double A2 = A * A;
  double B2 = B * B;

  // center window around peak value
  double start_x = peak_x - B;
  double end_x = peak_x + B;
  double start_y = peak_y - B;
  double end_y = peak_y + B;

  // find the mean and stdev of the background
  double nbg = 0;
  double mean_bg = 0;
  double prev_mean_bg = 0;
  double sigma2_bg = 0;
  double sigma_bg = 0;

  for (int i = 0; i < 9; ++i) {
    double summe = 0;
    double a = 0;
    double q = 0;
    double nbg = 0;

    for (int y = start_y; y <= end_y; ++y) {
      double dy = y - peak_y;
      double dy2 = dy * dy;

      for (int x = start_x; x <= end_x; ++x) {
        double dx = x - peak_x;
        double r2 = dx * dx + dy2;

        if (r2 <= A2 || r2 > B2) {
          continue;
        }

        int val = img.m_buffer[y * img.get_width() + x];

        if (i > 0 &&
            (val < mean_bg - 2 * sigma_bg || val > mean_bg + 2 * sigma_bg)) {
          continue;
        }

        summe += val;
        nbg += 1;

        double k = nbg;
        double a0 = a;

        a += (val - a) / k;
        q += (val - a0) * (val - a);
      }
    }

    if (nbg < 10) {
      std::cerr << "Too few background points!" << std::endl;
      break;
    }

    prev_mean_bg = mean_bg;
    mean_bg = summe / nbg;
    sigma2_bg = q / (nbg - 1);
    sigma_bg = std::sqrt(sigma2_bg);

    if (i > 0 && std::abs(mean_bg - prev_mean_bg) < 0.5) {
      break;
    }
  }

  // find pixels over threshold within aperture; compute mass and centroid
  double thresh = mean_bg + 3 * sigma_bg + 0.5;
  double cx = 0;
  double cy = 0;
  double mass = 0;
  int n = 0;

  start_x = peak_x - A;
  end_x = peak_x + A;
  start_y = peak_y - A;
  end_y = peak_y + A;

  for (int y = start_y; y <= end_y; ++y) {
    double dy = y - peak_y;
    double dy2 = dy * dy;

    if (dy2 > A2) {
      continue;
    }

    for (int x = start_x; x <= end_x; ++x) {
      double dx = x - peak_x;

      // exclude points outside aperture
      if (dx * dx + dy2 > A2) {
        continue;
      }

      int val = img.m_buffer[y * img.get_width() + x];

      // exclude points below threshold
      if (val < thresh) {
        continue;
      }

      double d = val - mean_bg;

      cx += dx * d;
      cy += dy * d;

      mass += d;
      n += 1;
    }
  }

  if (mass <= 0) {
    _x = peak_x;
    _y = peak_y;
  } else {
    _x = peak_x + cx / mass;
    _y = peak_y + cy / mass;
  }

  return mass;
}


inline double calculate_seeing_correlation(std::vector<Image> &frames) {
  /// Calculate coordinates ///
  double cx, cy, avg_x, avg_y;
  double mass;

  int count = 0;

  std::vector<double> x_positions;
  std::vector<double> y_positions;

  for (const Image& frame : frames) {
    mass = calculate_centroid(frame, 0, 0, frame.get_width(), cx, cy);

    if (mass <= 0.0) {
      return 0;
    } else {
      x_positions.push_back(cx);
      y_positions.push_back(cy);
      avg_x += cx;
      avg_y += cy;
      count ++;
    }
  }
  avg_x /= count;
  avg_y /= count;

  if (count < frames.size() / 2) {
      printf("Too many frames failed to calculate centroid\n");
      return 0;
  }

  double s_xx = 0, s_yy = 0, s_xy = 0;
  for (int i = 0; i < count; ++ i) {
    s_xy += (x_positions[i]-avg_x)*(y_positions[i]-avg_y); 
    s_xx += (x_positions[i]-avg_x)*(x_positions[i]-avg_x); 
    s_yy += (y_positions[i]-avg_y)*(y_positions[i]-avg_y); 
  }

  // Korellationskoeffizient
  return std::abs(s_xy / std::sqrt(s_xx * s_yy));
}

inline double calculate_seeing_average(std::vector<Image> &frames) {

  /// Calculate coordinates ///
  double cx, cy;
  double mass;

  std::vector<double> x_positions;
  std::vector<double> y_positions;

  int count = 0;
  for (const Image& frame : frames) {
    mass = calculate_centroid(frame, 0, 0, frame.get_width(), cx, cy);

    if (mass <= 0.0) {
      return 0;
    } else {
      x_positions.push_back(cx);
      y_positions.push_back(cy);
      count ++;
    }
  }

  /// Calculate average ///
  double avg_x = 0.0;
  double avg_y = 0.0;

  for (int i = 1; i < count; ++i) {
    avg_x += x_positions[i] - x_positions[i - 1];
    avg_y += y_positions[i] - y_positions[i - 1];
  }

  avg_x /= (count - 1);
  avg_y /= (count - 1);

  /// Calculate scatter from Average ///
  double avg_diff_x = 0.0;
  double avg_diff_y = 0.0;

  for (int i = 0; i < count; ++i) {
    double estimated_x = x_positions[0] + avg_x * i;
    double estimated_y = y_positions[0] + avg_y * i;

    avg_diff_x += std::abs(estimated_x - x_positions[i]);
    avg_diff_y += std::abs(estimated_y - y_positions[i]);
  }

  avg_diff_x /= count;
  avg_diff_y /= count;

  /// Calculate seeing ///
  double avg_diff = std::sqrt(avg_diff_x * avg_diff_x + avg_diff_y * avg_diff_y);

  return avg_diff * 2.34; // Bogensekunden
}

inline std::string hex_string(int length) {
  char hex_characters[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                           '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

  std::string str;

  while (str.size() < length) {
    str.push_back(hex_characters[rand() % 16]);
  }

  return str;
}

inline std::vector<std::string> exec(const char *cmd) {
  std::array<char, 128> buffer;
  std::vector<std::string> result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
  if (!pipe) {
    throw std::runtime_error("popen() failed!");
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result.push_back(buffer.data());
  }
  return result;
}

template<typename T>
inline T& goToLine(T& file, unsigned int num){
    file.seekg(std::ios::beg);
    for (int i=0; i < num - 1; ++i){
        file.ignore(std::numeric_limits<std::streamsize>::max(),'\n');
    }
    return file;
}

template<typename ... Args>
inline std::string fmt( const std::string& format, Args ... args ) {
    int size_s = std::snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
    if( size_s <= 0 ){ throw std::runtime_error( "Error during formatting." ); }
    auto size = static_cast<size_t>( size_s );
    std::unique_ptr<char[]> buf( new char[ size ] );
    std::snprintf( buf.get(), size, format.c_str(), args ... );
    return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
}

/*
 * Original code: https://gist.github.com/privong/e830e8a16457f4efe7e6 (18.01.2023)
 */
inline double get_siderial_time(double lon) {

    time_t obstime;
    char obstimestr[40];
    struct tm obstimestruct;
    double d, t, GMST_s, LMST_s;

    /*
	double MM = 11;
	double DD = 27;
	double YY = 22;
	double hh = 16;
	double mm = 29;

    */
    //obstime = 1669566540;

	obstime = time(NULL);   // seconds since Unix epoch

    // add JD to get to the UNIX epoch, then subtract to get the days since 2000 Jan 01, 12h UT1 
    d = (obstime / 86400.0) + 2440587.5 - 2451545.0;
    t = d / 36525;

    GMST_s = 24110.54841 + 8640184.812866 * t + 0.093104 * pow(t, 2) - 0.0000062 * pow(t, 3);
    // convert from UT1=0
    GMST_s += obstime;
    GMST_s = GMST_s - 86400.0 * floor(GMST_s / 86400.0);

    // adjust to LMST
    LMST_s = GMST_s + 3600*lon/15.;
    
    if (LMST_s <= 0) {  // LMST is between 0 and 24h
        LMST_s += 86400.0;
    }
    
    printf("lmst: %f\n", LMST_s / 3600.0);
    return LMST_s / 3600.0;
 }

/*
 * 2h55m = 17/6
 */
inline double get_deg_polaris(double longitude) {
    double lmst = get_siderial_time(longitude);
    return 30.0 * (12.0 - ((lmst-17.0/6.0)/2.0) + 6.0);
}


#endif
