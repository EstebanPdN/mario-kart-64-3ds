#include "base_actor_pool_3ds.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>

namespace {

struct ProbeActor {
    int type = 1;
    int flags = 1;
};

[[noreturn]] void Fail(const char* expression, int line) {
    std::fprintf(stderr, "base actor pool probe failed at line %d: %s\n", line,
                 expression);
    std::abort();
}

#define CHECK(expression)                \
    do {                                 \
        if (!(expression)) {             \
            Fail(#expression, __LINE__); \
        }                                \
    } while (false)

void TestCapacityAndStableReuse() {
    constexpr std::size_t kCapacity = 100;
    std::array<ProbeActor, kCapacity + 1> actors = {};
    mk64_3ds::BaseActorPool3DS<ProbeActor, kCapacity> pool;

    for (std::size_t index = 0; index < kCapacity; ++index) {
        CHECK(pool.Track(&actors[index]));
        CHECK(pool.Ordinal(&actors[index]) == index);
    }
    CHECK(pool.Count() == kCapacity);
    CHECK(pool.Full());
    CHECK(!pool.Track(&actors[kCapacity]));
    CHECK(!pool.Track(&actors[37]));

    actors[37].flags = 0;
    actors[37].type = 0;
    ProbeActor* reused = pool.FindReusable([](const ProbeActor* actor) {
        return actor->flags == 0 && actor->type == 0;
    });
    CHECK(reused == &actors[37]);
    CHECK(pool.Ordinal(reused) == 37);
    CHECK(pool.Count() == kCapacity);
}

void TestPermanentBoundaryAndReset() {
    std::array<ProbeActor, 8> actors = {};
    ProbeActor wrapper;
    mk64_3ds::BaseActorPool3DS<ProbeActor, 8> pool;

    for (ProbeActor& actor : actors) {
        CHECK(pool.Track(&actor));
    }
    CHECK(pool.Ordinal(&wrapper) == decltype(pool)::npos);
    CHECK(!pool.IsDynamic(&actors[2], 3));
    CHECK(pool.IsDynamic(&actors[3], 3));
    CHECK(pool.IsDynamic(&actors[7], 3));
    CHECK(!pool.IsDynamic(&wrapper, 0));

    pool.Reset();
    CHECK(pool.Count() == 0);
    CHECK(!pool.Full());
    CHECK(pool.Ordinal(&actors[0]) == decltype(pool)::npos);
    CHECK(pool.FindReusable([](const ProbeActor*) { return true; }) == nullptr);
    CHECK(pool.Track(&wrapper));
    CHECK(pool.Ordinal(&wrapper) == 0);
}

} // namespace

int main() {
    TestCapacityAndStableReuse();
    TestPermanentBoundaryAndReset();
    std::puts("base actor pool policy: ok");
    return 0;
}
