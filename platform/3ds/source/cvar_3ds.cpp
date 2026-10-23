#include <libultraship/bridge/consolevariablebridge.h>

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace {
// Game and renderer code reads CVars by literal name many times per frame.
// Transparent lookup hashes the C string in place; the previous std::string
// key construction cost a heap allocation, free and newlib lock per read.
struct NameHash {
    using is_transparent = void;
    size_t operator()(std::string_view name) const noexcept {
        return std::hash<std::string_view>{}(name);
    }
};
template <typename T>
using CVarMap = std::unordered_map<std::string, T, NameHash, std::equal_to<>>;

CVarMap<int32_t> sIntegers;
CVarMap<float> sFloats;
CVarMap<std::string> sStrings;
CVarMap<Color_RGBA8> sColors;
CVarMap<Color_RGB8> sColors24;

std::string_view NameOrEmpty(const char* name) {
    return name == nullptr ? std::string_view() : std::string_view(name);
}

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
    const auto it = sIntegers.find(NameOrEmpty(name));
    return it == sIntegers.end() ? defaultValue : it->second;
}

float CVarGetFloat(const char* name, float defaultValue) {
    const auto it = sFloats.find(NameOrEmpty(name));
    return it == sFloats.end() ? defaultValue : it->second;
}

const char* CVarGetString(const char* name, const char* defaultValue) {
    const auto it = sStrings.find(NameOrEmpty(name));
    return it == sStrings.end() ? defaultValue : it->second.c_str();
}

Color_RGBA8 CVarGetColor(const char* name, Color_RGBA8 defaultValue) {
    const auto it = sColors.find(NameOrEmpty(name));
    return it == sColors.end() ? defaultValue : it->second;
}

Color_RGB8 CVarGetColor24(const char* name, Color_RGB8 defaultValue) {
    const auto it = sColors24.find(NameOrEmpty(name));
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
    const std::string_view key(name);
    return sIntegers.find(key) != sIntegers.end() || sFloats.find(key) != sFloats.end() ||
           sStrings.find(key) != sStrings.end() || sColors.find(key) != sColors.end() ||
           sColors24.find(key) != sColors24.end();
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
