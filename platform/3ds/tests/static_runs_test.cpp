#include "static_runs_3ds.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
constexpr std::size_t kSlots = 68;
using Cache = mk64_3ds::StaticRunCache<kSlots>;
std::size_t gLive = 0;
void* Allocate(std::size_t bytes) { ++gLive; return std::malloc(bytes); }
void Release(void* pointer) { --gLive; std::free(pointer); }
bool Publish(const void*, std::size_t) { return true; }

struct Vtx { std::int16_t ob[3]; std::uint16_t flag; std::int16_t tc[2]; std::uint8_t cn[4]; };
static_assert(sizeof(Vtx) == 16);
struct Cmd { std::uint32_t w0, w1; };

mk64_3ds::StaticRunVertex Snapshot(const Vtx& v) {
    mk64_3ds::StaticRunVertex out{};
    for (int i = 0; i < 3; ++i) out.position[i] = v.ob[i];
    out.position[3] = 1.0f;
    out.texcoord0[0] = out.texcoord1[0] = v.tc[0];
    out.texcoord0[1] = out.texcoord1[1] = v.tc[1];
    std::memcpy(out.color, v.cn, 4);
    return out;
}
void Load(Cache& cache, const Vtx* vertices, std::size_t count, std::size_t first, std::uint32_t signature) {
    cache.NoteLoadRange(vertices, count, first);
    for (std::size_t i = 0; i < count; ++i) cache.NoteLoad(first + i, &vertices[i], signature, Snapshot(vertices[i]));
}
#define MAKE_CACHE(name, budget) Cache name((budget), 64); name.SetAllocator({Allocate, Release, Publish})
} // namespace

int main() {
    // All RGB/alpha source combinations and byte values match the packed
    // renderer, including mixed shade/primitive alpha and opaque materials.
    for (int shadeRgb = 0; shadeRgb < 2; ++shadeRgb)
        for (int shadeAlpha = 0; shadeAlpha < 2; ++shadeAlpha)
            for (int alpha = 0; alpha < 2; ++alpha)
                for (int value = 0; value < 256; ++value) {
                    const float input[4] = {value / 255.0f, (255 - value) / 255.0f, 0.25f, value / 255.0f};
                    float mask[4], constant[4];
                    mk64_3ds::StaticRunColor(input, shadeRgb, shadeAlpha, alpha, mask, constant);
                    for (int channel = 0; channel < 4; ++channel) {
                        const int shade = (value + 71 * channel) % 256;
                        const bool varying = channel == 3 ? alpha && shadeAlpha : shadeRgb;
                        const int expected = channel == 3 && !alpha ? 255 : varying ? shade :
                            static_cast<int>(input[channel] * 255.0f + 0.5f);
                        assert(mask[channel] * shade + constant[channel] == expected);
                    }
                }
    // Cached camera coefficients preserve the previous double-precision
    // construction exactly. Changing a matrix or aspect must refresh them.
    {
        mk64_3ds::StaticRunMatrix prepared;
        mk64_3ds::StaticRunTransform transform;
        for (int view = 0; view < 16; ++view) {
            for (int row = 0; row < 4; ++row)
                for (int col = 0; col < 4; ++col) transform.matrix[row][col] = (view * 17 - row * 8 + col) / 13.0f;
            transform.aspect = view % 2 ? 0.75f : 1.0f;
            for (int run = 0; run < 200; ++run) {
                const float center[3] = {float(run * 53 - 4000), float(run % 15), float(-run * 91)};
                float actual[4][4], expected[4][4];
                prepared.Rows(transform, center, actual);
                double rows[4][4];
                for (int i = 0; i < 4; ++i) {
                    const double x = transform.matrix[i][0], y = transform.matrix[i][1];
                    const double z = transform.matrix[i][2], w = transform.matrix[i][3];
                    rows[0][i] = x * transform.aspect; rows[1][i] = y;
                    rows[2][i] = (z - w) * 0.5; rows[3][i] = w;
                }
                for (int row = 0; row < 4; ++row) {
                    double translation = rows[row][3];
                    for (int i = 0; i < 3; ++i) translation += center[i] * rows[row][i];
                    for (int i = 0; i < 3; ++i) expected[row][i] = static_cast<float>(rows[row][i]);
                    expected[row][3] = static_cast<float>(translation);
                }
                assert(std::memcmp(actual, expected, sizeof(actual)) == 0);
            }
            assert(prepared.Preparations() == static_cast<unsigned>(view + 1));
        }
    }
    Vtx quad[4] = {{{100, 0, 100}, 0, {0, 0}, {255, 0, 0, 255}},
                   {{110, 0, 100}, 0, {32, 0}, {0, 255, 0, 255}},
                   {{110, 0, 110}, 0, {32, 32}, {0, 0, 255, 255}},
                   {{100, 0, 110}, 0, {0, 32}, {255, 255, 255, 255}}};
    Cmd commands[4] = {{0x32000000, 1}, {0, 0}, {0xB1000204, 0x00020604}, {0xB8000000, 0}};
    const void* start = &commands[0];
    const void* end = &commands[3];
    // Capture a run, then replay it with identical provenance.
    {
        MAKE_CACHE(cache, 1 << 20);
        cache.BeginFrame(1);
        cache.BeginCapture(start, 7);
        Load(cache, quad, 4, 0, 7);
        cache.CaptureTriangle(0, 1, 2);
        cache.CaptureTriangle(0, 2, 3);
        assert(cache.EndCapture(end));
        assert(cache.Counters().captures == 1 && cache.Bytes() > 0 && gLive == 1);
        const auto* entry = cache.Find(start, 7);
        assert(entry && entry->vertexCount == 4 && entry->indexCount == 6 && entry->end == end);
        // Positions are centred on an integral bounding-box centre.
        assert(entry->center[0] == 105.0f && entry->center[2] == 105.0f);
        assert(entry->vertices[0].position[0] == -5.0f && entry->vertices[2].position[2] == 5.0f);
        assert(entry->vertices[1].color[1] == 255 && entry->indices[5] == 3);
        cache.Replayed(*entry);
        for (std::size_t slot = 0; slot < 4; ++slot) assert(cache.IsVirtual(slot) && cache.VirtualSource(slot) == &quad[slot]);
        assert(cache.Counters().replays == 1 && cache.Counters().replayedTriangles == 2);
        // Another load signature (texture scale/geometry mode) never replays.
        assert(cache.Find(start, 8) == nullptr && cache.Counters().fallbacks == 1);
        cache.Clear();
        assert(gLive == 0 && cache.Bytes() == 0);
    }
    // A run that reads slots loaded earlier requires that exact provenance.
    {
        MAKE_CACHE(cache, 1 << 20);
        cache.BeginFrame(1);
        Load(cache, quad, 4, 0, 7); // Loaded by an earlier run.
        cache.BeginCapture(start, 7);
        cache.CaptureTriangle(0, 1, 2);
        assert(cache.EndCapture(end));
        assert(cache.Find(start, 7) != nullptr);
        Vtx other[4];
        std::memcpy(other, quad, sizeof(quad));
        Load(cache, other, 4, 0, 7); // Same slots now hold different sources.
        assert(cache.Find(start, 7) == nullptr);
        Load(cache, quad, 4, 0, 7);
        assert(cache.Find(start, 7) != nullptr);
        // Replayed slots have exact snapshots and can feed subsequent captures.
        Cmd loader[2] = {{0x32000000, 2}, {0xB8000000, 0}};
        cache.BeginCapture(&loader[0], 7);
        Load(cache, quad, 4, 0, 7);
        cache.CaptureTriangle(0, 1, 2);
        assert(cache.EndCapture(&loader[1]));
        cache.Replayed(*cache.Find(&loader[0], 7));
        assert(cache.IsVirtual(0) && cache.IsVirtual(3));
        Cmd second[2] = {{0xBF000000, 0x00020406}, {0xB8000000, 0}};
        cache.BeginCapture(&second[0], 7);
        cache.CaptureTriangle(0, 1, 2);
        assert(cache.EndCapture(&second[1]));
        const auto* secondEntry = cache.Find(&second[0], 7);
        assert(secondEntry && secondEntry->inputCount == 3);
        assert(cache.Snapshot(0).position[0] == quad[0].ob[0]);
    }
    // Changing commands or vertices in place invalidates permanently.
    {
        MAKE_CACHE(cache, 1 << 20);
        Vtx mutableQuad[4];
        std::memcpy(mutableQuad, quad, sizeof(quad));
        cache.BeginCapture(start, 7);
        Load(cache, mutableQuad, 4, 0, 7);
        cache.CaptureTriangle(0, 1, 2);
        assert(cache.EndCapture(end));
        mutableQuad[1].ob[1] = 50;
        assert(cache.Find(start, 7) == nullptr && cache.Known(start));
        mutableQuad[1].ob[1] = 0;
        assert(cache.Find(start, 7) == nullptr && gLive == 0);
        Cmd changed[4];
        std::memcpy(changed, commands, sizeof(commands));
        cache.BeginCapture(&changed[0], 7);
        Load(cache, quad, 4, 0, 7);
        cache.CaptureTriangle(0, 1, 2);
        assert(cache.EndCapture(&changed[3]));
        changed[2].w1 ^= 0x10;
        assert(cache.Find(&changed[0], 7) == nullptr);
    }
    // Inputs changed in place must not reuse stale geometry.
    {
        MAKE_CACHE(cache, 1 << 20);
        Vtx changed[4];
        std::memcpy(changed, quad, sizeof(quad));
        Load(cache, changed, 4, 0, 7);
        cache.BeginCapture(start, 7);
        cache.CaptureTriangle(0, 1, 2);
        assert(cache.EndCapture(end));
        changed[0].ob[0] += 20;
        Load(cache, changed, 4, 0, 7);
        // A stable address is not enough: this slot now contains new geometry.
        assert(cache.Find(start, 7) == nullptr);
    }
    // A signature change inside the run (e.g. lighting turned on) rejects it.
    {
        MAKE_CACHE(cache, 1 << 20);
        cache.BeginCapture(start, 7);
        Load(cache, quad, 2, 0, 7);
        Load(cache, quad + 2, 2, 2, 9);
        assert(!cache.Capturing() && cache.Known(start) && cache.Find(start, 7) == nullptr);
    }
    // Inherited vertices keep their own camera, including old virtual slots
    // after more than 128 unrelated replays (the former ring's capacity).
    {
        MAKE_CACHE(cache, 1 << 20);
        mk64_3ds::StaticRunTransform camera;
        camera.matrix[0][0] = camera.matrix[1][1] = camera.matrix[2][2] = camera.matrix[3][3] = 1;
        camera.aspect = 0.75f;
        cache.BeginCapture(start, 7, camera);
        cache.NoteLoadRange(quad, 3, 0);
        for (int i = 0; i < 3; ++i) cache.NoteLoad(i, quad + i, 7, Snapshot(quad[i]), camera);
        cache.CaptureTriangle(0, 1, 2);
        assert(cache.EndCapture(end));
        cache.Replayed(*cache.Find(start, 7, camera), camera);
        Cmd inherited[2] = {};
        cache.BeginCapture(inherited, 7, camera);
        cache.CaptureTriangle(0, 1, 2);
        assert(cache.EndCapture(inherited + 1));
        auto nextCamera = camera;
        nextCamera.matrix[3][0] = 100;
        assert(cache.Find(inherited, 7, nextCamera) == nullptr);
        assert(cache.Find(inherited, 7, camera) != nullptr);
        Cmd other[2] = {};
        cache.BeginCapture(other, 7, nextCamera);
        cache.NoteLoadRange(quad, 3, 4);
        for (int i = 0; i < 3; ++i) cache.NoteLoad(4 + i, quad + i, 7, Snapshot(quad[i]), nextCamera);
        cache.CaptureTriangle(4, 5, 6);
        assert(cache.EndCapture(other + 1));
        const auto* replay = cache.Find(other, 7, nextCamera);
        for (int i = 0; i < 300; ++i) {
            nextCamera.matrix[3][0] += 1;
            cache.Replayed(*replay, nextCamera);
        }
        assert(cache.Transform(0) == camera);
        assert(cache.Transform(4) == nextCamera);
        assert(cache.Find(inherited, 7, camera) != nullptr);
        // A mixed-matrix triangle cannot be submitted with one transform.
        Cmd mixed[2] = {};
        cache.BeginCapture(mixed, 7, camera);
        cache.CaptureTriangle(0, 1, 4);
        assert(!cache.Capturing() && !cache.EndCapture(mixed + 1));
    }
    // Forgotten (dynamic) slots cannot be captured.
    {
        MAKE_CACHE(cache, 1 << 20);
        Load(cache, quad, 4, 0, 7);
        cache.ForgetSlot(2);
        cache.BeginCapture(start, 7);
        cache.CaptureTriangle(0, 1, 2);
        assert(!cache.Capturing() && cache.Find(start, 7) == nullptr);
    }
    // Budget eviction removes the least recently used entry, never the current frame's.
    {
        const std::size_t entryBytes = 3 * sizeof(mk64_3ds::StaticRunVertex) + 8 + sizeof(mk64_3ds::StaticRunLoad);
        const std::size_t bigBytes = 3 * sizeof(mk64_3ds::StaticRunVertex) + 16 + sizeof(mk64_3ds::StaticRunLoad);
        MAKE_CACHE(cache, bigBytes * 4);
        Cmd runs[4][2] = {};
        for (int frame = 0; frame < 4; ++frame) {
            cache.BeginFrame(frame + 1);
            cache.BeginCapture(&runs[frame][0], 7);
            Load(cache, quad, 3, 0, 7);
            cache.CaptureTriangle(0, 1, 2);
            assert(cache.EndCapture(&runs[frame][1]));
        }
        assert(cache.Bytes() == entryBytes * 4);
        Cmd big[2] = {};
        cache.BeginFrame(5);
        cache.BeginCapture(&big[0], 7);
        Load(cache, quad, 3, 0, 7);
        cache.CaptureTriangle(0, 1, 2);
        cache.CaptureTriangle(0, 1, 2);
        assert(cache.EndCapture(&big[1]));
        assert(cache.Counters().evictions == 1 && cache.Find(&runs[0][0], 7) == nullptr);
        assert(cache.Find(&runs[3][0], 7) != nullptr);
        cache.SetEnabled(false);
        assert(cache.EntryCount() == 0 && gLive == 0 && cache.Find(&runs[3][0], 7) == nullptr);
    }
    // Empty runs and runs without an allocator are never stored.
    {
        Cache cache(1 << 20, 64);
        cache.BeginCapture(start, 7);
        assert(!cache.Capturing() && cache.Find(start, 7) == nullptr);
        MAKE_CACHE(empty, 1 << 20);
        empty.BeginCapture(start, 7);
        assert(!empty.EndCapture(end) && empty.Known(start));
    }
    assert(gLive == 0);
    std::puts("static runs: ok");
    return 0;
}
