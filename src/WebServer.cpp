#include "WebServer.hpp"
#include "Settings.hpp"
#include "util.hpp"

#include <chrono>
#include <crow/http_response.h>
#include <crow/json.h>
#include <crow/logging.h>
#include <cstdlib>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

WebServer::WebServer(Settings* settings, int port, int version) : m_version(version), m_port(port), m_thread(nullptr) {
  crow::logger::setLogLevel(crow::LogLevel::ERROR);

  CROW_ROUTE(m_app, "/")([settings, this](const crow::request &req) {
    crow::mustache::context ctx;
    ctx["settings"] = settings->serialize();
    ctx["version"] = m_version;

    auto page = crow::mustache::load("index.html");
    return page.render(ctx);
  });

  CROW_ROUTE(m_app, "/call/<string>")([settings, this](const crow::request &req, const std::string &key) {
    auto option = settings->get<OptionButton>(key);
    if (option == nullptr) {
      auto resp = crow::response("false");
      resp.set_header("Content-Type", "text/plain");
      return resp;
    }

    auto resp = crow::response(option->call());
    resp.set_header("Content-Type", "text/plain");
    return resp;
  });

  CROW_ROUTE(m_app, "/set/<string>/<string>")([settings, this](const crow::request &req, const std::string &key, const std::string &value) {
    auto option = settings->get<Option>(key);
    if (option == nullptr) {
      std::cout << "Setting " << key << " was not found, couldn't apply changes." << std::endl;
      auto resp = crow::response("false");
      resp.set_header("Content-Type", "text/plain");
      return resp;
    }

    char *result = option->load(value);
    if (result != nullptr) {
      std::cout << "Setting " << key << " failed to save" << std::endl;
      auto resp = crow::response(result);
      resp.set_header("Content-Type", "text/plain");
      return resp;
    }

    bool success = settings->store();
    auto resp = crow::response(success ? "true" : "failed to save data");
    resp.set_header("Content-Type", "text/plain");

    std::cout << "Setting " << key << (success ? " succeeded" : " failed") << " saving to " << option->store() << std::endl;

    return resp;
  });

  CROW_ROUTE(m_app, "/fullimage")([settings, this](const crow::request &req) {
    auto response = crow::response(m_image.get_encoded_str(100));
    response.set_header("Content-Type", "image/jpeg");
    return response;
  });

  CROW_ROUTE(m_app, "/info")([settings, this](const crow::request &req) {
    auto response = crow::response(m_status_text);
    response.set_header("Content-Type", "text/json");

    crow::json::wvalue data;
    data["status"] = m_status_text;
    data["stars"] = StarInfo::serializeVector(m_stars, 50);
    data["settings"] = settings->serialize();
    data["profil"] = m_profile.serialize();

    double deg_per_px = settings->get<OptionNumber>("deg_per_px")->get() / 3600.0;
    double radius_polaris = (settings->get<OptionNumber>("radius_polaris")->get() / 3600.0) / deg_per_px;
    double deg_polaris = get_deg_polaris(settings->get<OptionNumber>("longitude")->get()) + 180;

    data["deg_per_px"] = deg_per_px;
    data["radius_polaris"] = radius_polaris;
    data["deg_polaris"] = deg_polaris;
    data["pltslv_x"] = m_pltslv_x;
    data["pltslv_y"] = m_pltslv_y;

    return data;
  });
}

void WebServer::exec(WebServer *server) {
  server->m_app.port(server->m_port).multithreaded().run();
}

void WebServer::run() { 
  m_thread = new std::thread(WebServer::exec, this); 

  m_streamer.start(m_port + 1);

  std::cout << "Running Webserver: http://localhost:" << m_port << std::endl;
  std::cout << "Running MJPEGStreamer: http://localhost:" << (m_port+1) << std::endl;
}

void WebServer::stop() {
  if (m_thread != nullptr) {
    m_streamer.stop();
    m_app.stop();
    m_thread->join();
  }
}

bool WebServer::hasClient() {
  return m_streamer.hasClient("/image");
}

void WebServer::applyData(const Image &img, const std::string &status, const std::vector<StarInfo> &stars, bool calculateProfile) {

  // Save resources by only encoding and publishing when client is available
  if (this->hasClient()) {

    // Returns current time used to limit framerate
    static auto last = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::milliseconds fps(1000 / 10); // limit frame rate to 10 FPS

    // if faster than 10 FPS, slow down
    if (now - last < fps) {
      std::this_thread::sleep_for(fps - (now - last));
    }
    last = now;

    // Publish the image, with 75% JPEG Quality level
    m_streamer.publish("/image", img.get_encoded_str(75));
  }

  // Store copy of image for /fullimage Route
  m_image.copy_from(img);

  // Copy stars and status information
  m_status_text = status;
  m_stars = stars;

  // Calculate Starprofil if set to true
  if (calculateProfile) {
    m_profile.set_from_image(img);
  } else {
    m_profile.first.clear();
    m_profile.second.clear();
  }
}

void WebServer::setPlateSolveData(double x, double y) {
  m_pltslv_x = x;
  m_pltslv_y = y;
}
