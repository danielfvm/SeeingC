#ifndef PROFIL_HPP
#define PROFIL_HPP

#include <vector>

#include "Image.hpp"

class Profil : public std::pair<std::vector<int>, std::vector<int>> {
public:
  Profil(std::vector<int> &profil_x, std::vector<int> &profil_y);

  Profil(const Image &img);

  Profil() {}

  void set_from_image(const Image& img);

  bool get_x_profil(int &min, int &max, int &mid);

  bool get_y_profil(int &min, int &max, int &mid);

  float get_fwhm();

private:
  bool get_profil(const std::vector<int> &profil, int &min, int &max, int &mid);
};

#endif // PROFIL_HPP
