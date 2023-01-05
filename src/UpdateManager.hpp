#ifndef UPDATE_MANAGER_HPP
#define UPDATE_MANAGER_HPP

#include <string>
#include <thread>

class UpdateManager {
public:
	UpdateManager(const std::string& path, int version, int interval) : m_path(path), m_version(version), m_interval(interval), m_running(false) {}

	// An update archive file has following format:
	//  seeing.VERSION.tar.gz
	static bool check(UpdateManager* updateManager);

	void listen();

	void stop();

	int get_version(const std::string& filename);

private:
	std::thread *m_thread;

	std::string m_path;
	bool m_running;
	int m_version;
	int m_interval;
};

#endif // UPDATE_MANAGER_HPP
