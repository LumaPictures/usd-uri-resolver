#include "memory_asset.h"

#include <pxr/base/tf/diagnostic.h>

#include <cstring>

PXR_NAMESPACE_OPEN_SCOPE

MemoryAsset::MemoryAsset(const char* raw, size_t size) : data_size(size) {
    if (size > 0) {
        auto* d = static_cast<char*>(malloc(data_size));
        memcpy(d, raw, data_size);
        data.reset(d, [](char* p) { free(p); });
    }
}

size_t MemoryAsset::_GetSize() const { return data_size; }

std::shared_ptr<const char> MemoryAsset::_GetBuffer() const { return data; }

size_t MemoryAsset::_Read(void* buffer, size_t count, size_t offset) const {
    if (data == nullptr || offset >= data_size) { return 0; }
    const auto copied = std::min(data_size - offset, count);
    memcpy(buffer, data.get() + offset, copied);
    return copied;
}

std::pair<FILE*, size_t> MemoryAsset::_GetFileUnsafe() const {
    if (temp == nullptr) {
        std::lock_guard<std::mutex> lock(temp_mutex);
        if (temp == nullptr) {
            temp = tmpfile();
            if (!TF_VERIFY(temp != nullptr)) {
                return {nullptr, 0};
            }
            fwrite(data.get(), data_size, 1, temp);
        }
    }
    return {temp, 0};
}

PXR_NAMESPACE_CLOSE_SCOPE
