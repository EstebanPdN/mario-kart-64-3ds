#pragma once

#include <array>
#include <cstdio>
#include <new>
#include <stdexcept>

namespace mk64_3ds {
class TextureCachePressure : public std::bad_alloc {
public:
    const char* what() const noexcept override { return "3DS texture cache working-set limit"; }
};

class TextureAllocationFailure : public std::bad_alloc {
public:
    TextureAllocationFailure(unsigned width, unsigned height) noexcept {
        std::snprintf(message, sizeof(message), "3DS linear texture allocation failed: %ux%u RGBA8",
                      width, height);
    }
    const char* what() const noexcept override { return message; }
private:
    char message[96] = {};
};

// Citro3D submits display transfers only for linked targets with used=true.
// Suppress those transfers for an internal batch, then restore them after the
// real GPU completion fence. Color/depth buffers remain untouched, and only
// the final EndFrame presents the completed image. No allocations are allowed.
template <typename Target, typename Clean, typename Submit, typename Wait>
bool FenceTextureBatch(Target* target, Clean clean, Submit submit, Wait wait) {
    if (target == nullptr) return false;
    while (target->prev != nullptr) target = target->prev;
    struct OutputHold {
        std::array<Target*, 3> targets = {};
        std::array<bool, 3> used = {};
        size_t count = 0;
        ~OutputHold() {
            for (size_t i = 0; i < count; ++i) targets[i]->used = used[i];
        }
    } hold;
    // Count before masking anything, so an invalid topology fails atomically.
    size_t outputs = 0;
    for (auto* current = target; current != nullptr; current = current->next)
        outputs += current->linked;
    if (outputs > hold.targets.size()) throw std::logic_error("3DS output target count exceeds three");
    clean();
    for (auto* current = target; current != nullptr; current = current->next) {
        if (!current->linked) continue;
        hold.targets[hold.count] = current;
        hold.used[hold.count++] = current->used;
        current->used = false;
    }
    submit();
    return wait();
}
} // namespace mk64_3ds
