#pragma once
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace mk64_3ds {
// Caller supplies synchronization. Overflow discards complete oldest lines;
// appending never allocates memory or invokes a filesystem operation.
template <std::size_t Capacity> class RamLog {
  public:
    void Append(const char* text, std::size_t length) {
        if (length > Capacity) { ++dropped; return; }
        while (used + length > Capacity) {
            while (used != 0) {
                const char ch = bytes[head];
                head = (head + 1) % Capacity; --used;
                if (ch == '\n') break;
            }
            ++dropped;
        }
        const auto tail = (head + used) % Capacity;
        const auto first = length < Capacity - tail ? length : Capacity - tail;
        std::memcpy(bytes.data() + tail, text, first);
        std::memcpy(bytes.data(), text + first, length - first);
        used += length;
    }
    bool Flush(FILE* file) {
        if (!file) return false;
        if (dropped && std::fprintf(file, "[RAM log: %zu older lines discarded]\n", dropped) < 0) return false;
        const auto first = used < Capacity - head ? used : Capacity - head;
        if (std::fwrite(bytes.data() + head, 1, first, file) != first ||
            std::fwrite(bytes.data(), 1, used - first, file) != used - first ||
            std::fflush(file) != 0) return false;
        Clear();
        return true;
    }
    void Clear() { head = used = dropped = 0; }
    std::size_t Size() const { return used; }
  private:
    std::array<char, Capacity> bytes{};
    std::size_t head = 0, used = 0, dropped = 0;
};
}
