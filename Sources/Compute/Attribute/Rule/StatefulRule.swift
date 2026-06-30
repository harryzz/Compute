import ComputeCxx
#if os(WASI)
import WASILibc
@inline(never) func _srTrace(_ s: String) { fputs("[SRTRACE] \(s)\n", stderr); fflush(stderr) }
#else
@inline(never) func _srTrace(_ s: String) {}
#endif

public protocol StatefulRule: _AttributeBody {

    associatedtype Value

    static var initialValue: Value? { get }
    mutating func updateValue()

}

extension StatefulRule {

    public static var initialValue: Value? {
        return nil
    }

    public static func _updateDefault(_ default: UnsafeMutableRawPointer) {
        guard let initialValue = initialValue else {
            return
        }
        withUnsafePointer(to: initialValue) { initialValuePointer in
            Graph.setOutputValue(initialValuePointer)
        }
    }
}

extension StatefulRule {

    public static func _update(_ self: UnsafeMutableRawPointer, attribute: AnyAttribute) {
        let rule = self.assumingMemoryBound(to: Self.self)
        rule.pointee.updateValue()
    }

    public var bodyChanged: Bool {
        fatalError("not implemented")
    }

    public var value: Value {
        unsafeAddress {
            guard let result = __IAGGraphGetOutputValue(Metadata(Value.self)) else {
                preconditionFailure()
            }
            let pointer = result.assumingMemoryBound(to: Value.self)
            return UnsafePointer(pointer)
        }
        nonmutating set {
            if AnyAttribute.current == nil { _srTrace("SR.VALUE-SETTER nil-current Value=\(Value.self)") }
            context.value = newValue
        }
    }

    public var hasValue: Bool {
        return context.hasValue
    }

    public var attribute: Attribute<Value> {
        if AnyAttribute.current == nil { _srTrace("StatefulRule.attribute NIL-CURRENT Value=\(Value.self)") }
        return Attribute<Value>(identifier: AnyAttribute.current!)
    }

    public var context: RuleContext<Value> {
        return RuleContext<Value>(attribute: attribute)
    }

}
