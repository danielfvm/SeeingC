#ifndef WEBSERVER_HPP
#define WEBSERVER_HPP

#include "Settings.hpp"
#include "util.hpp"


#include <crow.h>
#include <mutex>
#include <thread>
#include <unordered_set>

class WebServer {
public:
	WebServer(Settings& settings, int socket);

	void run();

	void stop();

	void applyData(Image& img, const std::string& status, const std::vector<StarInfo>& stars);

	Profil m_profile;

private:
	static void exec(WebServer* server);

	int m_port;
	crow::SimpleApp m_app;

    std::mutex m_mtx;
    std::unordered_set<crow::websocket::connection*> m_users;

	std::thread *m_thread, *m_image_thread;

	Settings m_settings;

	// Information for the website, set via applyData methode
	std::string m_image_buffer, m_status_text;
	std::vector<StarInfo> m_stars;
};

#endif // WEBSERVER_HPP
