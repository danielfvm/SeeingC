#include "UpdateManager.hpp"
#include "util.hpp"

#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <unistd.h>

// If filename contains "seeing" and "tar.gz" at the end we assume it is an
// update archive
int UpdateManager::get_version(const std::string &filename) {
  // Check if the filename starts with "seeing".
  if (filename.substr(0, 6) != "seeing") {
    return -1;
  }

  // Find the position of the second dot in the string.
  size_t dot_pos = filename.find('.', filename.find('.') + 1);
  if (dot_pos == std::string::npos) {
    // Return -1 if there is no second dot.
    return -1;
  }

  // Extract the part of the string between the first and second dots.
  std::string version_string =
      filename.substr(filename.find('.') + 1, dot_pos - filename.find('.') - 1);

  // Convert the version string to an integer.
  int version;
  std::istringstream iss(version_string);
  iss >> version;
  if (iss.fail()) {
    return -1;
  }

  // Check if the filename ends with "tar.gz".
  if (filename.substr(filename.length() - 7) != ".tar.xz") {
    return -1;
  }

  // Return the version number.
  return version;
}

bool UpdateManager::check(UpdateManager* updateManager) {
  while (updateManager->m_running) {
    sleep(updateManager->m_interval);

    std::cout << "Check for update ..." << std::endl;

    struct dirent **namelist;
    int n;

    if ((n = scandir(updateManager->m_path.c_str(), &namelist, 0, alphasort)) < 0) {
      return false;
    }

    for (int i = 0; i < n; ++i) {
      int version = updateManager->get_version(namelist[i]->d_name);
      if (version > updateManager->m_version) {
        std::cout << "Installing update from " << updateManager->m_path << namelist[i]->d_name << std::endl;
        system(fmt("tar -czvf /opt/seeing.%d.bak.tar.xz /opt/seeing/", updateManager->m_version).c_str());
        system("sudo rm -rf /opt/seeing");
        system("sudo mkdir /opt/seeing");
        system(fmt("sudo tar -zxf %s --directory /opt/seeing", updateManager->m_path + namelist[i]->d_name).c_str());
        system("systemctl restart seeing");
        exit(0);
      }

      free(namelist[i]);
    }
    free(namelist);
  }
}


void UpdateManager::listen() { 
	m_running = true;
	m_thread = new std::thread(UpdateManager::check, this); 
}


void UpdateManager::stop() {
	if (m_thread != nullptr) {
		m_running = false;
		m_thread->join();
	}
}
