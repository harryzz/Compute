import ComputeCxx

@_silgen_name("AGTypeApplyFields")
func AGTypeApplyFields(_ type: Metadata, body: (UnsafePointer<CChar>, Int, Metadata) -> Void)

public func forEachField(of type: Any.Type, do body: (UnsafePointer<Int8>, Int, Any.Type) -> Void) {
    #if arch(wasm32)
    // WASI: route through the C-imported AGTypeApplyFieldsC so the field-visitor
    // callback uses the C ABI (the swiftcall closure mislowers on wasm ->
    // signature_mismatch). The callback fires synchronously during the call, so
    // pass the closure by pointer + a non-capturing @convention(c) trampoline.
    withoutActuallyEscaping(body) { escaping in
        withUnsafePointer(to: escaping) { ctxPtr in
            AGTypeApplyFieldsC(
                Metadata(type),
                { fieldName, fieldOffset, fieldType, ctx in
                    ctx!.assumingMemoryBound(to: ((UnsafePointer<Int8>, Int, Any.Type) -> Void).self)
                        .pointee(fieldName, fieldOffset, fieldType.type)
                },
                UnsafeRawPointer(ctxPtr)
            )
        }
    }
    #else
    AGTypeApplyFields(Metadata(type)) { fieldName, fieldOffset, fieldType in
        body(fieldName, fieldOffset, fieldType.type)
    }
    #endif
}

@_silgen_name("AGTypeApplyFields2")
func AGTypeApplyFields2(
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
        return withoutActuallyEscaping(body) { escaping in
            withUnsafePointer(to: escaping) { ctxPtr in
                AGTypeApplyFields2C(
                    self,
                    options,
                    { fieldName, fieldOffset, fieldType, ctx in
                        ctx!.assumingMemoryBound(to: ((UnsafePointer<CChar>, Int, Any.Type) -> Bool).self)
                            .pointee(fieldName, fieldOffset, fieldType.type)
                    },
                    UnsafeRawPointer(ctxPtr)
                )
            }
        }
        #else
        return AGTypeApplyFields2(self, options: options) { fieldName, fieldOffset, fieldType in
            return body(fieldName, fieldOffset, fieldType.type)
        }
        #endif
    }

}

extension Metadata: @retroactive CustomStringConvertible {

    public var description: String {
        #if os(iOS) || os(macOS)
        return __AGTypeDescription(self) as String
        #else
        return String(__AGTypeCopyDescription(self))
        #endif
    }

}

extension Metadata: @retroactive Equatable {}

extension Metadata: @retroactive Hashable {}
