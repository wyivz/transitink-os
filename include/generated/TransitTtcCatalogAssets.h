#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

namespace transitink {

struct EmbeddedCatalogAsset {
    const char* path;
    const uint8_t* data;
    std::size_t size;
    const char* sha256;
};

extern const char kEmbeddedTtcCatalogRevision[];
extern const char kEmbeddedTtcCatalogGeneratedAt[];
extern const EmbeddedCatalogAsset kEmbeddedTtcCatalogAssets[];
extern const std::size_t kEmbeddedTtcCatalogAssetCount;

}  // namespace transitink
