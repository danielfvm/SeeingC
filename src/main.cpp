#include "ASICamera2.h"
#include "VirtualCamera.hpp"
#include "WebServer.hpp"
#include "Settings.hpp"

#include <longnam.h>
#include <sstream>
#include <stdint.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <CCfits/PHDU.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <algorithm>

#include <unistd.h>
#include <vector>

#include "fitsio2.h"

#include "AsiCamera.hpp"
#include "util.hpp"
#include "Image.hpp"

#include "serial.h"

#define PORT 8082

#define DEVICE_NAME "/dev/ttyUSB0"
#define ASTAP_PROGRAM "/home/daniel/Documents/Diploma/astap_command-line_version_Linux_amd64/astap_cli"
#define ASTAP_DATABASE "/home/daniel/Documents/Diploma/astap_command-line_version_Linux_amd64/h18"

// Helper definitions that convert from radians to degrees and vice versa
#define rad(deg) ((deg)*(M_PI/180.0))
#define deg(rad) ((rad)*(180.0/M_PI))

static std::vector<StarInfo> stars;
static bool global_exit = false;

static WebServer* server = nullptr;
static Camera* camera = nullptr;
static SerialManager* serial = nullptr;

// Prototypes
std::string btn_platesolving();
std::string btn_shutdown();

/* All settings accessible from the webinterface menu are defined here */
Settings settings({
	{"capture_mode", new OptionMode("Discover Stars", "Capture mode", 0, {"Search stars", "Calculate seeing"})},
	{"star_size", new OptionNumber("Discover Stars", "Minimum star area (px)", 100, 1, 1000, 1)}, 	// Minimum size of a star to count
	{"exposure", new OptionNumber("Discover Stars", "Exposure (ms)", 10, 0, 1000, 1)},
	{"gain", new OptionNumber("Discover Stars", "Gain", 300, 0, 480, 1)},
	{"v_threshold", new OptionBool("Discover Stars", "Visualize Threshold", false)},
	{"measure_mode", new OptionMode("Seeing", "Seeing-calculation type", 0, {"Average", "Correlation"})},
	{"roi", new OptionNumber("Seeing", "Region of interest (px)", 128, 32, 512, 32)},
	{"pause", new OptionNumber("Seeing", "Pause (s)", 5, 0, 60 * 60, 1)},
	{"measurements", new OptionNumber("Seeing", "Measurments per Seeing", 10, 1, 1000, 1)}, 		// Amount of measurements per seeing value
	{"btn_solving", new OptionButton("Calibrate Telescope", "Plate solving", btn_platesolving)},
	{"longitude", new OptionNumber("Calibrate Telescope", "Longitude", 16.57736, -180, 180)},
	{"latitude", new OptionNumber("Calibrate Telescope", "Latitude", 48.31286, -90, 90)},
	{"deg_per_px", new OptionNumber("Calibrate Telescope", "Degree per Pixel", 6.75e-4, 0, 1)},
	{"radius_polaris", new OptionNumber("Calibrate Telescope", "Radius of Polaris orbit (Degree)", 0.6369444, 0, 1)},
	{"btn_shutdown", new OptionButton("Other", "Shutdown Computer", btn_shutdown)},
});


void signalHandler(int signum) {
	/// Free memory & exit ///
	if (camera != nullptr)
		camera->close();
	if (server != nullptr)
		server->stop();
	if (serial != nullptr)
		serial->stop();

	exit(0);
}


double avg_seeing = 0;
std::vector<double> seeings;

double measure_star(const StarInfo& star, int area, int measurements, std::stringstream& status, int measure_mode) {
	std::vector<Image> frames;

	/// Set region of interest ///
	int roi_x = star.x()-area/2.0;
	int roi_y = star.y()-area/2.0;
	camera->set_roi(roi_x, roi_y, area, area);

	/// Start capturing data ///
	camera->start_capture();
	for (int i = 0; i < measurements; ++ i) {
		Image frame;
		camera->get_data(frame);
		frames.push_back(frame);
	}
	camera->stop_capture();

	std::cout << "Captured frames, dropped: " << camera->get_dropped_frames() << std::endl;

	/// Calculating coords ///
	double seeing = measure_mode == 0 ? calculate_seeing_average(frames) : calculate_seeing_correlation(frames);
	printf("Took %d images and calculated: seeing = %0.5f\"\n", measurements, seeing);

	Profil profil = calc_profile(frames[measurements-1]);
	server->m_profile = profil;

	//if (seeing != 0) {
		status << "ROI: [ x:" << roi_x << ", y:" << roi_y << ", w:" << area << ", h:" << area << " ]" << std::endl;
		status << "Seeing: " << seeing << std::endl;

		if (seeing != 0) {
			seeings.push_back(seeing);
			if (seeings.size() > 30) {
				avg_seeing = 0;
				for (auto x : seeings) {
					avg_seeing += x;
				}
				avg_seeing /= seeings.size();
				seeings.clear();
			}
		}
		status << "AVG Seeing: " << avg_seeing << std::endl;

		server->applyData(frames[measurements-1], status.str(), stars);

		//server->applyData(buffer_list[measurements-1], area, area, status.str(), stars);
	//}

	return seeing;
}

bool astap_solve(double& ra, double& dc) {
	int fullwidth, fullheight;
	std::string buffer;
	Image img;

	camera->get_fullsize(fullwidth, fullheight);  // get image resolution
	camera->set_roi(0, 0, fullwidth, fullheight); // and update roi with resolution
	camera->set_exposure(5000000); // 5s
	camera->set_gain(100);

	camera->start_capture();
	camera->get_data(img);
	camera->stop_capture();

	img.save_fits("temp.fits");
	std::cout << "saved fits" << std::endl;

	if (fork() == 0) {
		// runs command: ./astap_cli -d h18 -f file.fits -ra 3h -spd 180 -s 50 -m 4
		std::cout << "exec cmd: "<< std::endl;
		execlp(ASTAP_PROGRAM, "astap", "-f", "temp.fits", "-d", ASTAP_DATABASE, "-ra", "3h", "spd", "180", "-s", "50", "-m", "4", nullptr);
	} else {
		wait(nullptr);
		std::cout << "child finished" << std::endl;
		std::ifstream file("temp.ini");
		if (!file.is_open()) {
			return false;
		}

		// Check if platesolving was successful
		goToLine(file, 2);
		file >> buffer;
		if (buffer.compare("PLTSOLVD=F") == 0) {
			std::cout << "child exited with error" << std::endl;
			file.close();
			return false;
		}

		goToLine(file, 4);
		file.seekg(8, std::ios::cur);
		file >> ra;

		goToLine(file, 5);
		file.seekg(8, std::ios::cur);
		file >> dc;

		file.close();

		// Convert to different format
		ra = (24.0 / 360.0) * ra;
	}

	return true;
}


/**
 * This function calculates the azimuth and altitude of a celestial body given its right ascension, declination,
 * and the latitude and local mean sidereal time at the observer's location.
 *
 * Parameters:
 *  lmst: Local mean sidereal time at the observer's location in degrees.
 *  rightAscension: Right ascension of the celestial body in degrees.
 *  declination: Declination of the celestial body in degrees.
 *  latitude: Latitude of the observer's location in degrees.
 *  azimuth: Output parameter for the calculated azimuth in degrees.
 *  altitude: Output parameter for the calculated altitude in degrees.
 */
void get_azimut_and_height(double lmst, double rightAscension, double declination, double latitude, double& azimuth, double& altitude) {
  double phi = rad(latitude);
  double tau = rad(15 * (lmst - rightAscension));
  double delta = rad(declination);

  azimuth = deg(atan(sin(tau)/(sin(phi)*cos(tau)-cos(phi)*tan(delta))));
  altitude = deg(asin(sin(phi)*sin(delta) + cos(phi)*cos(delta)*cos(tau)));
}

/* 
 * Returns false if star is to close to the edge of the image, because this
 * can cause issues since the algorithm would look for pixels outside of
 * the bounds, which could lead to crashes or undefined beahviour.
 */
bool is_star_outside_box(const Image& img, StarInfo& star, int area) {
	int space = area/2;
	bool bottomright = star.x() > img.get_width() - space || star.y() > img.get_height() - space;
	bool topleft = star.x() < space || star.y() < space;

	return bottomright || topleft;
}

int auto_gain(StarInfo& star, int area, int threshold, int star_size_min, int gain) {
	/// Set region of interest ///
	int roi_x = star.x()-area/2.0;
	int roi_y = star.y()-area/2.0;
	printf("Setting roi %s\n", camera->set_roi(roi_x, roi_y, area, area) ? "success" : "failed");

	/// Start capturing data ///
	printf("START capturing frames\n");

	struct timespec begin, end;
	clock_gettime(CLOCK_REALTIME, &begin);

	std::vector<StarInfo> stars;
	Image frame;
	double x, y;

	do {
		gain -= 50;
		if (gain < 0) {
			gain = 0;
		}

		camera->set_gain(gain);
		camera->start_capture();
		camera->get_data(frame);
		camera->stop_capture();
	} while (gain > 0 && calculate_centroid(frame, 0, 0, area, x, y) == 0.0);

	return gain;
}

std::string btn_shutdown() {
	execlp("/bin/shutdown", "shutdown", "now", nullptr);
	return "exiting";
}


std::string btn_platesolving() {
	double rightAscension, declination;
	if (!astap_solve(rightAscension, declination)) {
		return "Failed to solve image";
	}

	double longitude = settings.get<OptionNumber>("longitude")->get();
	double latitude = settings.get<OptionNumber>("latitude")->get();
	double lmst = get_siderial_time(longitude);

	// Current image center
	double azimuth, altitude;
	get_azimut_and_height(lmst, rightAscension, declination, latitude, azimuth, altitude);

	// Difference in pixel
	double degPerPx = settings.get<OptionNumber>("deg_per_px")->get();
	double diffX = (azimuth-0)/degPerPx;
	double diffY = (altitude-latitude)/degPerPx;

	server->setPlateSolveData(diffX, diffY);

	std::stringstream ss;

	ss << "rekta: " << rightAscension << std::endl;
	ss << "dekli: " << declination << std::endl;
	ss << "delta azimuth: " << azimuth << std::endl;
	ss << "delta altitude: " << (altitude-latitude) << std::endl;

	return ss.str();
}

int main(int argc, char** argv) {
	// Changes working directory if WD (Working Directory) environment was set
	chdir(getenv("WD"));

	server = new WebServer(settings, PORT);
	server->run();

	serial = new SerialManager(DEVICE_NAME);
	serial->listen();

	/// Open asi camera, or if a path was provided the virtual camera ///
	if (argc >= 2) {
		camera = new VirtualCamera(argv[1]);
	} else {
		if (AsiCamera::get_num_of_cameras() == 0) {
			std::cerr << "No cameras available!" << std::endl;
			return EXIT_FAILURE;
		}

		camera = new AsiCamera(0);
	}

	if (!camera->open()) {
		std::cerr << "Failed to open Camera" << std::endl;
		return EXIT_FAILURE;
	}

	/// Get camera resolution ///
	int width, height;
	camera->get_fullsize(width, height);
	std::cout << "Camera resolution: " << width << "x" << height << std::endl;

	/// Temporary variables ///
	std::stringstream status;
	Image img;


	/// Add signal handler, does the exit on ctrl+c thingy ///
	std::signal(SIGTERM, signalHandler);
	std::signal(SIGINT, signalHandler);


	/// Start main loop ///
	while (true) {
		sleep(settings.get<OptionNumber>("pause")->get());

		stars.clear();
		status.str("");
		status.clear();

		/// Get settings ///
		int star_size_min = settings.get<OptionNumber>("star_size")->get();
		int capture_mode = settings.get<OptionMode>("capture_mode")->get();
		int measurements = settings.get<OptionNumber>("measurements")->get();
		int v_threshold = settings.get<OptionBool>("v_threshold")->get();
		int exposure = settings.get<OptionNumber>("exposure")->get() * 1000; // stored as ms but used as us
		int gain = settings.get<OptionNumber>("gain")->get();
		int area = settings.get<OptionNumber>("roi")->get();
		int measure_mode = settings.get<OptionMode>("measure_mode")->get();

		camera->set_roi(0, 0, width, height);
		camera->set_exposure(exposure);
		camera->set_gain(gain);


		/// Find biggest star ///
		camera->start_capture();
		camera->get_data(img);
		camera->stop_capture();

		status << "Master frame: " << camera->get_frame() << std::endl;

		int threshold = calculate_threshold(img);
		printf("Thre: %d\n", threshold);
		status << "Threshold: " << threshold << std::endl;

		printf("Search stars\n");
		int count = findStars(img, stars, threshold, star_size_min);
		printf("Stars found: %d\n", count);
		status << "Star count: " << count << std::endl;

		if (v_threshold && capture_mode == 0) {
			visualize_threshold(img, threshold);
		}

		/// In case no stars were found, retry ///
		if (count <= 0) {
			server->m_profile.first.clear();
			server->m_profile.second.clear();

			server->applyData(img, status.str(), stars);
			continue;
		}

		// Sort stars, first element is the biggest star
		std::sort(stars.begin(), stars.end(), sort_stars);

		printf("Brightest star with area %dpx and estimated diameter %0.2fpx at [ %0.2f, %0.2f ], with a circleness of %0.2f \n", stars[0].area, stars[0].diameter(), stars[0].x(), stars[0].y(), stars[0].circleness());
		status << "Brightest star: [ x:" << stars[0].x() << ", y:" << stars[0].y() << ", area:" << stars[0].area << ", d:" << stars[0].diameter() << " ]" << std::endl;


		/// No need to continue if in capture_mode 0 ///
		if (capture_mode == 0) {
			server->m_profile.first.clear();
			server->m_profile.second.clear();

			server->applyData(img, status.str(), stars);
			continue;
		}


		double _x, _y;
		double seeing;

		// If largest star is cannot be calculated, try reducing gain
		if (/* auto_gain && */ gain > 0 && calculate_centroid(img, stars[0].x()-area, stars[0].y()-area, area, _x, _y) == 0.0) {
			int newgain = auto_gain(stars[0], area, threshold, star_size_min, gain);

			if (newgain == 0) { // trying to fix it by changing gain failed, take next smaller star instead
				camera->set_gain(gain);
			} else {
				std::cout << "Reduced gain: " << gain << std::endl;
			}
		}

		for (int i = 0; i < stars.size(); ++ i) {

			// Skip star if to close to edge
			if (is_star_outside_box(img, stars[i], area)) {
				status << "Skipping star " << i << " to close on edge" << std::endl;	
				std::cout << "Skipping star " << i << " to close on edge" << std::endl;	
				continue;
			}

			// If it fails to calculate centroid of star, we will skip it too
			if (calculate_centroid(img, stars[i].x()-area, stars[i].y()-area, area, _x, _y) == 0.0) {
				status << "Skipping star "	<< i << std::endl;
				std::cout << "Skipping star "	<< i << std::endl;
				continue;
			}

			seeing = measure_star(stars[i], area, measurements, status, measure_mode);

			serial->send_seeing(seeing);

			if (seeing != 0) {
				status << "Measuring seeing success, avg: " << avg_seeing << std::endl;	
				std::cout << "Measuring seeing success" << std::endl;	
				break;
			} else {
				status << "Failed to measure star "	<< i << std::endl;	
				std::cout << "Failed to measure star "	<< i << std::endl;	
			}
		}
	}

	return 0;
}
