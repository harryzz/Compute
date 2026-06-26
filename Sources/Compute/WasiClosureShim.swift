// WasiClosureShim — the Swift side of Compute's wasm closure-ABI adaptation layer.
//
// Pairs with IAGWasiClosureShim.{h,cpp}. The public Compute API delegates here under
// `#if arch(wasm32)`, so the non-wasm path stays byte-identical to upstream.
//
// Discipline (kept uniform across this layer):
//   • SYNCHRONOUS closures (invoked during the call, never stored) → pass a pointer to the escaping
//     closure and reconstruct it in a NON-capturing @convention(c) thunk. No heap box, no ARC.
//   • PERSISTENT closures (the engine stores the callback) → a heap box whose retain ownership
//     matches that C API's contract (added per-function as those land).
#if arch(wasm32)
import ComputeCxx

enum WasiClosureShim {

    // MARK: - Type field enumeration (synchronous)

    static func forEachField(
        of type: Metadata,
        _ body: (UnsafePointer<CChar>, Int, Any.Type) -> Void
    ) {
        withoutActuallyEscaping(body) { escaping in
            withUnsafePointer(to: escaping) { ctxPtr in
                IAGTypeApplyFieldsC(
                    type,
                    { name, offset, ty, ctx in
                        ctx.assumingMemoryBound(to: ((UnsafePointer<CChar>, Int, Any.Type) -> Void).self)
                            .pointee(name, offset, ty.type)
                    },
                    UnsafeRawPointer(ctxPtr))
            }
        }
    }

    static func forEachField2(
        of type: Metadata,
        options: Metadata.ApplyOptions,
        _ body: (UnsafePointer<CChar>, Int, Any.Type) -> Bool
    ) -> Bool {
        withoutActuallyEscaping(body) { escaping in
            withUnsafePointer(to: escaping) { ctxPtr in
                IAGTypeApplyFields2C(
                    type, options,
                    { name, offset, ty, ctx in
                        ctx.assumingMemoryBound(to: ((UnsafePointer<CChar>, Int, Any.Type) -> Bool).self)
                            .pointee(name, offset, ty.type)
                    },
                    UnsafeRawPointer(ctxPtr))
            }
        }
    }
}
#endif
