//
// Created by PinkySmile on 08/12/2020.
//

#include <nlohmann/json.hpp>
#include <sstream>
#include "Handlers.hpp"
#include "../State.hpp"
#include "../Exceptions.hpp"
#include "package.hpp"

extern ShadyCore::PackageEx package;

Socket::HttpResponse connectRoute(const Socket::HttpRequest &requ)
{
	if (requ.ip != 0x0100007F)
		throw AbortConnectionException(403);
	if (requ.method != "POST")
		throw AbortConnectionException(405);

	Socket::HttpResponse response;
	auto menuObj = SokuLib::getMenuObj<SokuLib::MenuConnect>();
	std::string ip;
	unsigned short hport;
	bool isSpec;

	try {
		auto json = nlohmann::json::parse(requ.body);

		ip = json["ip"];
		hport = json["port"];
		isSpec = json["spec"];
		if (inet_addr(ip.c_str()) == -1)
			throw std::exception();
	} catch (...) {
		throw AbortConnectionException(400);
	}

	if (!SokuLib::MenuConnect::isInNetworkMenu()) {
		SokuLib::MenuConnect::moveToConnectMenu();
		menuObj = SokuLib::getMenuObj<SokuLib::MenuConnect>();
	}

	if (
		menuObj->choice >= SokuLib::MenuConnect::CHOICE_ASSIGN_IP_CONNECT &&
		menuObj->choice < SokuLib::MenuConnect::CHOICE_SELECT_PROFILE &&
		menuObj->subchoice == 3
	)
		throw AbortConnectionException(503);
	if (
		menuObj->choice >= SokuLib::MenuConnect::CHOICE_HOST &&
		menuObj->choice < SokuLib::MenuConnect::CHOICE_SELECT_PROFILE &&
		menuObj->subchoice == 255
	)
		throw AbortConnectionException(503);
	if (
		menuObj->choice == SokuLib::MenuConnect::CHOICE_HOST &&
		menuObj->subchoice == 2
	)
		throw AbortConnectionException(503);

	menuObj->joinHost(ip.c_str(), hport, isSpec);
	response.returnCode = 202;
	return response;
}

Socket::HttpResponse root(const Socket::HttpRequest &requ)
{
	Socket::HttpResponse response;
	char buffer[1024] = {0};

	if (requ.method != "GET")
		throw AbortConnectionException(405);
	GetPrivateProfileStringA("Server", "DefaultPage", "", buffer, sizeof(buffer), profilePath);
	if (!*buffer)
		throw AbortConnectionException(404);
	response.header["Location"] = buffer;
	response.returnCode = 301;
	return response;
}

Socket::HttpResponse loadInternalAsset(const Socket::HttpRequest &requ)
{
	if (requ.path == "/internal")
		throw AbortConnectionException(501);

	auto path = requ.path.substr(strlen("/internal/"));
	auto it = package.find(path);
	Socket::HttpResponse response;

	if (it == package.end())
		throw AbortConnectionException(404);

	auto &stream = it.open();
	std::stringstream body;

	switch (it->first.fileType.type) {
	case ShadyCore::FileType::TYPE_UNKNOWN:
		break;
	case ShadyCore::FileType::TYPE_TEXT:
		break;
	case ShadyCore::FileType::TYPE_TABLE:
		break;
	case ShadyCore::FileType::TYPE_LABEL:
		break;
	case ShadyCore::FileType::TYPE_IMAGE: {
		ShadyCore::Image image;

		response.header["content-type"] = "image/png";
		ShadyCore::getResourceReader(it.fileType())(&image, stream);
		ShadyCore::getResourceWriter({ShadyCore::FileType::TYPE_IMAGE, ShadyCore::FileType::IMAGE_PNG})(&image, body);
		break;
	}
	case ShadyCore::FileType::TYPE_PALETTE:
		break;
	case ShadyCore::FileType::TYPE_SFX:
		break;
	case ShadyCore::FileType::TYPE_BGM:
		break;
	case ShadyCore::FileType::TYPE_SCHEMA:
		break;
	case ShadyCore::FileType::TYPE_TEXTURE:
		break;
	}
	it.close(stream);
	response.body = body.str();
	response.returnCode = 200;
	return response;
}

Socket::HttpResponse getCharName(const Socket::HttpRequest &requ)
{
	auto name = SokuLib::getCharName(std::stoul(requ.path.substr(strlen("/charName/"))));
	Socket::HttpResponse response;

	response.body = name;
	response.returnCode = 200;
	return response;
}

static void setState(const Socket::HttpRequest &requ)
{
	if (requ.ip != 0x0100007F)
		throw AbortConnectionException(403);
	try {
		auto result = nlohmann::json::parse(requ.body);

		if (result.contains("left")) {
			auto &chr = result["left"];

			if (chr.contains("name"))
				_cache.leftName = chr["name"];
			if (chr.contains("score"))
				_cache.leftScore = chr["score"];
			/*result["left"] = {
				{"character", cache.left},
				{"score",     cache.leftScore},
				{"name",      convertShiftJisToUTF8(cache.leftName.c_str())},
				{"used",      cache.leftUsed},
				{"deck",      leftDeck},
				{"hand",      leftHand},
				{"stats",     statsToJson(cache.leftStats)}
			};*/
		}
		if (result.contains("right")) {
			auto &chr = result["right"];

			if (chr.contains("name"))
				_cache.rightName = chr["name"];
			if (chr.contains("score"))
				_cache.rightScore = chr["score"];
		}
		if (result.contains("round"))
			_cache.round = result["round"];
	} catch (nlohmann::detail::exception &) {
		throw AbortConnectionException(400);
	}
	broadcastOpcode(STATE_UPDATE, cacheToJson(_cache));
	_cache.noReset = true;
}

Socket::HttpResponse state(const Socket::HttpRequest &requ)
{
	Socket::HttpResponse response;

	response.returnCode = 200;
	if (requ.method == "POST")
		return setState(requ), response;

	if (requ.method != "GET")
		throw AbortConnectionException(405);

	response.header["Content-Type"] = "application/json";
	response.body = cacheToJson(_cache);
	return response;
}

void onNewWebSocket(WebSocket &s)
{
	sendOpcode(s, STATE_UPDATE, cacheToJson(_cache));
}

void sendOpcode(WebSocket &s, Opcodes op, const std::string &data)
{
	std::string json = "{"
		"\"o\": " + std::to_string(op) + ","
		"\"d\": " + data +
	"}";

	s.send(json);
}

void broadcastOpcode(Opcodes op, const std::string &data)
{
	std::string json = "{"
		"\"o\": " + std::to_string(op) + ","
		"\"d\": " + data +
	"}";

	webServer->broadcast(json);
}
