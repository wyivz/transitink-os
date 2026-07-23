#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

#include "generated/TransitCatalogAssets.h"

namespace transitink {

extern const char kEmbeddedTtcCatalogRevision[];
extern const char kEmbeddedTtcCatalogGeneratedAt[];
extern const EmbeddedCatalogAsset kEmbeddedTtcCatalogAssets[];
extern const std::size_t kEmbeddedTtcCatalogAssetCount;

}  // namespace transitink
