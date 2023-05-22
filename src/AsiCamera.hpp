#ifndef ASI_CAMERA_HPP
#define ASI_CAMERA_HPP

#include "ASICamera2.h"
#include "Camera.hpp"
#include "Image.hpp"

class AsiCamera : public Camera {
public:
	AsiCamera(int id) : m_id(id), m_opened(false), m_frame(0) {}

	bool open() {
		m_opened = (ASIOpenCamera(m_id) == ASI_SUCCESS && ASIInitCamera(m_id) == ASI_SUCCESS);
		if (!m_opened) {
			return false;
		}

		// Apply some default values
		ASIDisableDarkSubtract(m_id);
		ASISetControlValue(m_id, ASI_BANDWIDTHOVERLOAD, 100, ASI_FALSE);
		ASISetControlValue(m_id, ASI_WB_B, 99, ASI_FALSE);
		ASISetControlValue(m_id, ASI_WB_R, 75, ASI_FALSE);
		ASISetControlValue(m_id, ASI_GAMMA, 50, ASI_FALSE);
		ASISetControlValue(m_id, ASI_BRIGHTNESS, 450, ASI_FALSE);
		ASISetControlValue(m_id, ASI_FLIP, 0, ASI_FALSE);
		ASIStopExposure(m_id);

		set_exposure(10000);
		set_gain(0);

		m_opened = set_roi_format(ASI_IMG_Y8);
		if (!m_opened) {
			return false;
		}

		m_frame = 0;

		return m_opened;
	}

	bool start_capture() {
		return ASIStartVideoCapture(m_id) == ASI_SUCCESS;
	}

	bool stop_capture() {
		return ASIStopVideoCapture(m_id) == ASI_SUCCESS;
	}

	void set_exposure(int value) {
		m_timeout = value*2+500;
		ASISetControlValue(m_id, ASI_EXPOSURE, value, ASI_FALSE);
	}

	void set_gain(int value) {
		ASISetControlValue(m_id, ASI_GAIN, value, ASI_FALSE);
	}

	bool get_fullsize(int &width, int &height) { 
		ASI_CAMERA_INFO info;

		if (ASIGetCameraProperty(&info, m_id) != ASI_SUCCESS) {
			return false;
		}

		width = info.MaxWidth;
		height = info.MaxHeight;

		return true;
	}

	double get_pixelsize() {
		ASI_CAMERA_INFO info;
		ASIGetCameraProperty(&info, m_id);
		return info.PixelSize;
	}

	bool set_roi(int cx, int cy, int width, int height) { 
		ASI_IMG_TYPE type;
		int _w, _h, bin;

		ASIGetROIFormat(m_id, &_w, &_h, &bin, &type);
		m_width = width;
		m_height = height;

		return ASISetROIFormat(m_id, width, height, bin, type) == ASI_SUCCESS
			&& ASISetStartPos(m_id, cx, cy) == ASI_SUCCESS;
	}

	int get_dropped_frames() {
		int dropped;
		ASIGetDroppedFrames(m_id, &dropped);
		return dropped;
	}

	bool get_data(Image& img) { 
		m_frame += 1;

		img.set(m_width, m_height);

		return ASIGetVideoData(m_id, (unsigned char*)img.m_buffer, img.get_pixel_count(), m_timeout) == ASI_SUCCESS;
	}

	void close() { 
		if (m_opened) {
			ASIStopVideoCapture(m_id);
			//ASICloseCamera(m_id);
			m_opened = false;
		}
	}

	int get_frame() {
		return m_frame;
	}

	static int get_num_of_cameras() {
		return ASIGetNumOfConnectedCameras();
	}

private:
	int m_width, m_height;
	int m_id;
	int m_timeout;
	int m_frame;
	bool m_opened;

	bool set_roi_format(ASI_IMG_TYPE type) {
		ASI_IMG_TYPE _type;
		int bin;

		return ASIGetROIFormat(m_id, &m_width, &m_height, &bin, &_type) == ASI_SUCCESS
			&& ASISetROIFormat(m_id, m_width, m_height, bin, type) == ASI_SUCCESS;
	}

};

#endif // ASI_CAMERA_HPP
