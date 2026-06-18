#pragma once

#include <ComputeCxx/AGBase.h>

AG_ASSUME_NONNULL_BEGIN

AG_EXTERN_C_BEGIN

typedef struct AG_SWIFT_NAME(_AGClosureStorage) AGClosureStorage {
    const void *thunk;
    const void *_Nullable context;
} AGClosureStorage;

AG_EXPORT
AG_REFINED_FOR_SWIFT
AGClosureStorage AGRetainClosure(const void *thunk, const void *_Nullable context);

#if defined(__wasi__)
// WASI: non-refined C variant so Swift imports it with the C ABI (passes a plain
// @convention(c) trampoline as the thunk; @_silgen_name lowers the closure with the
// Swift CC -> call_indirect signature_mismatch). Same retain semantics.
AG_EXPORT
AGClosureStorage AGRetainClosureC(const void *thunk, const void *_Nullable context);
#endif

AG_EXPORT
AG_REFINED_FOR_SWIFT
void AGReleaseClosure(AGClosureStorage closure);

AG_EXTERN_C_END

AG_ASSUME_NONNULL_END
