#include <libultraship/bridge/consolevariablebridge.h>

#include <string>
#include <unordered_map>

namespace {
std::unordered_map<std::string, int32_t> sIntegers;
std::unordered_map<std::string, float> sFloats;
std::unordered_map<std::string, std::string> sStrings;
std::unordered_map<std::string, Color_RGBA8> sColors;
std::unordered_map<std::string, Color_RGB8> sColors24;

template <typename Map>
void ClearPrefix(Map& map, const char* prefix) {
    const std::string value = prefix == nullptr ? "" : prefix;
    for (auto it = map.begin(); it != map.end();) {
        if (it->first.rfind(value, 0) == 0) {
            it = map.erase(it);
        } else {
            ++it;
        }
    }
}
}

extern "C" {

int32_t CVarGetInteger(const char* name, int32_t defaultValue) {
    const auto it = sIntegers.find(name == nullptr ? "" : name);
    return it == sIntegers.end() ? defaultValue : it->second;
}

float CVarGetFloat(const char* name, float defaultValue) {
    const auto it = sFloats.find(name == nullptr ? "" : name);
    return it == sFloats.end() ? defaultValue : it->second;
}

const char* CVarGetString(const char* name, const char* defaultValue) {
    const auto it = sStrings.find(name == nullptr ? "" : name);
    return it == sStrings.end() ? defaultValue : it->second.c_str();
}

Color_RGBA8 CVarGetColor(const char* name, Color_RGBA8 defaultValue) {
    const auto it = sColors.find(name == nullptr ? "" : name);
    return it == sColors.end() ? defaultValue : it->second;
}

Color_RGB8 CVarGetColor24(const char* name, Color_RGB8 defaultValue) {
    const auto it = sColors24.find(name == nullptr ? "" : name);
    return it == sColors24.end() ? defaultValue : it->second;
}

void CVarSetInteger(const char* name, int32_t value) {
    if (name != nullptr) sIntegers[name] = value;
}
void CVarSetFloat(const char* name, float value) {
    if (name != nullptr) sFloats[name] = value;
}
void CVarSetString(const char* name, const char* value) {
    if (name != nullptr) sStrings[name] = value == nullptr ? "" : value;
}
void CVarSetColor(const char* name, Color_RGBA8 value) {
    if (name != nullptr) sColors[name] = value;
}
void CVarSetColor24(const char* name, Color_RGB8 value) {
    if (name != nullptr) sColors24[name] = value;
}

void CVarRegisterInteger(const char* name, int32_t value) {
    if (name != nullptr) sIntegers.emplace(name, value);
}
void CVarRegisterFloat(const char* name, float value) {
    if (name != nullptr) sFloats.emplace(name, value);
}
void CVarRegisterString(const char* name, const char* value) {
    if (name != nullptr) sStrings.emplace(name, value == nullptr ? "" : value);
}
void CVarRegisterColor(const char* name, Color_RGBA8 value) {
    if (name != nullptr) sColors.emplace(name, value);
}
void CVarRegisterColor24(const char* name, Color_RGB8 value) {
    if (name != nullptr) sColors24.emplace(name, value);
}

void CVarClear(const char* name) {
    if (name == nullptr) return;
    sIntegers.erase(name);
    sFloats.erase(name);
    sStrings.erase(name);
    sColors.erase(name);
    sColors24.erase(name);
}

bool CVarExists(const char* name) {
    if (name == nullptr) return false;
    return sIntegers.count(name) != 0 || sFloats.count(name) != 0 || sStrings.count(name) != 0 ||
           sColors.count(name) != 0 || sColors24.count(name) != 0;
}

void CVarClearBlock(const char* name) {
    ClearPrefix(sIntegers, name);
    ClearPrefix(sFloats, name);
    ClearPrefix(sStrings, name);
    ClearPrefix(sColors, name);
    ClearPrefix(sColors24, name);
}

void CVarCopy(const char* from, const char* to) {
    if (from == nullptr || to == nullptr) return;
    if (const auto it = sIntegers.find(from); it != sIntegers.end()) sIntegers[to] = it->second;
    if (const auto it = sFloats.find(from); it != sFloats.end()) sFloats[to] = it->second;
    if (const auto it = sStrings.find(from); it != sStrings.end()) sStrings[to] = it->second;
    if (const auto it = sColors.find(from); it != sColors.end()) sColors[to] = it->second;
    if (const auto it = sColors24.find(from); it != sColors24.end()) sColors24[to] = it->second;
}

void CVarLoad() {}
void CVarSave() {}

}
