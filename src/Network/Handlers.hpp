//
// Created by PinkySmile on 08/12/2020.
//

#ifndef SWRSTOYS_HANDLERS_HPP
#define SWRSTOYS_HANDLERS_HPP


#include "WebServer.hpp"
#include "package.hpp"

enum Opcodes {
	STATE_UPDATE,   // 0
	CARDS_UPDATE,   // 1
	L_SCORE_UPDATE, // 2
	R_SCORE_UPDATE, // 3
	L_CARDS_UPDATE, // 4
	R_CARDS_UPDATE, // 5
	L_NAME_UPDATE,  // 6
	R_NAME_UPDATE,  // 7
	L_STATS_UPDATE, // 8
	R_STATS_UPDATE, // 9
};

Socket::HttpResponse root(const Socket::HttpRequest &requ);
Socket::HttpResponse state(const Socket::HttpRequest &requ);
Socket::HttpResponse getCharName(const Socket::HttpRequest &requ);
Socket::HttpResponse connectRoute(const Socket::HttpRequest &requ);
Socket::HttpResponse loadSkillSheet(const Socket::HttpRequest &requ);
Socket::HttpResponse loadInternalAsset(const Socket::HttpRequest &requ);
void onNewWebSocket(WebSocket &s);
void sendOpcode(WebSocket &s, Opcodes op, const std::string &data);
void broadcastOpcode(Opcodes op, const std::string &data);

extern char profilePath[1024 + MAX_PATH];
extern char parentPath[1024 + MAX_PATH];
extern wchar_t soku2Path[1024 + MAX_PATH];
extern ShadyCore::PackageEx *package;

#endif //SWRSTOYS_HANDLERS_HPP
