#pragma once

#include "resource_runtime_3ds.h"

namespace mk64_3ds {
// Main-thread scope owner for archive texture bytes. Loading another resource
// or allocating a destination may reclaim unowned cache data; keep this lease
// alive until all source/palette pixels have been copied. It owns no GPU image.
class TextureLease3DS final {
  public:
    TextureLease3DS() = default;
    explicit TextureLease3DS(const char* name) { Acquire(name); }
    ~TextureLease3DS() { Reset(); }
    TextureLease3DS(const TextureLease3DS&) = delete;
    TextureLease3DS& operator=(const TextureLease3DS&) = delete;

    bool Acquire(const char* name) {
        Reset();
        return Mk64Resource3DSAcquireTexture(name,&resource_) == MK64_RESOURCE_LOAD_OK_3DS;
    }
    bool Valid() const { return resource_.data != nullptr && resource_.lifetimeToken != 0; }
    const Mk64TextureResource3DS& Get() const { return resource_; }
    void Reset() {
        if(resource_.lifetimeToken) Mk64Resource3DSReleaseTexture(resource_.lifetimeToken);
        resource_ = {};
    }

  private:
    Mk64TextureResource3DS resource_{};
};
} // namespace mk64_3ds
