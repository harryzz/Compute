#pragma once

#include <ComputeCxx/IAGBase.h>

IAG_ASSUME_NONNULL_BEGIN

IAG_EXTERN_C_BEGIN

typedef uint32_t IAGAttribute IAG_SWIFT_STRUCT IAG_SWIFT_NAME(AnyAttribute);

IAG_EXPORT
const IAGAttribute IAGAttributeNil;

#if defined(__wasi__)
// [wasm32] Subgraph imports as a foreign-reference class (IAGSubgraph.h), which does NOT actually
// synthesize a nested `Flags` member even though the rename target `IAGSubgraphRef.Flags` is known to
// the diagnostics engine — so the nested swift_name here would make BOTH `IAGAttributeFlags` (renamed
// away) and `Subgraph.Flags` (not a real member) unusable. Keep the enum's bare name on wasm; the
// `Subgraph.Flags` spelling is restored as a typealias in Compute/Graph/Subgraph.swift.
typedef IAG_OPTIONS(uint8_t, IAGAttributeFlags) {
    IAGAttributeFlagsNone = 0,
    IAGAttributeFlagsAll = 0xFF,
};
#else
typedef IAG_OPTIONS(uint8_t, IAGAttributeFlags) {
    IAGAttributeFlagsNone = 0,
    IAGAttributeFlagsAll = 0xFF,
} IAG_SWIFT_NAME(IAGSubgraphRef.Flags);
#endif

IAG_EXTERN_C_END

IAG_ASSUME_NONNULL_END
