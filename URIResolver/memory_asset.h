#pragma once

#include <pxr/pxr.h>

#include <pxr/usd/ar/asset.h>

#include <mutex>

PXR_NAMESPACE_OPEN_SCOPE

class MemoryAsset final : public ArAsset {
public:
    MemoryAsset(const char* raw, size_t size);
    ~MemoryAsset() override = default;

    MemoryAsset(const MemoryAsset&) = delete;
    MemoryAsset(MemoryAsset&&) = delete;
    MemoryAsset& operator=(const MemoryAsset&) = delete;
    MemoryAsset& operator=(MemoryAsset&&) = delete;

#if AR_VERSION == 2
    size_t GetSize() const override { return _GetSize(); }
    std::shared_ptr<const char> GetBuffer() const override {
        return _GetBuffer();
    };
    size_t Read(void* buffer, size_t count, size_t offset) const override {
        return _Read(buffer, count, offset);
    };
    std::pair<FILE*, size_t> GetFileUnsafe() const override {
        return _GetFileUnsafe();
    };
#else
    size_t GetSize() override { return _GetSize(); }
    std::shared_ptr<const char> GetBuffer() override {
        return _GetBuffer();
    };
    size_t Read(void* buffer, size_t count, size_t offset) override {
        return _Read(buffer, count, offset);
    };
    std::pair<FILE*, size_t> GetFileUnsafe() override {
        return _GetFileUnsafe();
    };
#endif

private:
    size_t _GetSize() const;
    std::shared_ptr<const char> _GetBuffer() const;
    size_t _Read(void* buffer, size_t count, size_t offset) const;
    std::pair<FILE*, size_t> _GetFileUnsafe() const;

    // We are using a shared ptr to store the data instead of a std::vector,
    // because the ArAsset interface requires return of the same struct, which
    // need to outlive the asset.
    std::shared_ptr<char> data;
    size_t data_size;
    mutable std::mutex temp_mutex;
    // This is going to be from tmpfile(), so no need to manually free it.
    mutable FILE* temp = nullptr;
};

PXR_NAMESPACE_CLOSE_SCOPE
