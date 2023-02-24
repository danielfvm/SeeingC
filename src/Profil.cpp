#include "Profil.hpp"

Profil::Profil(std::vector<int> &profil_x, std::vector<int> &profil_y) {
  this->first = profil_x;
  this->second = profil_y;
}

Profil::Profil(const Image &img) {
  this->set_from_image(img);
}


void Profil::set_from_image(const Image& img) {
  this->first.assign(img.get_width(), 0);
  this->second.assign(img.get_height(), 0);

  for (int x = 0; x < img.get_width(); ++x) {
    for (int y = 0; y < img.get_height(); ++y) {
      int brightness = img.get_pixel(x, y);
      this->first[x] += brightness;
      this->second[y] += brightness;
    }
  }
}

bool Profil::get_x_profil(int &min, int &max, int &mid) {
  return this->get_profil(this->first, min, max, mid);
}

bool Profil::get_y_profil(int &min, int &max, int &mid) {
  return this->get_profil(this->second, min, max, mid);
}

float Profil::get_fwhm() {
  int min, max, mid;

  if (!this->get_x_profil(min, max, mid)) {
    return 0;
  }

  int x1 = 0;
  int x2 = 0;
  int profval;
  int profvalprec;

  // Find middle values x coords
  for (int i = 1; i < this->first.size(); ++i) {
    profval = this->first[i];
    profvalprec = this->first[i - 1];

    if (profvalprec <= mid && profval >= mid)
      x1 = i;
    else if (profvalprec >= mid && profval <= mid)
      x2 = i;
  }

  profval = this->first[x1];
  profvalprec = this->first[x1 - 1];
  float f1 =
      (float)x1 - (float)(profval - mid) / (float)(profval - profvalprec);

  profval = this->first[x2];
  profvalprec = this->first[x2 - 1];
  float f2 =
      (float)x2 - (float)(profvalprec - mid) / (float)(profvalprec - profval);

  return f2 - f1;
}

bool Profil::get_profil(const std::vector<int> &profil, int &min, int &max, int &mid) {

  // Assign default value
  min = max = profil[0];

  // Search for biggest and smallest number in profil
  for (int i = 1; i < profil.size(); i++) {
    if (profil[i] < min)
      min = profil[i];
    else if (profil[i] > max)
      max = profil[i];
  }

  if (min >= max) {
    return false;
  }

  // Calculate middle value
  mid = (max - min) / 2 + min;

  return true;
}
