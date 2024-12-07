//
// Created by PinkySmile on 04/12/2020.
//

#include <csignal>
#include <iostream>
#include <fstream>
#include <regex>
#include <filesystem>
#include "WebServer.hpp"
#include "../Exceptions.hpp"
#include "nlohmann/json.hpp"

const std::map<std::string, std::string> WebServer::types{
	{"txt", "text/plain"},
	{"js", "text/javascript"},
	{"css", "text/css"},
	{"html", "text/html"},
	{"png", "image/png"},
	{"apng", "image/apng"},
	{"bmp", "image/bmp"},
	{"gif", "image/gif"},
	{"ico", "image/x-icon"},
	{"cur", "image/x-icon"},
	{"jpeg", "image/jpeg"},
	{"jpg", "image/jpeg"},
	{"jfif", "image/jpeg"},
	{"pjpeg", "image/jpeg"},
	{"pjg", "image/jpeg"},
	{"tif", "image/tiff"},
	{"tiff", "image/tiff"},
	{"svg", "image/svg+xml"},
	{"webp", "image/webp"},
};

const std::map<unsigned short, std::string> WebServer::codes{
	{ 100, "Continue"},
	{ 101, "Switching Protocols" },
	{ 102, "Processing" },
	{ 103, "Early Hints" },
	{ 200, "OK" },
	{ 201, "Created" },
	{ 202, "Accepted" },
	{ 203, "Non-Authoritative Information" },
	{ 204, "No Content" },
	{ 205, "Reset Content" },
	{ 206, "Partial Content" },
	{ 207, "Multi-Status" },
	{ 208, "Already Reported" },
	{ 210, "Content Different" },
	{ 226, "IM Used" },
	{ 300, "Multiple Choices" },
	{ 301, "Moved Permanently" },
	{ 302, "Found" },
	{ 303, "See Other" },
	{ 304, "Not Modified" },
	{ 305, "Use Proxy" },
	{ 306, "Switch Proxy" },
	{ 307, "Temporary Redirect" },
	{ 308, "Permanent Redirect" },
	{ 310, "Too many Redirects" },
	{ 400, "Bad Request" },
	{ 401, "Unauthorized" },
	{ 402, "Payment Required" },
	{ 403, "Forbidden" },
	{ 404, "Not Found" },
	{ 405, "Method Not Allowed" },
	{ 406, "Not Acceptable" },
	{ 407, "Proxy Authentication Required" },
	{ 408, "Request Time-out" },
	{ 409, "Conflict" },
	{ 410, "Gone" },
	{ 411, "Length Required" },
	{ 412, "Precondition Failed" },
	{ 413, "Request Entity Too Large" },
	{ 414, "Request-URI Too Long" },
	{ 415, "Unsupported Media Type" },
	{ 416, "Requested range unsatisfiable" },
	{ 417, "Expectation failed" },
	{ 418, "I’m a teapot" },
	{ 421, "Misdirected Request" },
	{ 425, "Too Early" },
	{ 426, "Upgrade Required" },
	{ 428, "Precondition Required" },
	{ 429, "Too Many Requests" },
	{ 431, "Request Header Fields Too Large" },
	{ 451, "Unavailable For Legal Reasons" },
	{ 500, "Internal Server Error" },
	{ 501, "Not Implemented" },
	{ 502, "Bad Gateway" },
	{ 503, "Service Unavailable" },
	{ 504, "Gateway Time-out" },
	{ 505, "HTTP Version not supported" },
	{ 506, "Variant Also Negotiates" },
	{ 509, "Bandwidth Limit Exceeded" },
	{ 510, "Not extended" },
	{ 511, "Network authentication required" },
};

WebServer::WebServer(int staticAge) :
	_staticAge(staticAge)
{
}

void WebServer::addRoute(const std::string &&route, std::function<Socket::HttpResponse(const Socket::HttpRequest &)> &&fct)
{
	std::cout << "Adding route " << route << std::endl;
	this->_routes[route] = fct;
}

void WebServer::addStaticFolder(const std::string &&route, const std::string &&path, bool discoverable)
{
	std::cout << "Adding static folder " << route << " -> " << path << std::endl;
	this->_folders[route] = {path, discoverable};
}

void WebServer::start(unsigned short port)
{
	this->_sock.bind(port);
	std::cout << "Started server on port " << port << std::endl;
	this->_thread = std::thread([this]{
		while (!this->_closed)
			this->_serverLoop();
	});
}

void ___(int){}

void WebServer::stop()
{
	this->_closed = true;
	auto old = signal(SIGINT, ___);

	std::for_each(
		this->_webSocks.begin(),
		this->_webSocks.end(),
		[](std::shared_ptr<WebSocketConnection> &s) {
			s->wsock.disconnect();
			if (s->thread.joinable())
				s->thread.join();
		}
	);
	raise(SIGINT); //Interrupt accept
	signal(SIGINT, old);
	if (this->_thread.joinable())
		this->_thread.join();
}

WebServer::~WebServer()
{
	this->stop();
}

void WebServer::_serverLoop()
{
	Socket newConnection = this->_sock.accept();
	Socket::HttpResponse response;
	Socket::HttpRequest requ;

#ifdef _DEBUG
	std::cerr << "New connection from " << inet_ntoa(newConnection.getRemote().sin_addr) << ":" << newConnection.getRemote().sin_port << std::endl;
#endif
	try {
		try {
			timeval time = {1, 0};

			requ = newConnection.readHttpRequest(&time);
			requ.ip = newConnection.getRemote().sin_addr.s_addr;
			requ.portno = newConnection.getRemote().sin_port;
			if (requ.httpVer != "HTTP/1.1")
				throw AbortConnectionException(505);
			WebServer::_parsePath(requ);
			if (requ.realPath == "/chat")
				return this->_addWebSocket(newConnection, requ);
			else {
				auto it = std::find_if(this->_routes.begin(), this->_routes.end(), [&requ](auto &pair){
					return std::regex_match(requ.realPath, std::regex{pair.first});
				});

				response.request = requ;
				if (it != this->_routes.end())
					response = it->second(requ);
				else
					response = this->_checkFolders(requ);
			}
		} catch (InvalidHTTPAnswerException &e) {
		#ifdef _DEBUG
			std::cout << e.what() << std::endl;
		#endif
			response = WebServer::_makeGenericPage(400);
		} catch (NotImplementedException &) {
			response = WebServer::_makeGenericPage(501);
		} catch (EOFException &) {
			response = WebServer::_makeGenericPage(408);
		} catch (AbortConnectionException &e) {
			if (*e.getBody()) {
				response.returnCode = e.getCode();
				response.codeName = WebServer::codes.at(response.returnCode);
				response.header["content-type"] = e.getType();
				response.body = e.getBody();
			} else
				response = WebServer::_makeGenericPage(e.getCode());
		}
		response.codeName = WebServer::codes.at(response.returnCode);
		response.header["Connection"] = "Close";
	} catch (std::exception &e) {
		std::cerr << e.what() << std::endl;
		response = WebServer::_makeGenericPage(500, e.what());
	}
	response.httpVer = "HTTP/1.1";
#ifdef _DEBUG
	std::cout << inet_ntoa(newConnection.getRemote().sin_addr) << ":" << newConnection.getRemote().sin_port << " ";
	if (!requ.httpVer.empty())
		std::cout << requ.path;
	else
		std::cout << "<Malformed HTTP request>";
	std::cout << ": " << response.returnCode << std::endl;
#endif
	try {
		newConnection.send(Socket::generateHttpResponse(response));
	} catch (...) {}
	newConnection.disconnect();
}

Socket::HttpResponse WebServer::_makeGenericPage(unsigned short code)
{
	Socket::HttpResponse response;

	response.returnCode = code;
	response.codeName = WebServer::codes.at(response.returnCode);
	response.header["Content-Type"] = "text/html";
	response.body = "<html>"
		"<head>"
			"<title>" + response.codeName + "</title>"
		"</head>"
		"<body style=\"text-align: center\">"
			"<h1>" +
				std::to_string(response.returnCode) + ": " + response.codeName +
			"</h1>"
			"<hr>"
		"</body>"
	"</html>";
	return response;
}

Socket::HttpResponse WebServer::_makeGenericPage(unsigned short code, const std::string &extra)
{
	Socket::HttpResponse response;

	response.returnCode = code;
	response.codeName = WebServer::codes.at(response.returnCode);
	response.header["Content-Type"] = "text/html";
	response.body = "<html>"
		"<head>"
			"<title>" + response.codeName + "</title>"
		"</head>"
		"<body style=\"text-align: center\">"
			"<h1>" +
				std::to_string(response.returnCode) + ": " + response.codeName + " (" + extra + ")"
			"</h1>"
			"<hr>"
		"</body>"
	"</html>";
	return response;
}

Socket::HttpResponse WebServer::_checkFolders(const Socket::HttpRequest &request)
{
	Socket::HttpResponse response;

	if (request.method != "GET")
		throw AbortConnectionException(405);
	for (auto &[key, folder] : this->_folders) {
		if (request.realPath.size() < key.length())
			continue;
		if (request.realPath.size() == key.length() && request.realPath != key)
			continue;
		if (request.realPath.size() > key.length() && (
			request.realPath.substr(0, key.length()) != key ||
			request.realPath[key.size()] != '/'
		))
			continue;

		auto url = request.realPath.substr(key.length());

		// TODO: Handle this case better
		if (url.find("/../") != std::string::npos)
			throw AbortConnectionException(401);

		std::string realPath = folder.first + url;

		if (realPath.empty() || realPath.back() != '/' || !folder.second) {
			std::string type = WebServer::_getContentType(request.realPath);
			int i = std::ifstream::in;

			if (type.substr(0, 5) != "text/")
				i |= std::ifstream::binary;

			std::ifstream stream{realPath, i};

			if (stream.fail())
				throw AbortConnectionException(404);
			response.returnCode = 200;
			response.header["Cache-Control"] = "private, immutable, max-age=" + std::to_string(this->_staticAge);
			response.header["Content-Type"] = type;
			response.body = {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
			return response;
		} else {
			std::error_code err;
			std::filesystem::directory_iterator it{realPath, err};

			if (err)
				throw AbortConnectionException(404);

			auto accept = request.header.find("accept");

			if (accept == request.header.end() || accept->second != "application/json") {
				response.body = "<html>"
					"<head>"
						"<title>Index of " + url + "</title>"
					"</head>"
				"<body>"
					"<h1>Index of " + url + "</h1>"
					"<hr>"
					"<pre>";

				if (url != "/")
					response.body += "&#128193 <a href=\"../\">../</a>\n";

				for (auto &entry : it) {
					auto name = entry.path().filename().string();

					if (entry.is_directory())
						response.body += "&#128193 <a href=\"" + name + "/\">" + name + "/</a>";
					else
						response.body += "   <a href=\"" + name + "\">" + name + "</a>";
					response.body += "\n";
				}
				response.body += "</pre><hr></body></html>";
				response.returnCode = 200;
				response.header["Cache-Control"] = "private, immutable, max-age=" + std::to_string(this->_staticAge);
				response.header["Content-Type"] = "text/html";
			} else {
				nlohmann::json json = nlohmann::json::array();

				for (auto &entry : it) {
					auto name = entry.path().filename().string();

					if (entry.is_directory())
						name.push_back('/');
					json.push_back(name);
				}
				response.body = json.dump();
				response.returnCode = 200;
				response.header["Cache-Control"] = "private, immutable, max-age=0";
				response.header["Content-Type"] = "application/json";
			}
			return response;
		}
	}
	throw AbortConnectionException(404);
}

std::string WebServer::_getContentType(const std::string &path)
{
	//TODO: Fix bug if URL contains a .
	size_t pos = path.find_last_of('.');
	size_t pos2 = path.find_last_of('/');

	if (pos2 != std::string::npos && path.substr(0, pos2).find('.') != std::string::npos)
		throw AbortConnectionException(401);

	if (pos == std::string::npos)
		return "application/octet-stream";

	auto it = WebServer::types.find(path.substr(pos + 1));

	if (it == WebServer::types.end())
		return "application/" + path.substr(pos + 1);
	return it->second;
}

void WebServer::_addWebSocket(Socket &sock, const Socket::HttpRequest &requ)
{
	auto response = WebSocket::solveHandshake(requ);
	std::shared_ptr<WebSocketConnection> wsock;
	std::weak_ptr<WebSocketConnection> wsock_weak;

	this->_webSocks.push_back(std::make_shared<WebSocketConnection>(sock));
	wsock_weak = wsock = this->_webSocks.back();
	wsock->wsock.needsMask(false);
	response.httpVer = "HTTP/1.1";
	response.codeName = WebServer::codes.at(response.returnCode);
	sock.send(Socket::generateHttpResponse(response));
	if (this->_onConnect)
		this->_onConnect(wsock->wsock);
	wsock->isThreadFinished = false;
	wsock->thread = std::thread([this, wsock_weak]{
		try {
			while (wsock_weak.lock()->wsock.isOpen()) {
				std::string msg = wsock_weak.lock()->wsock.getAnswer();

				if (this->_onMessage)
					this->_onMessage(wsock_weak.lock()->wsock, msg);
			}
		} catch (const std::exception &e) {
			wsock_weak.lock()->wsock.disconnect();
			if (this->_onError)
				this->_onError(wsock_weak.lock()->wsock, e);
		}
		wsock_weak.lock()->isThreadFinished = true;
	});
#ifdef _DEBUG
	std::cout << inet_ntoa(sock.getRemote().sin_addr) << ":" << sock.getRemote().sin_port << " " << requ.path << ": " << response.returnCode << std::endl;
#endif
}

void WebServer::broadcast(const std::string &msg)
{
	this->_webSocks.erase(
		std::remove_if(
			this->_webSocks.begin(),
			this->_webSocks.end(),
			[](std::shared_ptr<WebSocketConnection> &s) {
				return s->isThreadFinished;
			}
		),
		this->_webSocks.end()
	);
	for (auto &wsock : this->_webSocks)
		try {
			wsock->wsock.send(msg);
		} catch (...) {
			wsock->wsock.disconnect();
		}
}

void WebServer::onWebSocketConnect(const std::function<void(WebSocket &)> & fct)
{
	this->_onConnect = fct;
}

void WebServer::onWebSocketMessage(const std::function<void(WebSocket &, const std::string &)> & fct)
{
	this->_onMessage = fct;
}

void WebServer::onWebSocketError(const std::function<void(WebSocket &, const std::exception &)> & fct)
{
	this->_onError = fct;
}

void WebServer::_parsePath(Socket::HttpRequest &req)
{
	auto pos = req.path.find_first_of('?');

	req.realPath = WebServer::_decodeURIComponent(req.path.substr(0, pos));
	if (pos != std::string::npos) {
		std::string queryString = req.path.substr(pos + 1);

		while (true) {
			size_t equ = queryString.find_first_of('=');
			std::string key = queryString.substr(0, equ);
			size_t end = queryString.find('&');

			if (equ != std::string::npos)
				req.query[key] = queryString.substr(equ + 1);
			else if (!queryString.empty() && end != 0)
				req.query[key] = "";
			if (end == std::string::npos)
				break;
			queryString = queryString.substr(end + 1);
		}
	}
}

std::string WebServer::_decodeURIComponent(const std::string &elem)
{
	std::string result;
	char digits[] = "0123456789ABCDEF";

	result.reserve(elem.size());
	for (size_t i = 0; i < elem.size(); i++) {
		if (elem[i] < ' ' || elem[i] > 126)
			throw AbortConnectionException(400);
		if (elem[i] == '+')
			result.push_back(' ');
		else if (elem[i] == '%') {
			if (elem.size() < 3 || i + 2 >= elem.size())
				throw AbortConnectionException(400);

			auto ptr1 = strchr(digits, toupper(elem[i + 1]));
			auto ptr2 = strchr(digits, toupper(elem[i + 2]));

			if (!ptr1 || !ptr2)
				throw AbortConnectionException(400);
			result.push_back(((ptr1 - digits) << 4) | (ptr2 - digits));
			i += 2;
		} else
			result.push_back(elem[i]);
	}
	return result;
}
