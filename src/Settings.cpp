#include "Settings.hpp"
#include <crow/returnable.h>

Settings::Settings(const Config& values) : m_values(values), m_changed(false) {
	char line[200];

	std::ifstream file(cfg_filename);

	if (!file.is_open()) {
		return;
	}

	for (;;) {
		file.getline(line, 200);
		if (file.eof()) {
			break;
		}

		std::string data(line);
		int deli = data.find('=');

		std::string key = data.substr(0, deli); 
		std::string value = data.substr(deli+1);

		if (m_values.find(key) != m_values.end()) {
			m_values[key]->load(value);
		}
	}

	file.close();
}

Settings::~Settings() {
	for (auto it = m_values.begin(); it != m_values.end(); it ++) {
		free(it->second);
	}
}

bool Settings::store() {
	std::ofstream file(cfg_filename);

	if (!file.is_open()) {
		return false;
	}

	m_changed = true;

	for (auto it = m_values.begin(); it != m_values.end(); it ++) {
		std::string data = it->second->store();
		if (!data.empty()) {
			file << it->first << '=' << data << std::endl;
		}
	}

	file.close();
	return true;
}

crow::json::wvalue Settings::serialize() const {
	std::string group_name;
	crow::json::wvalue group;
	group["name"] = "";

	std::map<std::string, std::vector<crow::json::wvalue>> groups;

	for (auto it = m_values.begin(); it != m_values.end(); ++it) {
		groups[it->second->get_group()].push_back({
			{"option_name", it->second->get_name()},
			{"option_html", it->second->get_html(it->first.c_str())},
		});
	}

	std::vector<crow::json::wvalue> data;
	for (auto it = groups.begin(); it != groups.end(); ++it) {
		data.push_back({
			{"group_name", it->first},
			{"group_list", it->second},
		});
	}

	return data;
}
