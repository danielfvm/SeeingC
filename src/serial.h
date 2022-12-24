// https://stackoverflow.com/questions/6947413/how-to-open-read-and-write-from-serial-port-in-c

#ifndef SERIAL_H
#define SERIAL_H

#include <string>
#include <thread>

class SerialManager {
public:
	SerialManager(const std::string& device);

	bool open_uart(const std::string& device);

	void listen();

	void stop();

	void send_seeing(double seeing);

private:
	int set_interface_attribs(int fd, int speed, int parity);

	void set_blocking(int fd, int should_block);

	static void exec(SerialManager* serial);

	std::thread *m_thread;

	bool m_running;

	int m_fd;

	double m_seeing;
};

#endif // SERIAL_H
