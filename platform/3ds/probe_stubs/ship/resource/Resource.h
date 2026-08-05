#pragma once

#include <cstddef>
#include <memory>
#include <string>

namespace Ship {

struct ResourceInitData {
    std::string Path;
};

class IResource {
  public:
    inline static const std::string gAltAssetPrefix = "alt/";

    explicit IResource(std::shared_ptr<ResourceInitData> initData) : mInitData(std::move(initData)) {
    }
    virtual ~IResource() = default;

    virtual void* GetRawPointer() = 0;
    virtual size_t GetPointerSize() = 0;

    bool IsDirty() const {
        return mIsDirty;
    }
    void Dirty() {
        mIsDirty = true;
    }
    std::shared_ptr<ResourceInitData> GetInitData() {
        return mInitData;
    }

  private:
    std::shared_ptr<ResourceInitData> mInitData;
    bool mIsDirty = false;
};

template <class T> class Resource : public IResource {
  public:
    using IResource::IResource;
    virtual T* GetPointer() = 0;
    void* GetRawPointer() override {
        return static_cast<void*>(GetPointer());
    }
};

} // namespace Ship
