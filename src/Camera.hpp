#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <cstdint>

#include "Image.hpp"

// This is our Camera interface. Both our real ASI Camera and the "virtual Camera" for testing implement
// this interface. This allows us to easily interchange the real and the test camera.
class Camera {
public:
	virtual bool open() = 0;
	virtual bool get_fullsize(int &width, int &height) = 0;
	virtual bool set_roi(int cx, int cy, int width, int height) = 0;
	virtual bool get_data(Image& img) = 0;
	virtual bool start_capture() = 0;
	virtual bool stop_capture() = 0;
	virtual void set_exposure(int value) = 0;
	virtual void set_gain(int value) = 0;
	virtual int  get_dropped_frames() = 0;
	virtual int  get_frame() = 0;
	virtual void close() = 0;
};

#endif // CAMERA_HPP
