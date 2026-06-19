import ComputeCxx

#if !arch(wasm32)
@_silgen_name("AGGraphSetOutputValue")
@inline(__always)
@inlinable
func AGGraphSetOutputValue(_ value: UnsafeRawPointer, of type: Metadata)
#endif

extension Graph {

    @inline(__always)
    public static func setOutputValue<Value>(_ value: UnsafePointer<Value>) {
        #if arch(wasm32)
        // WASI: call the C-imported AGGraphSetOutputValueC (header-declared) so Swift
        // uses the C ABI; @_silgen_name mislowers the call on wasm.
        AGGraphSetOutputValueC(UnsafeRawPointer(value), Metadata(Value.self))
        #else
        AGGraphSetOutputValue(UnsafeRawPointer(value), of: Metadata(Value.self))
        #endif
    }

    @_transparent
    @inline(__always)
    public var mainUpdates: Int { numericCast(counter(for: .mainThreadUpdates)) }

}

extension Graph {

    @_transparent
    public static func anyInputsChanged(excluding excludedAttributes: [AnyAttribute]) -> Bool {
        return __AGGraphAnyInputsChanged(excludedAttributes, excludedAttributes.count)
    }

}

@_silgen_name("AGGraphSetUpdateCallback")
func AGGraphSetUpdateCallback(
    _ graph: UnsafeRawPointer,
    callback: (() -> Void)?
)

@_silgen_name("AGGraphSetInvalidationCallback")
func AGGraphSetInvalidationCallback(
    _ graph: UnsafeRawPointer,
    callback: ((AnyAttribute) -> Void)?
)

@_silgen_name("AGGraphWithMainThreadHandler")
func AGGraphWithMainThreadHandler(
    _ graph: UnsafeRawPointer,
    body: () -> Void,
    mainThreadHandler: (() -> Void) -> Void
)

#if arch(wasm32)
// WASI: AGGraphSetUpdate/InvalidationCallback take swiftcall closures that mislower on
// wasm (-> signature_mismatch). These are STORED callbacks (invoked later by C++), so
// box the Swift closure (heap object) and pass a non-capturing @convention(c) trampoline
// + the box pointer to the plain-C *C setters. The box is passRetained so it lives for
// the graph's lifetime (the host registers these once; replacing them would leak the old
// box, which is acceptable for a process-lifetime graph host).
private final class _GraphUpdateBox {
    let fn: () -> Void
    init(_ f: @escaping () -> Void) { self.fn = f }
}
private let _graphUpdateTrampoline: @convention(c) (UnsafeRawPointer) -> Void = { ctx in
    Unmanaged<_GraphUpdateBox>.fromOpaque(ctx).takeUnretainedValue().fn()
}
private final class _GraphInvalidationBox {
    let fn: (AnyAttribute) -> Void
    init(_ f: @escaping (AnyAttribute) -> Void) { self.fn = f }
}
private let _graphInvalidationTrampoline: @convention(c) (AnyAttribute, UnsafeRawPointer) -> Void = { attr, ctx in
    Unmanaged<_GraphInvalidationBox>.fromOpaque(ctx).takeUnretainedValue().fn(attr)
}
#endif

extension Graph {

    public func onUpdate(_ handler: @escaping () -> Void) {
        #if arch(wasm32)
        AGGraphSetUpdateCallbackC(
            self,
            _graphUpdateTrampoline,
            Unmanaged.passRetained(_GraphUpdateBox(handler)).toOpaque()
        )
        #else
        AGGraphSetUpdateCallback(unsafeBitCast(self, to: UnsafeRawPointer.self), callback: handler)
        #endif
    }

    public func onInvalidation(_ handler: @escaping (AnyAttribute) -> Void) {
        #if arch(wasm32)
        AGGraphSetInvalidationCallbackC(
            self,
            _graphInvalidationTrampoline,
            Unmanaged.passRetained(_GraphInvalidationBox(handler)).toOpaque()
        )
        #else
        AGGraphSetInvalidationCallback(unsafeBitCast(self, to: UnsafeRawPointer.self), callback: handler)
        #endif
    }

    public func withDeadline<T>(_ deadline: UInt64, _ body: () -> T) -> T {
        let oldDeadline = self.deadline
        self.deadline = deadline
        let result = body()
        self.deadline = oldDeadline
        return result
    }

    // check is static
    public static func withoutUpdate<T>(_ body: () -> T) -> T {
        let previousUpdate = Graph.clearUpdate()
        let result = body()
        Graph.setUpdate(previousUpdate)
        return result
    }

    public func withoutSubgraphInvalidation<T>(_ body: () -> T) -> T {
        let wasDeferring = self.beginDeferringSubgraphInvalidation()
        let result = body()
        self.endDeferringSubgraphInvalidation(wasDeferring: wasDeferring)
        return result
    }

    public func withMainThreadHandler(_ mainThreadHandler: (() -> Void) -> Void, do body: () -> Void) {
        #if arch(wasm32)
        // WASI single-threaded: AGGraphWithMainThreadHandler's swiftcall closures mislower
        // on wasm. There is only one thread, so run body directly (the main handler exists
        // only to hop work onto the main thread).
        body()
        #else
        AGGraphWithMainThreadHandler(
            unsafeBitCast(self, to: UnsafeRawPointer.self),
            body: body,
            mainThreadHandler: mainThreadHandler
        )
        #endif
    }

}

extension Graph {

    public static func startProfiling(_ graph: Graph?) {
        fatalError("not implemented")
    }

    public static func stopProfiling(_ graph: Graph?) {
        fatalError("not implemented")
    }

    public static func markProfile(name: UnsafePointer<Int8>) {
        fatalError("not implemented")

    }

    public static func resetProfile() {
        fatalError("not implemented")
    }

}

extension Graph {

    public func addTraceEvent<T>(_ event: UnsafePointer<Int8>, value: T) {
        fatalError("not implemented")
    }

    public func addTraceEvent<T>(_ event: UnsafePointer<Int8>, context: UnsafePointer<T>) {
        fatalError("not implemented")
    }

}

extension Graph {

    public func print(includeValues: Bool) {
        fatalError("not implemented")
    }

    public func archiveJSON(name: String?) {
        fatalError("not implemented")
    }

    public func graphvizDescription(includeValues: Bool) -> String {
        fatalError("not implemented")
    }

    public static func printStack(maxFrames: Int) {
        fatalError("not implemented")
    }

    public static func stackDescription(maxFrames: Int) -> String {
        fatalError("not implemented")
    }

}

extension Graph: @retroactive Equatable {

    public static func == (_ lhs: Graph, _ rhs: Graph) -> Bool {
        return lhs.counter(for: .contextID) == rhs.counter(for: .contextID)
    }

}
