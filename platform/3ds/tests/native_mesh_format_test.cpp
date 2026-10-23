#include "native_mesh_format_3ds.hpp"

#include <cassert>
#include <vector>

using namespace mk64_3ds::native_scene;

static void Set32(std::vector<std::uint8_t>& bytes, size_t offset, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[offset+i] = value >> (8*i);
}

int main() {
    // A minimal asset-free triangle, including unaligned file input support.
    std::vector<std::uint8_t> bytes(16 + 3*16 + 3*2, 0);
    std::memcpy(bytes.data(), "NCM1", 4);
    Set32(bytes, 4, 3); Set32(bytes, 8, 3); Set32(bytes, 12, 16);
    bytes[66] = 1; bytes[68] = 2;
    MeshView view;
    assert(ValidateMesh(bytes.data(), bytes.size(), view));
    assert(view.vertexCount == 3 && view.indexCount == 3);
    for (size_t size = 0; size < bytes.size(); ++size) {
        assert(!ValidateMesh(bytes.data(), size, view));
        assert(!view.vertices && !view.indices && !view.vertexCount);
    }
    auto bad = bytes; bad[68] = 3;
    assert(!ValidateMesh(bad.data(), bad.size(), view));
    bad = bytes; Set32(bad, 8, 0xffffffff);
    assert(!ValidateMesh(bad.data(), bad.size(), view));
    bad = bytes; Set32(bad, 4, 0xffffffff);
    assert(!ValidateMesh(bad.data(), bad.size(), view));
    bad = bytes; Set32(bad, 12, 40);
    assert(!ValidateMesh(bad.data(), bad.size(), view));
    bad = bytes; bad[3] = '2';
    assert(!ValidateMesh(bad.data(), bad.size(), view));
    bad = bytes; bad.push_back(0);
    assert(!ValidateMesh(bad.data(), bad.size(), view));
    bad = bytes; bad.insert(bad.begin(), 0);
    assert(ValidateMesh(bad.data()+1, bytes.size(), view));
    assert(!ValidateMesh(nullptr, bytes.size(), view));
}
