//
// Created by PinkySmile on 08/12/2020.
//

#include <SokuLib.hpp>
#include <dinput.h>
#include <iostream>
#include "Network/Handlers.hpp"
#include "nlohmann/json.hpp"
#include "Utils/ShiftJISDecoder.hpp"
#include "Utils/InputBox.hpp"
#include "State.hpp"

#define checkKey(key) (GetKeyState(keys[key]) & 0x8000)

bool enabled;
unsigned short port;
std::vector<unsigned> keys(TOTAL_NB_OF_KEYS);
std::unique_ptr<WebServer> webServer;
struct CachedMatchData _cache;
bool needReset;
bool needRefresh;
int (__thiscall SokuLib::BattleManager::*s_origCBattleManager_Render)();
int (__thiscall SokuLib::BattleManager::*s_origCBattleManager_Start)();
int (__thiscall SokuLib::BattleManager::*s_origCBattleManager_KO)();
int (__thiscall LoadingWatch::*s_origCLoadingWatch_Process)();
int (__thiscall BattleWatch::*s_origCBattleWatch_Process)();
int (__thiscall Loading::*s_origCLoading_Process)();
int (__thiscall Battle::*s_origCBattle_Process)();
int (__thiscall Title::*s_origCTitle_Process)();
HWND myWindow;

const char *jpTitle = "ôîò√ö±æzôVæÑ ü` Æ┤£WïëâMâjâçâïé╠ôΣé≡Æ╟éª Ver1.10a";

bool threadUsed = false;
std::thread thread;
std::vector<bool> oldState;
bool isPlaying = false;

void checkKeyInputs()
{
	std::vector<bool> isPressed;

	if (!threadUsed && thread.joinable())
		thread.join();

	if (GetForegroundWindow() != SokuLib::window)
		return;

	isPressed.reserve(TOTAL_NB_OF_KEYS);
	oldState.resize(TOTAL_NB_OF_KEYS);
	for (int i = 0; i < TOTAL_NB_OF_KEYS; i++) {
		auto val = checkKey(i);

		isPressed.push_back(val && !oldState[i]);
		oldState[i] = val;
	}

	if (isPressed[KEY_DECREASE_L_SCORE]) {
		_cache.leftScore--;
		broadcastOpcode(L_SCORE_UPDATE, std::to_string(_cache.leftScore));
	}
	if (isPressed[KEY_DECREASE_R_SCORE]) {
		_cache.rightScore--;
		broadcastOpcode(R_SCORE_UPDATE, std::to_string(_cache.rightScore));
	}
	if (isPressed[KEY_INCREASE_L_SCORE]) {
		_cache.leftScore++;
		broadcastOpcode(L_SCORE_UPDATE, std::to_string(_cache.leftScore));
	}
	if (isPressed[KEY_INCREASE_R_SCORE]) {
		_cache.rightScore++;
		broadcastOpcode(R_SCORE_UPDATE, std::to_string(_cache.rightScore));
	}
	if (isPressed[KEY_CHANGE_L_NAME]) {
		if (!threadUsed) {
			threadUsed = true;
			if (thread.joinable())
				thread.join();
			thread = std::thread{[]{
				auto answer = InputBox("Change left player name", "Left name", _cache.leftName);

				if (answer.empty()) {
					threadUsed = false;
					return;
				}
				_cache.leftName = answer;
				broadcastOpcode(L_NAME_UPDATE, "\"" + answer + "\"");
				threadUsed = false;
			}};
		}
	}
	if (isPressed[KEY_CHANGE_R_NAME]) {
		if (!threadUsed) {
			threadUsed = true;
			if (thread.joinable())
				thread.join();
			thread = std::thread{[]{
				auto answer = InputBox("Change right player name", "Right name",  _cache.rightName);

				if (answer.empty()) {
					threadUsed = false;
					return;
				}
				_cache.rightName = answer;
				broadcastOpcode(R_NAME_UPDATE, "\"" + answer + "\"");
				threadUsed = false;
			}};
		}
	}
	if (isPressed[KEY_RESET_SCORES]) {
		_cache.leftScore = 0;
		_cache.rightScore = 0;
		broadcastOpcode(L_SCORE_UPDATE, std::to_string(_cache.leftScore));
		broadcastOpcode(R_SCORE_UPDATE, std::to_string(_cache.rightScore));
	}
	if (isPressed[KEY_RESET_STATE]) {
		_cache = CachedMatchData();
		broadcastOpcode(STATE_UPDATE, cacheToJson(_cache));
		needRefresh = true;
		needReset = true;
	}
}

nlohmann::json statsToJson(const Stats &stats)
{
	nlohmann::json result = {
		{"doll",     stats.doll},
		{"rod",      stats.rod},
		{"grimoire", stats.grimoire},
		{"fan",      stats.fan},
		{"drops",    stats.drops}
	};
	std::map<std::string, unsigned char> skillMap;

	for (int i = 0; i < 16; i++) {
		if (!stats.skillMap[i].notUsed)
			skillMap[std::to_string(i)] = stats.skillMap[i].level;
	}

	result["skills"] = skillMap;
	return result;
}

std::string statsToString(const Stats &stats)
{
	return statsToJson(stats).dump(-1, ' ', true);
}

void updateCache(bool isMultiplayer)
{
	if (!isPlaying)
		return;

	auto &battleMgr = SokuLib::getBattleMgr();

	if (needReset) {
		if (_cache.noReset);
		else if (isMultiplayer) {
			auto &netObj = SokuLib::getNetObject();

			if (_cache.realLeftName != netObj.profile1name || _cache.realRightName != netObj.profile2name) {
				_cache.leftScore = 0;
				_cache.rightScore = 0;
				_cache.leftName = netObj.profile1name;
				_cache.rightName = netObj.profile2name;
				_cache.realLeftName = netObj.profile1name;
				_cache.realRightName = netObj.profile2name;
			}
		} else if (
			_cache.realLeftName != SokuLib::profile1.name.operator char *() ||
			_cache.realRightName != SokuLib::profile2.name.operator char *() ||
			SokuLib::subMode != SokuLib::BATTLE_SUBMODE_REPLAY
		) {
			_cache.leftScore = 0;
			_cache.rightScore = 0;
			_cache.leftName = static_cast<const char *>(SokuLib::profile1.name);
			_cache.rightName = static_cast<const char *>(SokuLib::profile2.name);
			_cache.realLeftName = static_cast<const char *>(SokuLib::profile1.name);
			_cache.realRightName = static_cast<const char *>(SokuLib::profile2.name);
		}
		needReset = false;
	}
	_cache.noReset = false;

	auto oldLeftHand = _cache.leftHand;
	auto oldRightHand = _cache.rightHand;

	_cache.leftCards.clear();
	_cache.rightCards.clear();
	_cache.leftHand.clear();
	_cache.rightHand.clear();
	_cache.leftUsed.clear();
	_cache.rightUsed.clear();

	auto &leftDeck = battleMgr.leftCharacterManager.deckInfo;
	auto &rightDeck = battleMgr.rightCharacterManager.deckInfo;
	auto &leftHand = battleMgr.leftCharacterManager.hand;
	auto &rightHand = battleMgr.rightCharacterManager.hand;

	// Left remaining cards
	for (int i = 0; i < leftDeck.deckCopy.size; i++)
		_cache.leftUsed.push_back(leftDeck.deckCopy[i]);
	for (int i = 0; i < leftDeck.deck.size; i++) {
		_cache.leftUsed.erase(std::find(_cache.leftUsed.begin(), _cache.leftUsed.end(), leftDeck.deck[i]));
		_cache.leftCards.push_back(leftDeck.deck[i]);
	}
	std::sort(_cache.leftCards.begin(), _cache.leftCards.end());

	// Right remaining cards
	for (int i = 0; i < rightDeck.deckCopy.size; i++)
		_cache.rightUsed.push_back(rightDeck.deckCopy[i]);
	for (int i = 0; i < rightDeck.deck.size; i++) {
		_cache.rightUsed.erase(std::find(_cache.rightUsed.begin(), _cache.rightUsed.end(), rightDeck.deck[i]));
		_cache.rightCards.push_back(rightDeck.deck[i]);
	}
	std::sort(_cache.rightCards.begin(), _cache.rightCards.end());

	//Hands
	for (int i = 0; i < battleMgr.leftCharacterManager.cardCount; i++)
		_cache.leftHand.push_back(leftHand.handCardBase[(i + leftHand.selectedCard) % leftHand.handCardMax]->id);
	for (int i = 0; i < battleMgr.rightCharacterManager.cardCount; i++)
		_cache.rightHand.push_back(rightHand.handCardBase[(i + rightHand.selectedCard) % rightHand.handCardMax]->id);
	std::sort(_cache.leftHand.begin(), _cache.leftHand.end());
	std::sort(_cache.rightHand.begin(), _cache.rightHand.end());
	std::sort(_cache.leftUsed.begin(), _cache.leftUsed.end());
	std::sort(_cache.rightUsed.begin(), _cache.rightUsed.end());

	if (_cache.leftHand.size() != oldLeftHand.size() && !needRefresh)
		broadcastOpcode(L_CARDS_UPDATE, generateLeftCardsJson(_cache));
	if (_cache.rightHand.size() != oldRightHand.size() && !needRefresh)
		broadcastOpcode(R_CARDS_UPDATE, generateRightCardsJson(_cache));

	if (_cache.weather != SokuLib::activeWeather) {
		auto old = _cache.weather;

		_cache.weather = SokuLib::activeWeather;
		if (old == SokuLib::WEATHER_MOUNTAIN_VAPOR || SokuLib::activeWeather == SokuLib::WEATHER_MOUNTAIN_VAPOR)
			if (!needRefresh)
				broadcastOpcode(CARDS_UPDATE, generateCardsJson(_cache));
	}

	Stats newStats;

	newStats.doll =     battleMgr.leftCharacterManager.sacrificialDolls;
	newStats.drops =    battleMgr.leftCharacterManager.drops;
	newStats.rod =      battleMgr.leftCharacterManager.controlRod;
	newStats.fan =      battleMgr.leftCharacterManager.tenguFans;
	newStats.grimoire = battleMgr.leftCharacterManager.grimoires;
	std::memcpy(newStats.skillMap, battleMgr.leftCharacterManager.skillMap, sizeof(newStats.skillMap));
	if (memcmp(&newStats, &_cache.leftStats, sizeof(newStats)) != 0) {
		std::memcpy(&_cache.leftStats, &newStats, sizeof(newStats));
		if (!needRefresh)
			broadcastOpcode(L_STATS_UPDATE, statsToString(newStats));
	}

	newStats.doll =     battleMgr.rightCharacterManager.sacrificialDolls;
	newStats.drops =    battleMgr.rightCharacterManager.drops;
	newStats.rod =      battleMgr.rightCharacterManager.controlRod;
	newStats.fan =      battleMgr.rightCharacterManager.tenguFans;
	newStats.grimoire = battleMgr.rightCharacterManager.grimoires;
	std::memcpy(newStats.skillMap, battleMgr.rightCharacterManager.skillMap, sizeof(newStats.skillMap));
	if (memcmp(&newStats, &_cache.rightStats, sizeof(newStats)) != 0) {
		std::memcpy(&_cache.rightStats, &newStats, sizeof(newStats));
		if (!needRefresh)
			broadcastOpcode(R_STATS_UPDATE, statsToString(newStats));
	}

	if (needRefresh) {
		_cache.left = SokuLib::leftChar;
		_cache.right = SokuLib::rightChar;
		needRefresh = false;
		broadcastOpcode(STATE_UPDATE, cacheToJson(_cache));
	}
	checkKeyInputs();
}

std::string cacheToJson(CachedMatchData cache)
{
	nlohmann::json result;
	std::vector<unsigned short> leftDeck;
	std::vector<unsigned short> rightDeck;
	std::vector<unsigned short> leftHand;
	std::vector<unsigned short> rightHand;

	if (cache.weather == SokuLib::WEATHER_MOUNTAIN_VAPOR) {
		leftDeck.resize(cache.leftCards.size() + cache.leftHand.size(), 21);
		rightDeck.resize(cache.rightCards.size() + cache.rightHand.size(), 21);
	} else {
		leftDeck = cache.leftCards;
		rightDeck = cache.rightCards;
		leftHand = cache.leftHand;
		rightHand = cache.rightHand;
	}

	result["left"] = {
		{ "character", cache.left },
		{ "score",     cache.leftScore },
		{ "name",      convertShiftJisToUTF8(cache.leftName.c_str()) },
		{ "used",      cache.leftUsed },
		{ "deck",      leftDeck },
		{ "hand",      leftHand },
		{ "stats",     statsToJson(cache.leftStats) }
	};
	result["right"] = {
		{ "character", cache.right },
		{ "score",     cache.rightScore },
		{ "name",      convertShiftJisToUTF8(cache.rightName.c_str()) },
		{ "used",      cache.rightUsed },
		{ "deck",      rightDeck },
		{ "hand",      rightHand },
		{ "stats",     statsToJson(cache.rightStats) }
	};
	result["round"] = cache.round;
	return result.dump(-1, ' ', true);
}

std::string generateCardsJson(CachedMatchData cache)
{
	nlohmann::json result;
	std::vector<unsigned short> leftDeck;
	std::vector<unsigned short> rightDeck;
	std::vector<unsigned short> leftHand;
	std::vector<unsigned short> rightHand;

	if (cache.weather == SokuLib::WEATHER_MOUNTAIN_VAPOR) {
		leftDeck.resize(cache.leftCards.size() + cache.leftHand.size(), 21);
		rightDeck.resize(cache.rightCards.size() + cache.rightHand.size(), 21);
	} else {
		leftDeck = cache.leftCards;
		rightDeck = cache.rightCards;
		leftHand = cache.leftHand;
		rightHand = cache.rightHand;
	}

	result["left"] = {
		{ "used", cache.leftUsed },
		{ "deck", leftDeck },
		{ "hand", leftHand },
	};
	result["right"] = {
		{ "used", cache.rightUsed },
		{ "deck", rightDeck },
		{ "hand", rightHand },
	};
	return result.dump(-1, ' ', true);
}

std::string generateRightCardsJson(CachedMatchData cache)
{
	nlohmann::json result;
	std::vector<unsigned short> rightDeck;
	std::vector<unsigned short> rightHand;

	if (cache.weather == SokuLib::WEATHER_MOUNTAIN_VAPOR)
		rightDeck.resize(cache.rightCards.size() + cache.rightHand.size(), 21);
	else {
		rightDeck = cache.rightCards;
		rightHand = cache.rightHand;
	}

	result = {
		{ "used", cache.rightUsed },
		{ "deck", rightDeck },
		{ "hand", rightHand },
	};
	return result.dump(-1, ' ', true);
}

std::string generateLeftCardsJson(CachedMatchData cache)
{
	nlohmann::json result;
	std::vector<unsigned short> leftDeck;
	std::vector<unsigned short> leftHand;

	if (cache.weather == SokuLib::WEATHER_MOUNTAIN_VAPOR)
		leftDeck.resize(cache.leftCards.size() + cache.leftHand.size(), 21);
	else {
		leftDeck = cache.leftCards;
		leftHand = cache.leftHand;
	}

	result = {
		{ "used", cache.leftUsed },
		{ "deck", leftDeck },
		{ "hand", leftHand },
	};
	return result.dump(-1, ' ', true);
}

void onRoundStart()
{
	isPlaying = true;
	_cache.oldLeftScore = 0;
	_cache.oldRightScore = 0;
}

void onKO()
{
	auto &battleMgr = SokuLib::getBattleMgr();

	isPlaying = false;

	if (
		_cache.oldLeftScore != battleMgr.leftCharacterManager.score ||
		_cache.oldRightScore != battleMgr.rightCharacterManager.score
	) {
		if (battleMgr.leftCharacterManager.score == 2) {
			_cache.leftScore++;
			broadcastOpcode(L_SCORE_UPDATE, std::to_string(_cache.leftScore));
		} else if (battleMgr.rightCharacterManager.score == 2) {
			_cache.rightScore++;
			broadcastOpcode(R_SCORE_UPDATE, std::to_string(_cache.rightScore));
		}
		_cache.oldLeftScore = battleMgr.leftCharacterManager.score;
		_cache.oldRightScore = battleMgr.rightCharacterManager.score;
	}
}
