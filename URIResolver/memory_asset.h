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

    size_t GetSize() override;
    std::shared_ptr<const char> GetBuffer() override;
    size_t Read(void* buffer, size_t count, size_t offset) override;
    std::pair<FILE*, size_t> GetFileUnsafe() override;

private:
    // We are using a shared ptr to store the data instead of a std::vector,
    // because the ArAsset interface requires return of the same struct, which
    // need to outlive the asset.
    std::shared_ptr<char> data;
    size_t data_size;
    std::mutex temp_mutex;
    // This is going to be from tmpfile(), so no need to manually free it.
    FILE* temp = nullptr;
};

PXR_NAMESPACE_CLOSE_SCOPE
