#ifndef SETTINGS_HPP
#define SETTINGS_HPP

#include "util.hpp"
#include <crow/json.h>
#include <cstdio>
#include <functional>
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <tuple>
#include <vector>

#define cfg_filename "settings.txt" // config file name

class Option {
protected:
	std::string m_group;

public:
	Option(const std::string& group) : m_group(group) {}

	virtual std::string get_html(const char* id) = 0;

	virtual char* load(const std::string& data) = 0;

	virtual std::string store() = 0;

	virtual std::string get_name() = 0;

	std::string get_group() const {
		return m_group;
	}
};

class OptionNumber : public Option {
public:
	OptionNumber(const std::string& group, const std::string& name, double value, double min, double max, int step = 0) 
		: Option(group), m_name(name), m_value(value), m_min(min), m_max(max), m_step(step) { }

	double get() {
		return m_value;
	}

	void set(double data) {
		m_value = data;
	}

	char* load(const std::string& data) {
		double val = std::stof(data);
		if (m_step != 0 && int(val-m_min)%m_step != 0)
			return (char*)"Not a valid multiple of number";
		m_value = val > m_max ? m_max : (val < m_min ? m_min : val);
		return nullptr;
	}

	std::string store() {
		std::stringstream ss;
		ss << m_value;

		return ss.str();
	}

	std::string get_html(const char* id) {
		std::string input;
		if (m_step != 0) {
			input = fmt("<input id=\"%s\" type=\"number\" value=\"%d\" min=\"%d\" max=\"%d\" step=\"%d\" />", id, (int)m_value, (int)m_min, (int)m_max, m_step);
		} else {
			input = fmt("<input id=\"%s\" type=\"number\" value=\"%0.5f\" min=\"%f\" max=\"%f\" />", id, m_value, m_min, m_max);
		}
		auto apply = fmt("<button id=\"btn_%s\" disabled>OK</button>", id);

		return input + apply;
	}

	std::string get_name() {
		return m_name;
	}

private:
	std::string m_name;
	double m_value;
	double m_max, m_min; 
	int m_step;
};


class OptionMode : public Option {
public:
	OptionMode(const std::string& group, const std::string& name, int selected, std::vector<std::string> modes) 
		: Option(group), m_name(name), m_selected(selected), m_modes(modes) {}

	std::string get_html(const char* id) {
		std::string options;

		for (int i = 0; i < m_modes.size(); ++ i) {
			options += fmt("<option value=\"%d\">%s</option>", i, m_modes[i].c_str());
		}

		return fmt("<select id=\"%s\">%s</select> <button id=\"btn_%s\" disabled>OK</button>", id, options.c_str(), id);
	}

	int get() {
		return m_selected;
	}

	char* load(const std::string& data) {
		int select = std::stoi(data);
		if (0 <= select && select < m_modes.size()) {
			m_selected = select;
			return nullptr;
		} 
		return (char*)"Invalid option";
	}

	std::string store() {
		std::stringstream ss;
		ss << m_selected;
		return ss.str();
	}

	std::string get_name() {
		return m_name;
	}

private:
	std::vector<std::string> m_modes;
	std::string m_name;
	int m_selected;
};

class OptionBool : public Option {
public:
	OptionBool(const std::string& group, const std::string& name, bool value) 
		: Option(group), m_name(name), m_value(value) {}

	std::string get_html(const char* id) {
		auto input = fmt("<label class=\"switch\"><input id=\"%s\" type=\"checkbox\" %s><span class=\"slider round\"></span></label>", id, m_value ? "checked" : "");
		auto apply = fmt("<button id=\"btn_%s\" disabled>OK</button>", id);

		return input + apply;
	}

	int get() {
		return m_value;
	}

	char* load(const std::string& data) {
		m_value = std::stoi(data) != 0;
		return nullptr;
	}

	std::string store() {
		return m_value ? "1" : "0";
	}

	std::string get_name() {
		return m_name;
	}

private:
	std::string m_name;
	bool m_value;
};


class OptionButton : public Option {
public:
	OptionButton(const std::string& group, const std::string& name, std::function<std::string()> callback) 
		: Option(group), m_name(name), m_callback(callback) {}

	std::string get_html(const char* id) {
		return fmt("<button class=\"actionbtn\" id=\"%s\">Run</button>", id);
	}

	std::string call() {
		return m_callback();
	}

	char* load(const std::string& data) { return nullptr; }

	std::string store() {
		return "";
	}

	std::string get_name() {
		return m_name;
	}

private:
	std::function<std::string()> m_callback;
	std::string m_name;
	bool m_value;
};

class Settings {
public:
	typedef std::map<std::string, Option*> CFG;

private:
	CFG m_values;

	void load() {
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

public:
	Settings(const CFG& values) : m_values(values) {
		load();
	}

	~Settings() {
		for (auto it = m_values.begin(); it != m_values.end(); it ++) {
			free(it->second);
		}
	}

	bool store() {
		std::ofstream file(cfg_filename);

		if (!file.is_open()) {
			return false;
		}

		for (auto it = m_values.begin(); it != m_values.end(); it ++) {
			std::string data = it->second->store();
			if (!data.empty()) {
				file << it->first << '=' << data << std::endl;
			}
		}

		file.close();
		return true;
	}

	template<typename T>
	T* get(const std::string& key) {
		if (m_values.find(key) != m_values.end()) {
			return dynamic_cast<T*>(m_values[key]);
		}
		return nullptr;
	}

	std::vector<crow::json::wvalue> json() {
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
};

#endif // SETTINGS_HPP
