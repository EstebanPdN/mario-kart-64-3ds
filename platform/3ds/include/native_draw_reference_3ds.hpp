#pragma once
#include <cstdint>
namespace mk64_3ds {
enum class NativeDrawFamily : uint8_t { Character, Vegetation, ObjectState, Particle };
struct NativeDrawReference {NativeDrawFamily family;uint16_t index;};
}
