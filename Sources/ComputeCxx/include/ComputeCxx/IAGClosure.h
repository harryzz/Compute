#pragma once

#include <ComputeCxx/IAGBase.h>

IAG_ASSUME_NONNULL_BEGIN

IAG_EXTERN_C_BEGIN

typedef struct IAG_SWIFT_NAME(_IAGClosureStorage) IAGClosureStorage {
    const void *thunk;
    const void *_Nullable context;
} IAGClosureStorage;

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
IAGClosureStorage IAGRetainClosure(const void *thunk, const void *_Nullable context);

#if defined(__wasi__)
// WASI: non-refined C variant so Swift imports it with the C ABI (passes a plain
// @convention(c) trampoline as the thunk; @_silgen_name lowers the closure with the
// Swift CC -> call_indirect signature_mismatch). Same retain semantics.
IAG_EXPORT
IAGClosureStorage IAGRetainClosureC(const void *thunk, const void *_Nullable context);
#endif

IAG_EXPORT
IAG_REFINED_FOR_SWIFT
void IAGReleaseClosure(IAGClosureStorage closure);

IAG_EXTERN_C_END

IAG_ASSUME_NONNULL_END
