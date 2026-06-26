import ComputeCxx

#if arch(wasm32)
// [wasm] Boxes for field-apply closures: the @convention(c) thunk reads the closure from a class
// property (clean swiftcc call) instead of reconstructing it from a raw pointer (which mismatches
// the multi-arg signature under wasm call_indirect). Sync use -> released right after the apply.
private final class _WandrFieldApplyVoidBox {
    let f: (UnsafePointer<CChar>, Int, Metadata) -> Void
    init(_ f: @escaping (UnsafePointer<CChar>, Int, Metadata) -> Void) { self.f = f }
}
private final class _WandrFieldApplyBoolBox {
    let f: (UnsafePointer<CChar>, Int, Metadata) -> Bool
    init(_ f: @escaping (UnsafePointer<CChar>, Int, Metadata) -> Bool) { self.f = f }
}
#endif

@_silgen_name("IAGTypeApplyFields")
func IAGTypeApplyFields(_ type: Metadata, body: (UnsafePointer<CChar>, Int, Metadata) -> Void)

public func forEachField(of type: Any.Type, do body: (UnsafePointer<Int8>, Int, Any.Type) -> Void) {
    #if arch(wasm32)
    // [wasm] @_silgen_name mislowers the swiftcall closure; route through the IAG_REFINED_FOR_SWIFT
    // import with a non-capturing @convention(c) thunk + boxed context (sync apply).
    withoutActuallyEscaping(body) { body in
        let box = Unmanaged.passRetained(_WandrFieldApplyVoidBox { fieldName, fieldOffset, fieldType in
            body(fieldName, fieldOffset, fieldType.type)
        }).toOpaque()
        defer { Unmanaged<_WandrFieldApplyVoidBox>.fromOpaque(box).release() }
        __IAGTypeApplyFields(
            Metadata(type),
            { name, offset, ty, c in
                Unmanaged<_WandrFieldApplyVoidBox>.fromOpaque(c!).takeUnretainedValue().f(name, offset, ty)
            },
            UnsafeRawPointer(box))
    }
    #else
    IAGTypeApplyFields(Metadata(type)) { fieldName, fieldOffset, fieldType in
        body(fieldName, fieldOffset, fieldType.type)
    }
    #endif
}

@_silgen_name("IAGTypeApplyFields2")
func IAGTypeApplyFields2(
    _ type: Metadata,
    options: Metadata.ApplyOptions,
    body: (UnsafePointer<CChar>, Int, Metadata) -> Bool
) -> Bool

extension Metadata {

    public init(_ type: Any.Type) {
        self.init(rawValue: unsafeBitCast(type, to: UnsafePointer<_Metadata>.self))
    }

    public var type: Any.Type {
        return unsafeBitCast(rawValue, to: Any.Type.self)
    }

    public func forEachField(
        options: Metadata.ApplyOptions,
        do body: (UnsafePointer<CChar>, Int, Any.Type) -> Bool
    )
        -> Bool
    {
        #if arch(wasm32)
        return withoutActuallyEscaping(body) { body in
            let box = Unmanaged.passRetained(_WandrFieldApplyBoolBox { fieldName, fieldOffset, fieldType in
                body(fieldName, fieldOffset, fieldType.type)
            }).toOpaque()
            defer { Unmanaged<_WandrFieldApplyBoolBox>.fromOpaque(box).release() }
            return __IAGTypeApplyFields2(
                self, options,
                { name, offset, ty, c in
                    Unmanaged<_WandrFieldApplyBoolBox>.fromOpaque(c).takeUnretainedValue().f(name, offset, ty)
                },
                UnsafeRawPointer(box))
        }
        #else
        return IAGTypeApplyFields2(self, options: options) { fieldName, fieldOffset, fieldType in
            return body(fieldName, fieldOffset, fieldType.type)
        }
        #endif
    }

}

extension Metadata: @retroactive CustomStringConvertible {

    public var description: String {
        #if os(iOS) || os(macOS)
        return __IAGTypeDescription(self) as String
        #else
        return String(__IAGTypeCopyDescription(self))
        #endif
    }

}

extension Metadata: @retroactive Equatable {}

extension Metadata: @retroactive Hashable {}
