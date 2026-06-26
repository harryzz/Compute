import ComputeCxx

#if arch(wasm32)
// [wasm port] IAGGraphInternAttributeType takes a swiftcall closure (makeAttributeType) that
// mislowers on wasm (@_silgen_name -> call_indirect signature_mismatch). Route through the
// header-declared plain-C IAGGraphInternAttributeTypeC with a non-capturing @convention(c) thunk;
// the closure is invoked synchronously, so pass it by stack pointer.
public func internAttributeType(
    ctx: UnownedGraphContext,
    body: Metadata,
    makeAttributeType: () -> UnsafePointer<_AttributeType>
) -> UInt32 {
    return withoutActuallyEscaping(makeAttributeType) { escaping in
        withUnsafePointer(to: escaping) { ctxPtr in
            IAGGraphInternAttributeTypeC(
                ctx,
                body,
                { c in
                    UnsafeRawPointer(
                        c!.assumingMemoryBound(to: (() -> UnsafePointer<_AttributeType>).self).pointee()
                    )
                },
                UnsafeRawPointer(ctxPtr)
            )
        }
    }
}
#else
@_silgen_name("IAGGraphInternAttributeType")
public func internAttributeType(
    ctx: UnownedGraphContext,
    body: Metadata,
    makeAttributeType: () -> UnsafePointer<_AttributeType>
) -> UInt32
#endif

extension Graph {
    static func typeIndex(
        ctx: UnownedGraphContext,
        body: any _AttributeBody.Type,
        valueType: Metadata,
        flags: _AttributeType.Flags,
        update: () -> (UnsafeMutableRawPointer, AnyAttribute) -> Void
    ) -> UInt32 {
        let makeAttributeType: () -> UnsafePointer<_AttributeType> = {
            let bodyType: _AttributeBody.Type
            #if CompatibilityModeAttributeGraphV6
            bodyType = Body.self
            #else
            bodyType = flags.contains(.external) ? _External.self : body
            #endif
            let attributeType =
                _AttributeType(
                    selfType: bodyType,
                    valueType: valueType,
                    flags: flags,
                    update: update()
                )
            let pointer = UnsafeMutablePointer<_AttributeType>.allocate(capacity: 1)
            pointer.initialize(to: attributeType)
            return UnsafePointer(pointer)
        }
        return internAttributeType(
            ctx: ctx,
            body: Metadata(body),
            makeAttributeType: makeAttributeType
        )
    }
}

#if !arch(wasm32)
@_silgen_name("IAGGraphSetOutputValue")
@inline(__always)
@inlinable
func IAGGraphSetOutputValue(_ value: UnsafeRawPointer, of type: Metadata)
#endif

extension Graph {

    @inline(__always)
    public static func setOutputValue<Value>(_ value: UnsafePointer<Value>) {
        #if arch(wasm32)
        // [wasm port] @_silgen_name mislowers on wasm; use the header-declared plain-C variant.
        IAGGraphSetOutputValueC(UnsafeRawPointer(value), Metadata(Value.self))
        #else
        IAGGraphSetOutputValue(UnsafeRawPointer(value), of: Metadata(Value.self))
        #endif
    }

    @_transparent
    @inline(__always)
    public var mainUpdates: Int { numericCast(counter(for: .mainThreadUpdates)) }

}

extension Graph {

    @_transparent
    public static func anyInputsChanged(excluding excludedAttributes: [AnyAttribute]) -> Bool {
        return __IAGGraphAnyInputsChanged(excludedAttributes, excludedAttributes.count)
    }

}

@_silgen_name("IAGGraphSetUpdateCallback")
func IAGGraphSetUpdateCallback(
    _ graph: UnsafeRawPointer,
    callback: (() -> Void)?
)

@_silgen_name("IAGGraphSetInvalidationCallback")
func IAGGraphSetInvalidationCallback(
    _ graph: UnsafeRawPointer,
    callback: ((AnyAttribute) -> Void)?
)

@_silgen_name("IAGGraphWithMainThreadHandler")
func IAGGraphWithMainThreadHandler(
    _ graph: UnsafeRawPointer,
    body: () -> Void,
    mainThreadHandler: (() -> Void) -> Void
)

#if arch(wasm32)
// [wasm] Boxes for the escaping closures stored by the graph's update/invalidation callbacks.
// Retained for the graph's lifetime (set once per graph; intentionally not released — negligible).
private final class _WandrGraphUpdateBox { let f: () -> Void; init(_ f: @escaping () -> Void) { self.f = f } }
private final class _WandrGraphInvalidationBox {
    let f: (AnyAttribute) -> Void
    init(_ f: @escaping (AnyAttribute) -> Void) { self.f = f }
}
#endif

extension Graph {

    public func onUpdate(_ handler: @escaping () -> Void) {
        #if arch(wasm32)
        // [wasm] The @_silgen_name shadow mislowers the swiftcall-closure ABI -> call_indirect
        // signature_mismatch. Route through the IAG_REFINED_FOR_SWIFT import (__-prefixed), which
        // carries IAG_SWIFT_CC(swift)/IAG_SWIFT_CONTEXT so the compiler emits the correct swiftcc
        // thunk + context. The handler is boxed (the C side stores it for later updates).
        let ctx = Unmanaged.passRetained(_WandrGraphUpdateBox(handler)).toOpaque()
        __IAGGraphSetUpdateCallback(
            self,
            { c in Unmanaged<_WandrGraphUpdateBox>.fromOpaque(c).takeUnretainedValue().f() },
            UnsafeRawPointer(ctx))
        #else
        IAGGraphSetUpdateCallback(unsafeBitCast(self, to: UnsafeRawPointer.self), callback: handler)
        #endif
    }

    public func onInvalidation(_ handler: @escaping (AnyAttribute) -> Void) {
        #if arch(wasm32)
        let ctx = Unmanaged.passRetained(_WandrGraphInvalidationBox(handler)).toOpaque()
        __IAGGraphSetInvalidationCallback(
            self,
            { attr, c in Unmanaged<_WandrGraphInvalidationBox>.fromOpaque(c).takeUnretainedValue().f(attr) },
            UnsafeRawPointer(ctx))
        #else
        IAGGraphSetInvalidationCallback(unsafeBitCast(self, to: UnsafeRawPointer.self), callback: handler)
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
        IAGGraphWithMainThreadHandler(
            unsafeBitCast(self, to: UnsafeRawPointer.self),
            body: body,
            mainThreadHandler: mainThreadHandler
        )
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
