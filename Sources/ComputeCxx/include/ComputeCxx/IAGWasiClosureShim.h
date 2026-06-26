#pragma once

// IAGWasiClosureShim — the C side of Compute's wasm closure-ABI adaptation layer.
//
// On wasm32, a Swift closure passed to a C function declared with a swiftcall callback
// (IAG_SWIFT_CC(swift) + IAG_SWIFT_CONTEXT) mislowers: the @convention(c) thunk Swift can actually
// emit doesn't match clang's swiftcall lowering, so the call_indirect traps (signature mismatch /
// uninitialized element). For every Compute function that takes such a callback, this layer declares
// a plain-C `…C` counterpart whose callback uses the ordinary C calling convention — which the Swift
// thunk matches exactly. All such declarations live here (one place), gated to wasi only.

#include <ComputeCxx/IAGBase.h>
#include <ComputeCxx/IAGType.h>

#if defined(__wasi__)

IAG_ASSUME_NONNULL_BEGIN
IAG_EXTERN_C_BEGIN

// MARK: - Type field enumeration (synchronous)

// Plain-C counterpart of IAGTypeApplyFields. The visit runs synchronously; the callback is invoked
// with the C convention so a Swift @convention(c) thunk + pointer-to-escaping-closure matches it.
IAG_EXPORT
void IAGTypeApplyFieldsC(IAGTypeID typeID,
                         void (*apply)(const char *field_name, size_t field_offset,
                                       IAGTypeID field_type, const void *context),
                         const void *apply_context);

// Plain-C counterpart of IAGTypeApplyFields2 (the returning / options variant).
IAG_EXPORT
bool IAGTypeApplyFields2C(IAGTypeID typeID, IAGTypeApplyOptions options,
                          bool (*apply)(const char *field_name, size_t field_offset,
                                        IAGTypeID field_type, const void *context),
                          const void *apply_context);

IAG_EXTERN_C_END
IAG_ASSUME_NONNULL_END

#endif // __wasi__
