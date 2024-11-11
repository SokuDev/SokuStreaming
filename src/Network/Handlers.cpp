//
// Created by PinkySmile on 08/12/2020.
//

#include <nlohmann/json.hpp>
#include <sstream>
#include <fstream>
#include "Handlers.hpp"
#include "../State.hpp"
#include "../Exceptions.hpp"

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
	nlohmann::json json;
	std::map<std::string, std::string> errors;

	try {
		json = nlohmann::json::parse(requ.body);
	} catch (std::exception &e) {
		throw AbortConnectionException(400, nlohmann::json{
			{"error", "JSON parsing error"},
			{"details", e.what()},
			{"body", requ.body}
		}.dump(), "application/json");
	}
	if (!json.contains("ip"))
		errors["ip"] = "This field is required";
	if (!json.contains("port"))
		errors["port"] = "This field is required";
	if (!json.contains("spec"))
		errors["spec"] = "This field is required";

	if (errors.begin() != errors.end())
		throw AbortConnectionException(400, nlohmann::json{errors}.dump(), "application/json");

	if (!json["ip"].is_string())
		errors["ip"] = "String expected but got " + std::string(json["ip"].type_name());
	if (!json["port"].is_number())
		errors["port"] = "Number expected but got " + std::string(json["port"].type_name());
	if (!json["spec"].is_boolean())
		errors["spec"] = "Boolean expected but got " + std::string(json["spec"].type_name());
	if (errors.begin() != errors.end())
		throw AbortConnectionException(400, nlohmann::json{errors}.dump(), "application/json");

	ip = json["ip"];
	hport = json["port"];
	isSpec = json["spec"];
	if (inet_addr(ip.c_str()) == -1)
		throw AbortConnectionException(400, nlohmann::json{{{"ip", "This field is invalid"}}}.dump(), "application/json");

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
	puts(buffer);
	if (!*buffer)
		throw AbortConnectionException(404);
	response.header["Location"] = buffer;
	response.returnCode = 302;
	return response;
}

const std::vector<std::string> convertedFormats{"txt", "csv", "lbl", "png", "bmp", "act", "wav", "xml"};
const std::map<std::string, std::string> gameMimeTypes{
	// Game format
	{ "cv0", "application/octet-stream" }, // TYPE_TEXT,    TEXT_GAME
	{ "cv1", "application/octet-stream" }, // TYPE_TABLE,   TABLE_GAME
	{ "sfl", "application/octet-stream" }, // TYPE_LABEL,   LABEL_RIFF
	{ "cv2", "application/octet-stream" }, // TYPE_IMAGE,   IMAGE_GAME
	{ "pal", "application/octet-stream" }, // TYPE_PALETTE, PALETTE_PAL
	{ "cv3", "application/octet-stream" }, // TYPE_SFX,     SFX_GAME
	{ "ogg", "audio/ogg" },                // TYPE_BGM,     BGM_OGG
	{ "dds", "application/octet-stream" }, // TYPE_TEXTURE, TEXTURE_DDS
	{ "dat", "application/octet-stream" }, // TYPE_SCHEMA,  SCHEMA_GAME_GUI
	{ "pat", "application/octet-stream" }, // TYPE_SCHEMA,  SCHEMA_GAME_PATTERN | SCHEMA_GAME_ANIM

	// Standard format
	{ "txt", "text/plain" },               // TYPE_TEXT,    TEXT_NORMAL
	{ "csv", "text/plain" },               // TYPE_TABLE,   TABLE_CSV
	{ "lbl", "text/plain" },               // TYPE_LABEL,   LABEL_LBL
	{ "png", "image/png" },                // TYPE_IMAGE,   IMAGE_PNG
	{ "bmp", "image/bmp" },                // TYPE_IMAGE,   IMAGE_BMP
	{ "act", "application/octet-stream" }, // TYPE_PALETTE, PALETTE_ACT
	{ "wav", "audio/wav" },                // TYPE_SFX,     SFX_WAV
	{ "xml", "application/xml" },          // TYPE_SCHEMA,  SCHEMA_XML
};
const std::map<std::string, std::string> gameFormatExtensions{
	// Standard format
	{ "txt", "cv0" },
	{ "csv", "cv1" },
	{ "lbl", "sfl" },
	{ "png", "cv2" },
	{ "bmp", "cv2" },
	{ "act", "pal" },
	{ "wav", "cv3" },
	{ "xml", "pat" }
};
const std::map<std::string, ShadyCore::FileType> gameFileTypes{
	{ "txt", {ShadyCore::FileType::TYPE_TEXT,    ShadyCore::FileType::TEXT_NORMAL} },
	{ "csv", {ShadyCore::FileType::TYPE_TABLE,   ShadyCore::FileType::TABLE_CSV} },
	{ "lbl", {ShadyCore::FileType::TYPE_LABEL,   ShadyCore::FileType::LABEL_LBL} },
	{ "png", {ShadyCore::FileType::TYPE_IMAGE,   ShadyCore::FileType::IMAGE_PNG} },
	{ "bmp", {ShadyCore::FileType::TYPE_IMAGE,   ShadyCore::FileType::IMAGE_BMP} },
	{ "act", {ShadyCore::FileType::TYPE_PALETTE, ShadyCore::FileType::PALETTE_ACT} },
	{ "wav", {ShadyCore::FileType::TYPE_SFX,     ShadyCore::FileType::SFX_WAV} },
	{ "xml", {ShadyCore::FileType::TYPE_SCHEMA,  ShadyCore::FileType::SCHEMA_XML} },
};

Socket::HttpResponse loadInternalAsset(const Socket::HttpRequest &requ)
{
	if (requ.realPath == "/internal")
		throw AbortConnectionException(501);

	auto path = requ.realPath.substr(strlen("/internal/"));
	auto pos = path.find_last_of('.');

	if (pos == std::string::npos)
		throw AbortConnectionException(404);

	auto ext = path.substr(pos + 1);
	SokuLib::PackageReader reader;
	Socket::HttpResponse response;
	char *buffer;
	auto it = gameFormatExtensions.find(ext);

	if (it != gameFormatExtensions.end())
		path = path.substr(0, pos + 1) + it->second;

	reader.open(path.c_str());
	if (!reader.isOpen() && ext == "xml") {
		path = path.substr(0, pos + 1) + "dat";
		reader.open(path.c_str());
	}
	if (!reader.isOpen())
		throw AbortConnectionException(404);
	buffer = new char[reader.GetLength()];
	reader.Read(buffer, reader.GetLength());
	reader.close();

	response.body = std::string{buffer, buffer + reader.GetLength()};
	if (std::find(convertedFormats.begin(), convertedFormats.end(), ext) != convertedFormats.end()) {
		std::stringstream input;
		std::stringstream body;
		auto fileType = gameFileTypes.at(ext);

		input.str(response.body);
		switch (fileType.type) {
		case ShadyCore::FileType::TYPE_IMAGE: {
			ShadyCore::Image image;

			ShadyCore::getResourceReader({ShadyCore::FileType::TYPE_IMAGE, ShadyCore::FileType::IMAGE_GAME})(&image, input);
			if (image.bitsPerPixel == 8 && requ.query.find("palette") != requ.query.end())
				throw AbortConnectionException(501);
			ShadyCore::getResourceWriter(fileType)(&image, body);
			break;
		}
		case ShadyCore::FileType::TYPE_TEXT:
		case ShadyCore::FileType::TYPE_TABLE:
		case ShadyCore::FileType::TYPE_LABEL:
		case ShadyCore::FileType::TYPE_PALETTE:
		case ShadyCore::FileType::TYPE_SFX:
		case ShadyCore::FileType::TYPE_BGM:
		case ShadyCore::FileType::TYPE_SCHEMA:
		case ShadyCore::FileType::TYPE_TEXTURE:
			throw AbortConnectionException(501);
		}
		response.body = body.str();
	}
	response.returnCode = 200;
	response.header["Content-Type"] = gameMimeTypes.at(ext);
	response.header["Cache-Control"] = "private, immutable, max-age=" + std::to_string(GetPrivateProfileIntA("Server", "Cache", 0, profilePath));
	return response;
}

Socket::HttpResponse getCharNames(const Socket::HttpRequest &)
{
	Socket::HttpResponse response;
	nlohmann::json json = nlohmann::json::object();

	for (auto id : availableCharacters)
		json[std::to_string(id)] = SokuLib::getCharName(id);
	response.body = json.dump();
	response.header["Cache-Control"] = "private, immutable, max-age=" + std::to_string(GetPrivateProfileIntA("Server", "Cache", 0, profilePath));
	response.header["Content-Type"] = "application/json";
	response.returnCode = 200;
	return response;
}

Socket::HttpResponse getCharName(const Socket::HttpRequest &requ)
{
	auto id = std::stoul(requ.realPath.substr(strlen("/charName/")));

	if (std::find(availableCharacters.begin(), availableCharacters.end(), id) == availableCharacters.end())
		throw AbortConnectionException(404);

	auto name = SokuLib::getCharName(id);
	Socket::HttpResponse response;

	response.header["Cache-Control"] = "private, immutable, max-age=" + std::to_string(GetPrivateProfileIntA("Server", "Cache", 0, profilePath));
	response.header["Content-Type"] = "text/plain";
	response.body = name;
	response.returnCode = 200;
	return response;
}

Socket::HttpResponse loadSkillSheet(const Socket::HttpRequest &requ)
{
	auto id = std::stoul(requ.realPath.substr(strlen("/skillSheet/")));

	if (std::find(availableCharacters.begin(), availableCharacters.end(), id) == availableCharacters.end())
		throw AbortConnectionException(404);

	auto name = SokuLib::getCharName(id);
	Socket::HttpResponse response;
	std::filesystem::path path;

	if (id < 20)
		path = std::filesystem::path(parentPath) / "skillSheets" / (name + std::string("Skills.png"));
	else
		path = std::filesystem::path(soku2Path) / "sheets" / (name + std::string("Skills.png"));

	std::ifstream stream{path, std::ifstream::binary};

	puts(path.string().c_str());
	if (stream.fail())
		throw AbortConnectionException(404);

	response.returnCode = 200;
	response.header["Cache-Control"] = "private, immutable, max-age=" + std::to_string(GetPrivateProfileIntA("Server", "Cache", 0, profilePath));
	response.header["Content-Type"] = "image/png";
	response.body = {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
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
	} catch (nlohmann::detail::exception &e) {
		throw AbortConnectionException(400, nlohmann::json{
			{"error", "JSON error"},
			{"details", e.what()},
			{"body", requ.body}
		}.dump(), "application/json");
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
