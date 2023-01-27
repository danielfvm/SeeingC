#include "WebServer.hpp"
#include "Settings.hpp"

#include <crow/http_response.h>
#include <crow/json.h>
#include <crow/logging.h>
#include <cstdlib>
#include <sstream>
#include <thread>
#include <vector>

WebServer::WebServer(Settings &settings, int port)
    : m_settings(settings), m_port(port), m_thread(nullptr) {

  crow::logger::setLogLevel(crow::LogLevel::ERROR);
  CROW_ROUTE(m_app, "/")
  ([&](const crow::request &req) {
    crow::mustache::context ctx;
    ctx["groups"] = settings.json();
    ctx["rand"] = std::rand();

    auto page = crow::mustache::load("index.html");
    return page.render(ctx);
  });

  CROW_ROUTE(m_app, "/call/<string>")
  ([&](const crow::request &req, const std::string &key) {
    auto option = settings.get<OptionButton>(key);
    if (option == nullptr) {
      auto resp = crow::response("false");
      resp.set_header("Content-Type", "text/plain");
      return resp;
    }

    auto resp = crow::response(option->call());
    resp.set_header("Content-Type", "text/plain");
    return resp;
  });

  CROW_ROUTE(m_app, "/set/<string>/<string>")
  ([&](const crow::request &req, const std::string &key,
       const std::string &value) {
    auto option = settings.get<Option>(key);
    if (option == nullptr) {
      auto resp = crow::response("false");
      resp.set_header("Content-Type", "text/plain");
      return resp;
    }
    char *result = option->load(value);
    if (result != nullptr) {
      auto resp = crow::response(result);
      resp.set_header("Content-Type", "text/plain");
      return resp;
    }

    auto resp =
        crow::response(settings.store() ? "true" : "failed to save data");
    resp.set_header("Content-Type", "text/plain");
    return resp;
  });

  CROW_ROUTE(m_app, "/image")
  ([&](const crow::request &req) {
    auto response = crow::response(m_image_buffer);
    response.set_header("Content-Type", "image/jpeg");
    return response;
  });

  CROW_ROUTE(m_app, "/status")
  ([&](const crow::request &req) {
    auto response = crow::response(m_status_text);
    response.set_header("Content-Type", "text/plain");
    return response;
  });

  CROW_ROUTE(m_app, "/indicators")
  ([&](const crow::request &req) {
    double deg_per_px = settings.get<OptionNumber>("deg_per_px")->get();
    double radius_polaris = settings.get<OptionNumber>("radius_polaris")->get() / deg_per_px;
    double deg_polaris = get_deg_polaris(settings.get<OptionNumber>("longitude")->get());

    std::stringstream ss;
    ss << radius_polaris << std::endl;
    ss << deg_polaris << std::endl;
    ss << m_pltslv_x << std::endl;
    ss << m_pltslv_y << std::endl;

    auto response = crow::response(ss.str());
    response.set_header("Content-Type", "text/plain");
    return response;
  });

  CROW_ROUTE(m_app, "/stars")
  ([&](const crow::request &req) {
    std::stringstream data;

    for (const StarInfo &info : m_stars) {
      data << info.x() << ' ' << info.y() << ' ' << info.diameter()
           << std::endl;
    }

    auto response = crow::response(data.str());
    response.set_header("Content-Type", "text/plain");
    return response;
  });

  CROW_ROUTE(m_app, "/profil")
  ([&](const crow::request &req) {
    std::stringstream data;

    for (const int &value : m_profile.first) {
      data << value << std::endl;
    }

    for (const int &value : m_profile.second) {
      data << value << std::endl;
    }

    auto response = crow::response(data.str());
    response.set_header("Content-Type", "text/plain");
    return response;
  });

  CROW_ROUTE(m_app, "/platesolve")
  ([&](const crow::request &req) {
    auto response = crow::response(m_status_text);
    response.set_header("Content-Type", "text/plain");
    return response;
  });
}

void WebServer::exec(WebServer *server) {
  server->m_app.port(server->m_port).multithreaded().run();
}

void WebServer::run() { 
  m_thread = new std::thread(WebServer::exec, this); 
}

void WebServer::stop() {
  if (m_thread != nullptr) {
    m_app.stop();
    m_thread->join();
  }
}

void WebServer::applyData(Image &img, const std::string &status,
                          const std::vector<StarInfo> &stars) {
  m_image_buffer = img.get_encoded_str();
  m_status_text = status;
  m_stars = stars;
}

void WebServer::setPlateSolveData(double x, double y) {
  m_pltslv_x = x;
  m_pltslv_y = y;
}
