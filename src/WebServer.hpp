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
	WebServer(Settings& settings, int socket, int version);

	void run();

	void stop();

	void applyData(const Image& img, const std::string& status, const std::vector<StarInfo>& stars, bool calculateProfile);

	void setPlateSolveData(double x, double y);

	const Image& getCurrentDisplayedImage() const { return m_image; }

private:
	static void exec(WebServer* server);

	// The information used to create a diagram for the star profile
	Profil m_profile;

	int m_port, m_version;
	crow::SimpleApp m_app;
    nadjieb::MJPEGStreamer m_streamer;

    std::mutex m_mtx;
    std::unordered_set<crow::websocket::connection*> m_users;

	std::thread *m_thread, *m_image_thread;

	Settings m_settings;

	Image m_image;

	// Information for the website, set via applyData methode
	std::string m_status_text;
	std::string m_indicators;
	std::vector<StarInfo> m_stars;

    double m_pltslv_x = 0;
    double m_pltslv_y = 0;
};

#endif // WEBSERVER_HPP
