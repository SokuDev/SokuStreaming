//
// Created by Gegel85 on 05/04/2019.
//

#include <cstring>
#include <sstream>
#include "Socket.hpp"
#include "../Exceptions.hpp"

#ifndef _WIN32
#include <unistd.h>
#else
#define close(fd) closesocket(fd)
#endif


#ifndef _WIN32
#	include <netdb.h>
#	include <arpa/inet.h>
#	include <sys/select.h>
	typedef fd_set FD_SET;
#endif


std::string getLastSocketError()
{
#ifdef _WIN32
	wchar_t *s = nullptr;

	FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPWSTR)&s,
		0,
		NULL
	);

	std::stringstream stream;

	stream << "WSAGetLastError " << WSAGetLastError();
	if (!s)
		return stream.str();
	stream << ": ";
	for (int i = 0; s[i]; i++)
		stream << static_cast<char>(s[i]);
	LocalFree(s);
	return stream.str();
#else
	return strerror(errno);
#endif
}

Socket::Socket()
{
#ifdef _WIN32
	WSADATA WSAData;
	if (WSAStartup(MAKEWORD(2,2), &WSAData))
		throw WSAStartupFailedException(getLastSocketError());
#endif
}

Socket::~Socket()
{
	Socket::disconnect();
}

void Socket::connect(const std::string &host, unsigned short portno)
{
	struct hostent	*server;

	if (this->isOpen())
		throw AlreadyOpenedException("This socket is already opened");

	/* create the socket */
	this->_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (this->_sockfd == INVALID_SOCKET)
		throw SocketCreationErrorException(getLastSocketError());

	/* lookup the ip address */
	server = gethostbyname(host.c_str());
	if (server == nullptr)
		throw HostNotFoundException("Cannot find host '" + host + "'");
	this->connect(*reinterpret_cast<unsigned *>(server->h_addr), portno);
}

void Socket::connect(unsigned int ip, unsigned short portno)
{
	struct sockaddr_in	serv_addr = {};

	if (this->isOpen())
		throw AlreadyOpenedException("This socket is already opened");

	/* fill in the structure */
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = ip;

	/* connect the socket */
	if (::connect(this->_sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
		throw ConnectException(std::string("Cannot connect to ") + inet_ntoa(serv_addr.sin_addr));
	this->_opened = true;
}

Socket::HttpResponse Socket::makeHttpRequest(const Socket::HttpRequest &request)
{
	if (this->isOpen())
		throw AlreadyOpenedException("This socket is already opened");

	std::string requestString = this->generateHttpRequest(request);
	unsigned ip;

	if (request.ip)
		ip = request.ip;
	else
		ip = inet_addr(request.host.c_str());
	if (ip != INADDR_NONE)
		this->connect(ip, request.portno);
	else
		this->connect(request.host, request.portno);

	HttpResponse response = this->readHttpResponse();

	response.request = request;
	if (response.returnCode >= 400)
		throw HTTPErrorException(response);
	return response;
}

void Socket::disconnect()
{
	if (!this->isOpen())
		return;
	if (!this->_noDestroy)
		close(this->_sockfd);
	this->_opened = false;
}

std::string Socket::_read(int size)
{
	std::string result;

	result.resize(size);
	while (true) {
		int bytes = recv(this->_sockfd, result.data(), size, 0);

		if (bytes <= 0) {
			throw EOFException(getLastSocketError());
		}
		result.resize(bytes);
		return result;
	}
}

std::string Socket::read(int size)
{
	this->_mutex.lock();
	if (this->_buffer.empty()) {
		this->_mutex.unlock();
		return this->_read(size);
	}

	std::string result = this->_buffer.substr(0, size);

	this->_buffer = this->_buffer.substr(size);
	this->_mutex.unlock();
	return result;
}

std::string Socket::readExactly(int size)
{
	this->_mutex.lock();
	while (size < this->_buffer.size())
		this->_buffer += this->_read(1024);

	std::string result = this->_buffer.substr(0, size);

	this->_buffer = this->_buffer.substr(size);
	this->_mutex.unlock();
	return result;
}

std::string Socket::getline(const char *delim)
{
	this->_mutex.lock();

	size_t pos = this->_buffer.find_first_of(delim);

	while (pos == std::string::npos) {
		this->_buffer += this->_read(1024);
		pos = this->_buffer.find_first_of(delim);
	}

	std::string result = this->_buffer.substr(0, pos + strlen(delim));

	this->_buffer = this->_buffer.substr(pos + strlen(delim));
	this->_mutex.unlock();
	return result;
}

Socket::HttpRequest Socket::readHttpRequest()
{
	auto line = this->getline("\r\n");
	std::stringstream header(line);
	HttpRequest request;

	header >> request.method;
	header >> request.path;
	header >> request.httpVer;

	if (header.fail())
		throw InvalidHTTPAnswerException("Invalid HTTP request (Invalid first line)");

	for (std::string str = this->getline("\r\n"); str.length() > 2; str = this->getline("\r\n")) {
		std::size_t pos = str.find(':');

		if (pos == std::string::npos)
			throw InvalidHTTPAnswerException("Invalid HTTP request (Invalid header line)");

		std::string name = str.substr(0, pos);
		size_t end = str.size() - 3;

		for (auto &c : name)
			c = std::tolower(c);
		pos++;
		while (pos < str.size() && std::isspace(str[pos]))
			pos++;
		while (pos < end && std::isspace(str[end]))
			end--;
		request.header[name] = str.substr(pos, end - pos + 1);
	}

	try {
		request.host = request.header.at("host");
	} catch (std::out_of_range &) {
		throw InvalidHTTPAnswerException("Invalid HTTP request (no host)");
	}

	if (request.header.find("transfer-encoding") == request.header.end()) {
		try {
			request.body = this->readExactly(std::stoul(request.header.at("content-length")));
		} catch (std::out_of_range &) {
		} catch (std::exception &e) {
			puts(e.what());
			throw InvalidHTTPAnswerException("Invalid HTTP request (bad content-length)");
		}
	} else if (request.header["transfer-encoding"] == "chunked") {
		try {
			for (size_t size = std::stoul(this->getline("\r\n"), nullptr, 16); size; size = std::stoul(this->getline("\r\n"), nullptr, 16))
				request.body += this->readExactly(size);
		} catch (...) {
			throw InvalidHTTPAnswerException("Invalid HTTP request (bad chunk length)");
		}
	} else
		throw InvalidHTTPAnswerException("Invalid HTTP request (bad transfer-encoding)");
	return request;
}

Socket::HttpResponse Socket::readHttpResponse()
{
	std::stringstream header(this->getline("\r\n"));
	HttpResponse response;
	std::string str;

	header >> response.httpVer;
	header >> response.returnCode;

	if (header.fail())
		throw InvalidHTTPAnswerException("Invalid HTTP response (bad first line)");

	response.codeName = response.codeName.substr(1, response.codeName.length() - 2);
	for (std::string str = this->getline("\r\n"); str.length() > 2; str = this->getline("\r\n")) {
		std::size_t pos = str.find(':');

		if (pos == std::string::npos)
			throw InvalidHTTPAnswerException("Invalid HTTP response (Invalid header line)");

		std::string name = str.substr(0, pos);
		size_t end = str.size() - 3;

		for (auto &c : name)
			c = std::tolower(c);
		pos++;
		while (pos < str.size() && std::isspace(str[pos]))
			pos++;
		while (pos < end && std::isspace(str[end]))
			end--;
		response.header[name] = str.substr(pos, end - pos + 1);
	}

	if (response.header.find("transfer-encoding") == response.header.end()) {
		try {
			response.body = this->readExactly(std::stoul(response.header.at("content-length")));
		} catch (std::out_of_range &) {
		} catch (...) {
			throw InvalidHTTPAnswerException("Invalid HTTP response (bad content-length)");
		}
	} else if (response.header["transfer-encoding"] == "chunked") {
		try {
			for (size_t size = std::stoul(this->getline("\r\n"), nullptr, 16); size; size = std::stoul(this->getline("\r\n"), nullptr, 16))
				response.body += this->readExactly(size);
		} catch (...) {
			throw InvalidHTTPAnswerException("Invalid HTTP response (bad chunk length)");
		}
	} else
		throw InvalidHTTPAnswerException("Invalid HTTP response (bad transfer-encoding)");
	return response;
}

void Socket::send(const std::string &msg)
{
	unsigned pos = 0;

	while (pos < msg.length()) {
		int bytes = ::send(this->_sockfd, &msg.c_str()[pos], msg.length() - pos, 0);

		if (bytes <= 0)
			throw EOFException(getLastSocketError());
		pos += bytes;
	}
}

bool	Socket::isOpen() const
{
	FD_SET	set;
	timeval time = {0, 0};

	FD_ZERO(&set);
	FD_SET(this->_sockfd, &set);
	if (this->_opened && select(0, &set, nullptr, nullptr, &time) == -1)
		this->_opened = false;
	return (this->_opened);
}

Socket::Socket(SOCKET sockfd, struct sockaddr_in addr) :
	Socket()
{
	this->_sockfd = sockfd;
	this->_opened = true;
	this->_remote = addr;
}

void Socket::bind(unsigned short port)
{
	struct sockaddr_in	serv_addr = {};

	if (this->isOpen())
		throw AlreadyOpenedException("This socket is already opened");

	this->_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (this->_sockfd == INVALID_SOCKET)
		throw SocketCreationErrorException(getLastSocketError());
	this->_opened = true;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	if (::bind(this->_sockfd, reinterpret_cast<const sockaddr *>(&serv_addr), sizeof(serv_addr)) < 0)
		throw BindFailedException(getLastSocketError());
	if (listen(this->_sockfd, 16) < 0)
		throw ListenFailedException(getLastSocketError());
}

const sockaddr_in &Socket::getRemote() const
{
	return this->_remote;
}

Socket Socket::accept()
{
	struct sockaddr_in serv_addr = {};
	int size = sizeof(serv_addr);
	SOCKET fd = ::accept(this->_sockfd, reinterpret_cast<sockaddr *>(&serv_addr), &size);

	if (fd == INVALID_SOCKET)
		throw AcceptFailedException(getLastSocketError());
	return {fd, serv_addr};
}

std::string Socket::generateHttpResponse(const Socket::HttpResponse &res)
{
	auto response = res;
	std::stringstream msg;

	if (response.header.find("Content-Length") == response.header.end() && !response.body.empty())
		response.header["Content-Length"] = std::to_string(response.body.size());

	/* fill in the parameters */
	msg << response.httpVer << " " << response.returnCode << " " << response.codeName << "\r\n";
	for (auto &entry : response.header)
		msg << entry.first << ": " << entry.second << "\r\n";
	msg << "\r\n" << response.body;
	return msg.str();
}

std::string Socket::generateHttpRequest(const Socket::HttpRequest &req)
{
	auto request = req;
	std::stringstream msg;

	/* fill in the parameters */
	if (request.header.find("Host") == request.header.end())
		request.header["Host"] = request.host;
	if (request.header.find("Content-Length") == request.header.end() && !request.body.empty())
		request.header["Content-Length"] = std::to_string(request.body.length());

	/* fill in the parameters */
	msg << request.method << " " << request.path << " " << request.httpVer << "\r\n";
	for (auto &entry : request.header)
		msg << entry.first << ": " << entry.second << "\r\n";
	msg << "\r\n" << request.body;
	return msg.str();
}

void Socket::setNoDestroy(bool noDestroy) const
{
	this->_noDestroy = noDestroy;
}

bool Socket::isDisconnected() const
{
	return !this->isOpen();
}

Socket::Socket(const Socket &socket) :
	_opened(socket.isOpen()),
	_sockfd(socket.getSockFd()),
	_remote(socket.getRemote())
{
	socket.setNoDestroy(true);
	this->setNoDestroy(false);
}

Socket &Socket::operator=(const Socket &socket)
{
	if (this->isOpen() && !this->_noDestroy)
		this->disconnect();
	this->_opened = socket.isOpen();
	this->_sockfd = socket.getSockFd();
	this->_remote = socket.getRemote();
	socket.setNoDestroy(true);
	this->setNoDestroy(false);
	return *this;
}