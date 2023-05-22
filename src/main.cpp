#include "ASICamera2.h"
#include "VirtualCamera.hpp"
#include "WebServer.hpp"
#include "Settings.hpp"

#include <chrono>
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
#include <thread>
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

#define PORT 8080

#define DEVICE_NAME "/dev/ttyUSB0"
#define ASTAP_PROGRAM "/home/daniel/Documents/Diploma/astap_command-line_version_Linux_amd64/astap_cli"
#define ASTAP_DATABASE "/home/daniel/Documents/Diploma/astap_command-line_version_Linux_amd64/h18"

// Helper definitions that convert from radians to degrees and vice versa
#define rad(deg) ((deg)*(M_PI/180.0))
#define deg(rad) ((rad)*(180.0/M_PI))

static WebServer* server = nullptr;
static Camera* camera = nullptr;
static SerialManager* serial = nullptr;
static Settings* settings = nullptr;

// Prototypes
std::string btn_platesolving();
std::string btn_shutdown();
std::string btn_download_image();
std::string btn_download_log();

typedef enum {
	M_AVERAGE,
	M_CORRELATION,
	M_FWHM,
} MeasureMode;

typedef enum {
	C_SEARCH,
	C_CALCULATE,
} CaptureMode;

int loadVersion() {
  std::ifstream input("./VERSION");
  int version;

  if (input) {
    input >> version;
    input.close();
    return version;
  }

  return 0;
}


void signalHandler(int signum) {
	// Free memory & exit
	if (camera != nullptr)
		camera->close();
	if (server != nullptr)
		server->stop();
	if (serial != nullptr)
		serial->stop();

	exit(0);
}

double measure_star(Image& frame, const StarInfo& star, int area) {
	const int measurements = settings->get<OptionNumber>("measurements")->get();
	const int measure_mode = settings->get<OptionMode>("measure_mode")->get();

	std::vector<Image> frames;

	// Set region of interest
	int roi_x = star.x()-area/2.0;
	int roi_y = star.y()-area/2.0;
	camera->set_roi(roi_x, roi_y, area, area);

	// Start capturing data
	camera->start_capture();
	for (int i = 0; i < measurements; ++ i) {
		Image img;
		camera->get_data(img);
		frames.push_back(img);

	    server->applyData(img, "Capturing Frame " + std::to_string(i) + " of " + std::to_string(measurements), {}, true);
	}
	camera->stop_capture();
	std::cout << "Captured frames, dropped: " << camera->get_dropped_frames() << std::endl;

	// Calculating seeing from frames
	double seeing;
	switch (measure_mode) {
	case M_AVERAGE:
		seeing = calculate_seeing_average(frames);
		break;
	case M_CORRELATION:
		seeing = calculate_seeing_correlation(frames);
		break;
	case M_FWHM:
		seeing = calculate_seeing_fwhm(frames);
		break;
	default:
		seeing = 0;
	}
	printf("Took %d images and calculated: seeing = %0.4f\n", measurements, seeing);

	// Return latest frame for displaying in webinterface 
	frame.copy_from(frames[measurements-1]);

	return seeing;
}

bool astap_solve(double& ra, double& dc) {
	std::string buffer;

	server->getCurrentDisplayedImage().save_fits("temp.fits");
	std::cout << "Saved temp.fits for ASTAP and starting ASTAP" << std::endl;

	if (fork() == 0) {
		// runs command: ./astap_cli -d h18 -f file.fits -ra 3h -spd 180 -s 50 -m 4
		execlp(ASTAP_PROGRAM, "astap", "-f", "temp.fits", "-d", ASTAP_DATABASE, "-ra", "3h", "-spd", "180", "-s", "50", nullptr);
	} else {
		wait(nullptr);
		std::cout << "ASTAP process finished" << std::endl;
		std::ifstream file("temp.ini");
		if (!file.is_open()) {
			std::cout << "No temp.ini file found!" << std::endl;
			return false;
		}

		// Check if platesolving was successful
		goToLine(file, 2);
		file >> buffer;
		if (buffer.compare("PLTSOLVD=F") == 0) {
			std::cout << "ASTAP exited with error" << std::endl;
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

		std::cout << "Extracted ra: " << ra << " and dc: " << dc << std::endl;

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
	execlp("/bin/shutdown", "shutdown", "-r", "now", nullptr);
	return "Restarting, WebInterface will be unavailable during restart.";
}

std::string btn_download_image() {
	// This function should actually never be called, as downloading the image should work fully client side
	// by creating a downloading of the currently displayed canvas
	return "Creating download link..."; 
}


std::string btn_platesolving() {
	double rightAscension, declination;
	if (!astap_solve(rightAscension, declination)) {
		return "Failed to solve image";
	}

	double longitude = settings->get<OptionNumber>("longitude")->get();
	double latitude = settings->get<OptionNumber>("latitude")->get();
	double lmst = get_siderial_time(longitude);

	// Current image center
	double azimuth, altitude;
	get_azimut_and_height(lmst, rightAscension, declination, latitude, azimuth, altitude);

	// Difference in pixel
	double degPerPx = settings->get<OptionNumber>("deg_per_px")->get() / 3600.0;
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

std::string btn_download_log() {
	system("journalctl -u seeing -S today > seeing.log");
	std::cout << "journalctl process finished" << std::endl;

	std::string data = readFile("seeing.log");
	if (data.empty()) {
		return "ERROR: No log file created!";
	}

	return data;
}

struct Comma final : std::numpunct<char> {
    char do_decimal_point() const override { return ','; }
};

bool store_seeing(double seeing) {
	auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);

    std::ostringstream date, time;
    date << std::put_time(&tm, "%y%m%d");
    time << std::put_time(&tm, "%H:%M");

	std::ofstream out("./measurements/SE" + date.str() + ".dat", std::ios_base::app);
    out.imbue(std::locale(std::locale::classic(), new Comma));

	if (!out.is_open()) {
		return false;
	}

	char data[40];
	int len = sprintf(data, "%s\t%.2f\n", time.str().c_str(), seeing);

	out.write(data, len);
	out.close();

	return true;
}

int main(int argc, char** argv) {
	// Changes working directory if WD (Working Directory) environment was set
	chdir(getenv("WD"));

	// Load the version from the "VERSION" file
	const int VERSION = loadVersion();

	// Apply Settings, has to be after chdir otherwise it cannot find the settings->txt file
	settings = new Settings({
		{"capture_mode", new OptionMode("Discover Stars", "Capture mode", C_SEARCH, {"Search stars", "Calculate seeing"})},
		{"star_size", new OptionNumber("Discover Stars", "Minimum star area (px)", 50, 1, 1000, 1)}, 	// Minimum size of a star to count
		{"exposure", new OptionNumber("Discover Stars", "Exposure (ms)", 10, 0, 10000, 1)},
		{"gain", new OptionNumber("Discover Stars", "Gain", 300, 0, 480, 1)},
		{"min_threshold", new OptionNumber("Discover Stars", "Minimum Threshold", 100, 1, 255, 1)},
		{"v_threshold", new OptionBool("Discover Stars", "Visualize Threshold", false)},
		{"measure_mode", new OptionMode("Seeing", "Seeing-calculation type", M_AVERAGE, {"Average", "Correlation", "FWHM"})},
		{"roi", new OptionNumber("Seeing", "Region of interest (px)", 128, 32, 512, 32)},
		{"pause", new OptionNumber("Seeing", "Pause (s)", 5, 0, 60 * 60, 1)},
		{"measurements", new OptionNumber("Seeing", "Measurments per Seeing", 10, 3, 10000, 1)}, 		// Amount of measurements per seeing value
		{"btn_solving", new OptionButton("Calibrate Telescope", "Plate solving", btn_platesolving)},
		{"longitude", new OptionNumber("Calibrate Telescope", "Longitude", 16.57736, -180, 180)},
		{"latitude", new OptionNumber("Calibrate Telescope", "Latitude", 48.31286, -90, 90)},
		{"deg_per_px", new OptionNumber("Calibrate Telescope", "Arcsec per Pixel", 5.76, 0, 20)},
		{"radius_polaris", new OptionNumber("Calibrate Telescope", "Radius of Polaris orbit (Arcsec)", 2400, 0, 10000)},
		{"btn_shutdown", new OptionButton("Other", "Restart Computer", btn_shutdown)},
		{"btn_download_image", new OptionButton("Other", "Save current image", btn_download_image)},
		{"btn_download_log", new OptionButton("Other", "Save log files", btn_download_log)},
	});


	// Start the webserver
	server = new WebServer(settings, PORT, VERSION);
	server->run();

	// Start the serial communication
	serial = new SerialManager(DEVICE_NAME);
	serial->listen();
	

	// Add signal handler, does the exit on ctrl+c thingy
	std::signal(SIGTERM, signalHandler);
	std::signal(SIGINT, signalHandler);


	// Open asi camera, or if a path was provided the virtual camera
	int retry_count = 3;
	if (argc >= 2) {
		camera = new VirtualCamera(argv[1]);
	} else {
		while (AsiCamera::get_num_of_cameras() <= 0) {
			std::cout << "No cameras available, trying again in 10s ..." << std::endl;
			sleep(10);
		}
		camera = new AsiCamera(0);
	}

	if (!camera->open()) {
		std::cerr << "Failed to open Camera" << std::endl;
		return EXIT_FAILURE;
	}

	// Get camera resolution
	int width, height;
	camera->get_fullsize(width, height);
	std::cout << "Camera resolution: " << width << "x" << height << std::endl;

	// Temporary variables
	std::stringstream status;
	std::vector<StarInfo> stars;
	Image img;

	// Add signal handler, does the exit on ctrl+c thingy
	std::signal(SIGTERM, signalHandler);
	std::signal(SIGINT, signalHandler);

    auto lastTime = std::chrono::high_resolution_clock::now();

	// Start main loop
	while (true) {
		stars.clear();
		status.str("");
		status.clear();

		// Get settings
		const int star_size_min = settings->get<OptionNumber>("star_size")->get();
		const int capture_mode = settings->get<OptionMode>("capture_mode")->get();
		const int show_threshold = settings->get<OptionBool>("v_threshold")->get();
		const int min_threshold = settings->get<OptionNumber>("min_threshold")->get();
		const int exposure = settings->get<OptionNumber>("exposure")->get() * 1000; // stored as ms but used as us
		const int gain = settings->get<OptionNumber>("gain")->get();
		const int area = settings->get<OptionNumber>("roi")->get();

		const auto nowTime = std::chrono::high_resolution_clock::now();

		if (!server->hasClient() && capture_mode == C_SEARCH) {
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			continue;
		}

		camera->set_roi(0, 0, width, height);
		camera->set_exposure(exposure);
		camera->set_gain(gain);


		// Find biggest star
		camera->start_capture();
		camera->get_data(img);
		camera->stop_capture();

		status << "Master frame: " << camera->get_frame() << std::endl;

		const int threshold = std::max(calculate_threshold(img), min_threshold);
		status << "Threshold: " << threshold << std::endl;

		const int count = findStars(img, stars, threshold, star_size_min);
		status << "Star count: " << count << std::endl;

		if (show_threshold && capture_mode == C_SEARCH) {
			visualize_threshold(img, threshold);
		}

		// In case no stars were found, retry
		if (count <= 0) {
			server->applyData(img, status.str(), stars, false);

			if (nowTime > lastTime + std::chrono::seconds(10)) {
				printf("Found %d stars with threshold %d\n", count, threshold);
				printf("Master frame: %d\n", camera->get_frame());
				lastTime = nowTime;
			}

			continue;
		}

		// Sort stars, first element is the biggest star
		std::sort(stars.begin(), stars.end(), sort_stars);

		status << "Brightest star: [ x:" << stars[0].x() << ", y:" << stars[0].y() << ", area:" << stars[0].area << ", d:" << stars[0].diameter() << " ]" << std::endl;

		if (nowTime > lastTime + std::chrono::seconds(10)) {
			printf("Found %d stars with threshold %d\n", count, threshold);
			printf("Brightest star [ a: %dpx, d: %0.2fpx, x: %0.2f, y: %0.2f ]\n", stars[0].area, stars[0].diameter(), stars[0].x(), stars[0].y());
			printf("Master frame: %d, dim: %d, %d\n", camera->get_frame(), img.get_width(), img.get_height());
			lastTime = nowTime;
		}


		/// No need to continue if in capture_mode SEARCH as we do not need to calculate seeing ///
		if (capture_mode == C_SEARCH) {
			server->applyData(img, status.str(), stars, false);
			continue;
		}

		/// Search for a viable star
		Image latestFrame;
		double seeing = 0;
		int i;

		for (i = 0; i < stars.size(); ++ i) {

			// Skip star if to close to edge
			if (is_star_outside_box(img, stars[i], area)) {
				std::cout << "Skipping star " << i << " to close on edge" << std::endl;	
				continue;
			}

			// If it fails to calculate centroid of star, we will skip it too
			double _x, _y;
			if (settings->get<OptionMode>("measure_mode")->get() == M_AVERAGE && calculate_centroid(img, stars[i].x()-area, stars[i].y()-area, area, _x, _y) == 0.0) {
				std::cout << "Skipping star " << i << " failed to calculate centroid" << std::endl;
				continue;
			}

			// Try to calculate seeing value
			seeing = measure_star(latestFrame, stars[i], area);

			// if we got our value we can exit the loop
			if (seeing != 0) {
				break;
			}

			// if seeing is zero it failed to calculate seeing
			std::cout << "Skipping star " << i << " overexposured" << std::endl;
		}

		// Serial update send seeing
		status << "Seeing on Star" << i << ": " << seeing << std::endl;
		server->applyData(latestFrame, status.str(), stars, true);

		if (seeing > 0) {
			serial->send_seeing(seeing);
			store_seeing(seeing);
		}

		// Sleep to not constantly make measurements using the pause setting from the webinterface
		for (int i = settings->get<OptionNumber>("pause")->get(); i > 0 && !settings->m_changed; -- i) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
			if (server->hasClient()) {
				server->applyData(latestFrame, status.str() + "Sleep Timeout: " + std::to_string(i-1) + "s", stars, true);
			}
		}

		if (settings->m_changed) {
			settings->m_changed = false;
			continue;
		}
	}

	return 0;
}
