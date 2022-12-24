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

#define MAX_MEASUREMENTS 1000
#define PORT 8082

#define DEVICE_NAME "/dev/ttyUSB0"
#define ASTAP_PROGRAM "/home/daniel/Documents/Diploma/astap_command-line_version_Linux_amd64/astap_cli"
#define ASTAP_DATABASE "/home/daniel/Documents/Diploma/astap_command-line_version_Linux_amd64/h18"

static std::vector<StarInfo> stars;
static bool global_exit = false;

static WebServer* server = nullptr;
static Camera* camera = nullptr;
static SerialManager* serial = nullptr;

void signalHandler(int signum) {
	/// Free memory & exit ///
	if (camera != nullptr)
		camera->close();
	if (server != nullptr)
		server->stop();
	if (serial != nullptr)
		serial->stop();
	delete server;

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

// https://www.latlong.net/
double longitude;

double get_siderial_time(double Long) {
    std::time_t t = std::time(0);   // get time now
    std::tm* now = std::localtime(&t);

	/* 2022-11-27T16:29:17
	double MM = now->tm_mon+1;
	double DD = now->tm_mday;
	double YY = now->tm_year+1900;
	double hh = now->tm_hour;
	double mm = now->tm_min;
	*/
	double MM = 11;
	double DD = 27;
	double YY = 2022;
	double hh = 16;
	double mm = 29;

	// convert mm to fractional time:
	mm = mm/60.0;

	// reformat UTC time as fractional hours:
	double UT = hh+mm;

	// calculate the Julian date:
	double JD = (367*YY) - int((7*(YY+int((MM+9)/12.0)))/4.0) + int((275*MM)/9.0) + DD + 1721013.5 + (UT/24.0);

	// calculate the Greenwhich mean sidereal time:
	double GMST = 18.697374558 + 24.06570982441908 * (JD - 2451545);
	GMST = GMST - int(GMST/24.0)*24;    //use modulo operator to convert to 24 hours

	// Convert to the local sidereal time by adding the longitude (in hours) from the GMST.
	// (Hours = Degrees/15, Degrees = Hours*15)
	Long = Long/15.0;      		// Convert longitude to hours
	double LST = GMST+Long;     // Fraction LST. If negative we want to add 24...
	if (LST < 0) {
		LST = LST + 24;
	}

	return LST;
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

// ./astap_cli -d h18 -f 20221127_1629_1s_GMax_RGB24.fits -ra 3h -spd 180 -s 50 -m 4 -analyse snr_min

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
		
		std::cout << "values: " << ra << " " << dc << std::endl;
	}

	return true;
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
	double ra, dc;

	astap_solve(ra, dc);

	double siderial_time = get_siderial_time(longitude);
	double h_a = siderial_time - ra;

	return "Rektaszension=" + std::to_string(ra) + " Deklination=" + std::to_string(dc) + " Ha=" + std::to_string(h_a) + " Sid=" + std::to_string(siderial_time);
}

int main(int argc, char** argv) {
	Settings settings({
		{"btn_solving", new OptionButton("Plate solving", btn_platesolving)},
		{"exposure", 	new OptionNumber("Exposure (ms)", 10, 0, 1000)},
		{"gain", 		new OptionNumber("Gain", 300, 0, 480, 1)},
		{"capture_mode",new OptionMode("Capture mode", 0, {"Search stars", "Calculate seeing"})},
		{"measure_mode",new OptionMode("Seeing-calculation type", 0, {"Average", "Correlation"})},
		{"roi", 		new OptionNumber("Region of interest (px)", 128, 32, 512, 32)}, // Region Of Interest, multiple of 2
		{"measurements",new OptionNumber("Measurments", 10, 1, MAX_MEASUREMENTS)}, 		// Amount of measurements per seeing value
		{"star_size", 	new OptionNumber("Minimum star area (px)", 1, 1, 1000, 1)}, 	// Minimum size of a star to count
		{"v_threshold", new OptionBool("Visualize Threshold", false)},
		{"btn_shutdown",new OptionButton("Shutdown Computer", btn_shutdown)},
		{"longitude", 	new OptionNumber("Longitude", 0, -180, 180)},
	});

	server = new WebServer(settings, PORT);
	server->run();

	serial = new SerialManager(DEVICE_NAME);
	serial->listen();

	/// Add signal handler, does the exit on ctrl+c thingy ///
	std::signal(SIGINT, signalHandler);


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


	/// Start main loop ///
	while (true) {
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
		longitude = settings.get<OptionNumber>("longitude")->get();

		camera->set_roi(0, 0, width, height);
		camera->set_exposure(exposure);
		camera->set_gain(gain);


		/// Find biggest star ///
		camera->start_capture();
		camera->get_data(img);
		camera->stop_capture();

		status << "Main frame: " << camera->get_frame() << std::endl;

		int threshold = calculate_threshold(img);
		printf("Thre: %d\n", threshold);
		status << "Threshold: " << threshold << std::endl;

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
