#include "../includes/Socket.hpp"
#include "stdexcept"
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>

Socket::Socket()
{
	_socketFd = -1;
	_isBlocking = true;
	_addrLen = sizeof(_address);

	_socketFd = socket(AF_INET, SOCK_STREAM, 0);
	if (_socketFd == -1)
		throw std::runtime_error("Failed to create socket");
}

Socket::~Socket()
{
	if (_socketFd != -1)
		close(_socketFd);
}

void Socket::bind(const std::string& host, int port)
{
	_address.sin_family = AF_INET;
	_address.sin_addr.s_addr = inet_addr(host.c_str());
	_address.sin_port = htons(port);

	if (::bind(_socketFd, (struct sockaddr*)&_address, sizeof(_address)) == -1)
		throw std::runtime_error("Failed to bind socket");
}

void Socket::listen(int backlog)
{
	if (::listen(_socketFd, backlog) == -1)
		throw std::runtime_error("Failed to listen on socket");
}

int Socket::accept()
{
	int clientSocket = ::accept(_socketFd, (struct sockaddr*)&_address, (socklen_t*)&_addrLen);
	if (clientSocket < 0)
		throw std::runtime_error("Failed to accept connection");
	return clientSocket;
}

void Socket::setNonBLocking()
{
	int flags = fcntl(_socketFd, F_GETFL, 0);
	if (flags == -1)
		throw std::runtime_error("Failed to get socket flags");
	if (fcntl(_socketFd, F_SETFL, flags | O_NONBLOCK) == -1)
		throw std::runtime_error("Failed to set socket to non-blocking");
	_isBlocking = false;
}

void Socket::setReuseAddr()
{
	int opt = 1;
	if (setsockopt(_socketFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
		throw std::runtime_error("Failed to set socket to reuse address");
}

std::string Socket::getClientIP() const {return inet_ntoa(_address.sin_addr);}
int Socket::getFd() const				{return _socketFd;}
int Socket::getClientPort() const		{return ntohs(_address.sin_port);}