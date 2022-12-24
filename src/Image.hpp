#ifndef IMAGE_HPP
#define IMAGE_HPP

#include <CCfits/CCfits.h>

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

enum ErrorCode {
  IMAGE_SUCCESS,
  IMAGE_FILE_ERROR,
  IMAGE_ALLOC_ERROR,
  IMAGE_OTHER_ERROR,
};

class Image {
public:
  uint8_t *m_buffer;

  Image(int width, int height) : m_width(width), m_height(height) {
    m_buffer = (uint8_t *)malloc(get_pixel_count());
  }

  Image(int width, int height, uint8_t *buffer)
      : m_width(width), m_height(height), m_buffer(buffer) {}

  Image(const std::string &filepath) : m_buffer(nullptr) {
    ErrorCode code = open_tiff(filepath);
    if (code != IMAGE_SUCCESS) {
      throw std::runtime_error("Failed to open file: " + filepath + " error code: " + std::to_string(code));
    }
  }

  Image() : m_width(0), m_height(0), m_buffer(nullptr) {}

  Image(const Image &img) : m_width(img.m_width), m_height(img.m_height) {
    m_buffer = (uint8_t *)malloc(get_pixel_count());
    std::memcpy(m_buffer, img.m_buffer, get_pixel_count());
  }

  ~Image() {
    if (m_buffer) {
      free(m_buffer);
    }
  }

  ErrorCode open_tiff(const std::string &filepath);

  ErrorCode save_tiff(const std::string &filepath) const;

  void set(int width, int height, uint8_t *buffer);

  void set(int width, int height);

  void get_subarea(Image &img, int sx, int sy, int width, int height) const;

  std::string get_encoded_str();

  int get_width() const { return m_width; }

  int get_height() const { return m_height; }

  int get_pixel_count() const { return m_width * m_height; }

  uint8_t get_pixel(int x, int y) const;
  uint8_t& get_pixel(int x, int y);

  void save_fits(const char *filename);

private:
  int m_width, m_height;
  std::string m_build_buffer;

  uint8_t to_grayscale(int rgba) const;

  static void write_func(Image *img, void *data, int size);
};

#endif // IMAGE_HPP
