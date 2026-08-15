#pragma once

#include <array>
#include <cstddef>
#include <utility>

namespace mk64_3ds {

template <typename Actor, std::size_t Capacity>
class BaseActorPool3DS {
  public:
    template <typename IsReusable>
    Actor* FindReusable(IsReusable&& isReusable) const {
        for (std::size_t index = 0; index < mCount; ++index) {
            Actor* actor = mSlots[index];
            if (actor != nullptr && std::forward<IsReusable>(isReusable)(actor)) {
                return actor;
            }
        }
        return nullptr;
    }

    bool Track(Actor* actor) {
        if (actor == nullptr || mCount >= Capacity || Ordinal(actor) != npos) {
            return false;
        }
        mSlots[mCount++] = actor;
        return true;
    }

    void Reset() {
        for (std::size_t index = 0; index < mCount; ++index) {
            mSlots[index] = nullptr;
        }
        mCount = 0;
    }

    std::size_t Ordinal(const Actor* actor) const {
        for (std::size_t index = 0; index < mCount; ++index) {
            if (mSlots[index] == actor) {
                return index;
            }
        }
        return npos;
    }

    bool IsDynamic(const Actor* actor, std::size_t permanentCount) const {
        const std::size_t ordinal = Ordinal(actor);
        return ordinal != npos && ordinal >= permanentCount;
    }

    bool Full() const {
        return mCount >= Capacity;
    }

    std::size_t Count() const {
        return mCount;
    }

    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

  private:
    std::array<Actor*, Capacity> mSlots = {};
    std::size_t mCount = 0;
};

} // namespace mk64_3ds
