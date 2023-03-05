#ifndef VIRTUAL_CAMERA_HPP
#define VIRTUAL_CAMERA_HPP

#include "Camera.hpp"
#include "Image.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fitsio.h>
#include <iostream>
#include <stdexcept>
#include <tiffio.h>
#include <vector>
#include <string>
#include <dirent.h>
#include <thread>


#include <fitsio2.h>

class VirtualCamera : public Camera {
public:
	VirtualCamera(std::string path) : m_path(path), m_exposure_time(10000), m_frame(0) {
		if (m_path[m_path.size()-1] != '\\' && m_path[m_path.size()-1] != '/') {
			m_path += '/';
		}
	}

	bool open() { 
		std::cout << "Loading images for virtual camera from folder: " << m_path << std::endl;

		struct dirent **namelist;
		int n;

		if ((n = scandir(m_path.c_str(), &namelist, 0, alphasort)) < 0) {
			return false;
		}

		for (int i = 0; i < n; ++ i) {
			if (namelist[i]->d_name[0] != '.') {
				m_data.push_back(Image(m_path + namelist[i]->d_name));
			}
			free(namelist[i]);
		}
		free(namelist);

		if (m_data.empty()) {
			throw std::runtime_error("Directory empty, no images opened.");
		} else {
			std::cout << "Finished loading " << m_data.size() << " images." << std::endl;
		}

		m_fullwidth = m_data[0].get_width();
		m_fullheight = m_data[0].get_height();

		set_roi(0, 0, m_fullwidth, m_fullheight);

		return true;
	}

	bool get_fullsize(int &width, int &height) {
		width = m_fullwidth;
		height = m_fullheight;

		return true;
	}

	bool set_roi(int cx, int cy, int width, int height) { 
		m_width = width;
		m_height = height;
		m_cx = std::min(std::max(cx, 0), m_fullwidth-width);
		m_cy = std::min(std::max(cy, 0), m_fullheight-height);

		return true;
	}

	bool get_data(Image& img) {
		struct timespec begin, end;
		clock_gettime(CLOCK_REALTIME, &begin);

		m_data[m_frame % m_data.size()].get_subarea(img, m_cx, m_cy, m_width, m_height);

		m_frame ++;

		clock_gettime(CLOCK_REALTIME, &end);

		long seconds = end.tv_sec - begin.tv_sec;
		long nanoseconds = end.tv_nsec - begin.tv_nsec;
		int elapsed = (seconds + nanoseconds*1e-9) * 1e6;
		int delay = m_exposure_time - elapsed;

		std::this_thread::sleep_for(std::chrono::microseconds(delay));

		return true;
	}

	bool start_capture() {
		return true;
	}

	bool stop_capture() { 
		return true;
	}

	void set_exposure(int value) {
		m_exposure_time = value;
	}

	void set_gain(int value) {}

	void close() {}

	int get_dropped_frames() { 
		return 0;
	}

	int get_frame() {
		return m_frame;
	}

private:
	std::vector<Image> m_data;
	std::string m_path;
	int m_cx, m_cy, m_width, m_height;
	int m_fullwidth, m_fullheight;
	int m_exposure_time;
	int m_frame;
};

#endif // VIRTUAL_CAMERA_HPP
