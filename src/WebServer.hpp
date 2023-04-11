#ifndef WEBSERVER_HPP
#define WEBSERVER_HPP

#include "Image.hpp"
#include "Settings.hpp"
#include "Profil.hpp"
#include "util.hpp"
#include "mjpeg_streamer.hpp"


#include <crow.h>
#include <mutex>
#include <thread>
#include <unordered_set>

class WebServer {
public:
	WebServer(Settings* settings, int socket, int version);

	void run();

	void stop();

	void applyData(const Image& img, const std::string& status, const std::vector<StarInfo>& stars, bool calculateProfile);

	void setPlateSolveData(double x, double y);

	const Image& getCurrentDisplayedImage() const { return m_image; }

	bool hasClient();

private:
	static void exec(WebServer* server);

	int m_port, m_version;

    nadjieb::MJPEGStreamer m_streamer;
	crow::SimpleApp m_app;

	std::thread *m_thread;

	Image m_image; 		 // Displayed picture
	Profil m_profile; 	 // The information used to create a diagram for the star profile

	// Information for the website, set via applyData methode
	std::string m_status_text;
	std::string m_indicators;
	std::vector<StarInfo> m_stars;

	// PlateSolving information, set via setPlateSolveData
    double m_pltslv_x = 0;
    double m_pltslv_y = 0;
};

#endif // WEBSERVER_HPP
