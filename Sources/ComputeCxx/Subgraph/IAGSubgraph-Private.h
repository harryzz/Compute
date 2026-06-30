#pragma once

#include "ComputeCxx/IAGBase.h"

#if TARGET_OS_MAC
#include "CoreFoundationPrivate/CFRuntime.h"
#else
#include <SwiftCorelibsCoreFoundation/CFRuntime.h>
#endif

#include "ComputeCxx/IAGSubgraph.h"

// [DBG #14] minimal live-storage counter — proves faithful frees actually free (created vs finalized).
// Set to 0 / delete to remove; pure diagnostic, no behavior change. Counter lives in IAGSubgraph.cpp.
#ifndef IAG_DBG_STORAGE_COUNT
#define IAG_DBG_STORAGE_COUNT 1
#endif

IAG_ASSUME_NONNULL_BEGIN

namespace IAG {
class Subgraph;
}

struct IAGSubgraphStorage {
    CFRuntimeBase base;
    IAG::Subgraph *_Nullable subgraph;
};

IAG_ASSUME_NONNULL_END
