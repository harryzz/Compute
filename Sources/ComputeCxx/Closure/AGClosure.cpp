#include "ComputeCxx/AGClosure.h"

#include <swift/Runtime/HeapObject.h>

#include "ClosureFunction.h"

namespace AG {

AG_CLOSURE_CONTEXT_REFCOUNT_ATTR void *retain_swift_context(const void *context) noexcept {
    void *mutable_context = const_cast<void *>(context);
    return ::swift::swift_retain(reinterpret_cast<::swift::HeapObject *>(mutable_context));
}

AG_CLOSURE_CONTEXT_REFCOUNT_ATTR void release_swift_context(const void *context) noexcept {
    void *mutable_context = const_cast<void *>(context);
    ::swift::swift_release(reinterpret_cast<::swift::HeapObject *>(mutable_context));
}

} // namespace AG

AGClosureStorage AGRetainClosure(const void *thunk, const void *_Nullable context) {
    const void *retained_context = context ? AG::retain_swift_context(context) : nullptr;
    return AGClosureStorage((void *)thunk, retained_context);
}

void AGReleaseClosure(AGClosureStorage closure) {
    if (closure.context) {
        AG::release_swift_context(closure.context);
    }
}
