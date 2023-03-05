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
	std::string m_name;

public:
	Option(const std::string& group, const std::string& name) 
        : m_group(group), m_name(name) {}

	virtual std::string get_html(const char* id) = 0;

	virtual char* load(const std::string& data) = 0;

	virtual std::string store() = 0;

	std::string get_name() const {
      return m_name;
    }

	std::string get_group() const {
		return m_group;
	}
};

class Settings {
public:
	typedef std::map<std::string, Option*> Config;

	Settings(const Config& values);

	~Settings();

	bool store();

	crow::json::wvalue serialize() const;

	template<typename T>
	T* get(const std::string& key) {
		return m_values.find(key) != m_values.end() 
            ? dynamic_cast<T*>(m_values[key]) : nullptr;
	}

	bool m_changed;

private:
	Config m_values;
};



/* Implementations of Option */

class OptionNumber : public Option {
public:
	OptionNumber(const std::string& group, const std::string& name, double value, double min, double max, int step = 0) 
		: Option(group, name), m_value(value), m_min(min), m_max(max), m_step(step) { }

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
			input = fmt("<input autocomplete=\"off\" id=\"%s\" type=\"number\" value=\"%d\" min=\"%d\" max=\"%d\" step=\"%d\" />", id, (int)m_value, (int)m_min, (int)m_max, m_step);
		} else {
			input = fmt("<input autocomplete=\"off\" id=\"%s\" type=\"number\" value=\"%0.5f\" min=\"%f\" max=\"%f\" />", id, m_value, m_min, m_max);
		}
		auto apply = fmt("<button id=\"btn_%s\">OK</button>", id);

		return input + apply;
	}

private:
	double m_value;
	double m_max, m_min; 
	int m_step;
};


class OptionMode : public Option {
public:
	OptionMode(const std::string& group, const std::string& name, int selected, std::vector<std::string> modes) 
		: Option(group, name), m_selected(selected), m_modes(modes) {}

	std::string get_html(const char* id) {
		std::string options;

		for (int i = 0; i < m_modes.size(); ++ i) {
			options += fmt("<option value=\"%d\" %s>%s</option>", i, m_selected == i ? "selected" : "", m_modes[i].c_str());
		}

		return fmt("<select autocomplete=\"off\" id=\"%s\">%s</select> <button id=\"btn_%s\">OK</button>", id, options.c_str(), id);
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

private:
	std::vector<std::string> m_modes;
	int m_selected;
};

class OptionBool : public Option {
public:
	OptionBool(const std::string& group, const std::string& name, bool value) 
		: Option(group, name), m_value(value) {}

	std::string get_html(const char* id) {
		auto input = fmt("<label class=\"switch\"><input autocomplete=\"off\" id=\"%s\" type=\"checkbox\" %s><span class=\"slider round\"></span></label>", id, m_value ? "checked" : "");
		auto apply = fmt("<button id=\"btn_%s\">OK</button>", id);

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

private:
	bool m_value;
};


class OptionButton : public Option {
public:
	OptionButton(const std::string& group, const std::string& name, std::function<std::string()> callback) 
		: Option(group, name), m_callback(callback) {}

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

private:
	std::function<std::string()> m_callback;
	bool m_value;
};

#endif // SETTINGS_HPP
