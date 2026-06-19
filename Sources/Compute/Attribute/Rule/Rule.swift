import ComputeCxx

public protocol Rule: _AttributeBody {

    associatedtype Value

    static var initialValue: Value? { get }
    var value: Value { get }

}

extension Rule {

    public static var initialValue: Value? {
        return nil
    }

    public static func _updateDefault(_ self: UnsafeMutableRawPointer) {
        guard let initialValue = initialValue else {
            return
        }
        withUnsafePointer(to: initialValue) { initialValuePointer in
            Graph.setOutputValue(initialValuePointer)
        }
    }

    public static func _update(_ self: UnsafeMutableRawPointer, attribute: AnyAttribute) {
        let rule = self.assumingMemoryBound(to: Self.self)
        let value = rule.pointee.value
        withUnsafePointer(to: value) { valuePointer in
            Graph.setOutputValue(valuePointer)
        }
    }

}

extension Rule {

    public var bodyChanged: Bool {
        fatalError("not implemented")
    }

    public var attribute: Attribute<Value> {
        return Attribute<Value>(identifier: AnyAttribute.current!)
    }

    public var context: RuleContext<Value> {
        return RuleContext<Value>(attribute: attribute)
    }

}

@_silgen_name("AGGraphReadCachedAttribute")
func AGGraphReadCachedAttribute(
    hash: Int,
    type: Metadata,
    body: UnsafeRawPointer,
    valueType: Metadata,
    options: CachedValueOptions,
    owner: AnyAttribute,
    changed: UnsafeMutablePointer<Bool>?,
    attributeTypeID: (AGUnownedGraphContextRef) -> UInt32
) -> UnsafeRawPointer

#if arch(wasm32)
// WASI: AGGraphReadCachedAttribute takes a swiftcall closure-with-arg/return that
// mislowers on wasm (the @_silgen_name above lowers to signature_mismatch and traps
// on the VStack/layout cachedValue path). Route through the plain-C AGGraphReadCachedAttributeC
// (declared in the ComputeCxx header). The closure is invoked SYNCHRONOUSLY inside
// cache_fetch, but the engine's ClosureFunctionCI retains the context via swift_retain,
// so we must pass a real heap object (a box) — not a stack pointer like internAttributeType
// does. The box is kept alive for the duration of the call with withExtendedLifetime;
// the engine's retain/release on it is balanced.
private final class _CachedAttrBox {
    let fn: (AGUnownedGraphContextRef) -> UInt32
    init(_ f: @escaping (AGUnownedGraphContextRef) -> UInt32) { self.fn = f }
}
private let _cachedAttrTrampoline:
    @convention(c) (AGUnownedGraphContextRef, UnsafeRawPointer?) -> UInt32 = { graph, ctx in
        Unmanaged<_CachedAttrBox>.fromOpaque(ctx!).takeUnretainedValue().fn(graph)
    }
#endif

extension Rule where Self: Hashable {

    public func cachedValue(options: CachedValueOptions, owner: AnyAttribute?) -> Value {
        return withUnsafePointer(to: self) { bodyPointer in
            Self._cachedValue(
                options: options,
                owner: owner,
                hashValue: hashValue,
                bodyPtr: bodyPointer,
                update: { Self._update }
            ).pointee
        }
    }

    public func cachedValueIfExists(options: CachedValueOptions, owner: AnyAttribute?) -> Value? {
        return withUnsafePointer(to: self) { bodyPointer in
            let value = __AGGraphReadCachedAttributeIfExists(
                hashValue,
                Metadata(Self.self),
                bodyPointer,
                Metadata(Value.self),
                options,
                owner ?? .nil,
                nil
            )
            guard let value else {
                return nil
            }
            return value.assumingMemoryBound(to: Value.self).pointee
        }
    }

    public static func _cachedValue(
        options: CachedValueOptions,
        owner: AnyAttribute?,
        hashValue: Int,
        bodyPtr: UnsafeRawPointer,
        update: () -> (UnsafeMutableRawPointer, AnyAttribute) -> Void
    ) -> UnsafePointer<Value> {
        func makeTypeID(_ graph: AGUnownedGraphContextRef) -> UInt32 {
            return graph.internAttributeType(type: Metadata(Self.self)) {
                let attributeType = _AttributeType(
                    selfType: Self.self,
                    valueType: Value.self,
                    flags: [],  // TODO: check flags are empty
                    update: update()
                )
                let pointer = UnsafeMutablePointer<_AttributeType>.allocate(capacity: 1)
                pointer.initialize(to: attributeType)
                return UnsafePointer(pointer)
            }
        }
        #if arch(wasm32)
        // The closure is invoked synchronously inside AGGraphReadCachedAttributeC (it returns
        // before withoutActuallyEscaping ends), so `update`/makeTypeID never truly escape.
        let value: UnsafeRawPointer = withoutActuallyEscaping(makeTypeID) { escapingMakeTypeID in
            let box = _CachedAttrBox(escapingMakeTypeID)
            return withExtendedLifetime(box) {
                UnsafeRawPointer(
                    AGGraphReadCachedAttributeC(
                        hashValue,
                        Metadata(Self.self),
                        bodyPtr,
                        Metadata(Value.self),
                        options,
                        owner ?? .nil,
                        nil,
                        _cachedAttrTrampoline,
                        Unmanaged.passUnretained(box).toOpaque()
                    )
                )
            }
        }
        #else
        let value = AGGraphReadCachedAttribute(
            hash: hashValue,
            type: Metadata(Self.self),
            body: bodyPtr,
            valueType: Metadata(Value.self),
            options: options,
            owner: owner ?? .nil,
            changed: nil,
            attributeTypeID: makeTypeID
        )
        #endif
        return value.assumingMemoryBound(to: Value.self)
    }

}
