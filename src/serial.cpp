#include <cstdlib>
#include <cstring>
#include <fcntl.h> 
#include <iostream>
#include <ostream>
#include <string.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>

#include "serial.h"

SerialManager::SerialManager(const std::string& device) : m_running(false), m_seeing(0), m_thread(nullptr) {
	open_uart(device);
}

bool SerialManager::open_uart(const std::string& device) {
	m_fd = open(device.c_str(), O_RDWR | O_NOCTTY | O_SYNC);

	if (m_fd < 0) {
		std::cerr << "Failed to open serial port: " << device << std::endl;
		m_thread = nullptr;
		return false;
	}

	set_interface_attribs(m_fd, B9600, 0);
	set_blocking(m_fd, 1);

	return true;
}


void SerialManager::send_seeing(double seeing) {
	m_seeing.push_back(seeing);
}

double SerialManager::get_avg_seeing() {
	double sum_seeing = 0;
	for (auto& seeing : m_seeing) {
		sum_seeing += seeing;
	}
	return sum_seeing / (double)m_seeing.size();
}

void SerialManager::exec(SerialManager* serial) {
	char buffer[20];
	int len;

	do {
		len = read(serial->m_fd, buffer, 20);
		if (len == 6 && std::memcmp(buffer, "shut", 4) == 0) {
			std::cout << "Device is shutting down" << std::endl;
			write(serial->m_fd, "ok\r\n", 4);
			sleep(1);
			execlp("/bin/shutdown", "shutdown", "now", nullptr);
		} else if (len == 5 && std::memcmp(buffer, "get", 3) == 0) {
			std::cout << "Send-Data-Request recv from SQM" << std::endl;
			if (!serial->m_seeing.empty()) {
				memset(buffer, 0, sizeof(buffer));
				len = sprintf(buffer, "%03f\r\n", serial->get_avg_seeing()); 
				serial->m_seeing.clear();

				std::cout << "Send seeing value: " << buffer << std::endl;
				write(serial->m_fd, buffer, len);
			} else {
				std::cout << "No data to be send" << std::endl;
			}
		} else {
			std::cout << "[SERIAL] No match, read in " << len <<  " bytes: " << buffer << std::endl;
		}
	} while (serial->m_running);

	close(serial->m_fd);
	serial->m_fd = -1;
}


void SerialManager::listen() { 
	if (m_fd < 0) {
		std::cerr << "Cannot listen closed serial port!" << std::endl;
		m_thread = nullptr;
		return;
	}

	m_running = true;
	m_thread = new std::thread(SerialManager::exec, this); 
}


void SerialManager::stop() {
	if (m_thread != nullptr) {
		m_running = false;
		m_thread->join();
	}
}

int SerialManager::set_interface_attribs(int fd, int speed, int parity) {
	struct termios tty;
	if (tcgetattr(fd, &tty) != 0) {
		perror("error d from tcgetattr");
		return -1;
	}

	cfsetospeed(&tty, speed);
	cfsetispeed(&tty, speed);

	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
	// disable IGNBRK for mismatched speed tests; otherwise receive break
	// as \000 chars
	tty.c_iflag &= ~IGNBRK;         // disable break processing
	tty.c_lflag = 0;                // no signaling chars, no echo,
	// no canonical processing
	tty.c_oflag = 0;                // no remapping, no delays
	tty.c_cc[VMIN]  = 0;            // read doesn't block
	tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

	tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

	tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
	// enable reading
	tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
	tty.c_cflag |= parity;
	tty.c_cflag &= ~CSTOPB;
	tty.c_cflag &= ~CRTSCTS;

	if (tcsetattr(fd, TCSANOW, &tty) != 0) {
		perror("error d from tcsetattr");
		return -1;
	}
	return 0;
}

void SerialManager::set_blocking(int fd, int should_block) {
	struct termios tty;
	memset(&tty, 0, sizeof tty);
	if (tcgetattr(fd, &tty) != 0) {
		perror("error %d from tggetattr");
		return;
	}

	tty.c_cc[VMIN]  = should_block ? 1 : 0;
	tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

	if (tcsetattr(fd, TCSANOW, &tty) != 0) {
		perror("error %d setting term attributes");
	}
}
