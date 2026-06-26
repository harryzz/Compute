// IAGWasiClosureShim.cpp — the C side of Compute's wasm closure-ABI adaptation layer.
// See IAGWasiClosureShim.h for the why. Each entry is the plain-C counterpart of a Compute function
// that takes a swiftcall callback. Self-contained entries (the field visitors) reimplement the visit
// directly with a plain-C body — deliberately NOT via the swiftcall IAG::ClosureFunction, which is
// exactly what mislowers on wasm. Entries that need engine internals delegate to a small `…_c` method
// exposed on the core type instead (added in this layer, kept minimal).

#include "ComputeCxx/IAGWasiClosureShim.h"

#if defined(__wasi__)

#include "ComputeCxx/IAGType.h"
#include "ContextDescriptor.h"
#include "Metadata.h"
#include "MetadataVisitor.h"

// MARK: - Type field enumeration

void IAGTypeApplyFieldsC(IAGTypeID typeID,
                         void (*apply)(const char *field_name, size_t field_offset,
                                       IAGTypeID field_type, const void *context),
                         const void *apply_context) {
    class VisitorC : public IAG::swift::metadata_visitor {
        void (*_body)(const char *, size_t, IAGTypeID, const void *);
        const void *_context;

      public:
        VisitorC(void (*body)(const char *, size_t, IAGTypeID, const void *), const void *context)
            : _body(body), _context(context) {}

        bool unknown_result() override { return true; }
        bool visit_field(const IAG::swift::metadata &type, const IAG::swift::field_record &field,
                         size_t field_offset, size_t field_size) override {
            auto field_type = type.mangled_type_name_ref(field.MangledTypeName.get(), true, nullptr);
            if (field_type) {
                _body(field.FieldName.get(), field_offset, IAGTypeID(field_type), _context);
            }
            return true;
        }
    };

    VisitorC visitor(apply, apply_context);
    reinterpret_cast<const IAG::swift::metadata *>(typeID)->visit(visitor);
}

bool IAGTypeApplyFields2C(IAGTypeID typeID, IAGTypeApplyOptions options,
                          bool (*apply)(const char *field_name, size_t field_offset,
                                        IAGTypeID field_type, const void *context),
                          const void *apply_context) {
    class VisitorC : public IAG::swift::metadata_visitor {
        IAGTypeApplyOptions _options;
        bool (*_body)(const char *, size_t, IAGTypeID, const void *);
        const void *_context;

      public:
        VisitorC(IAGTypeApplyOptions options, bool (*body)(const char *, size_t, IAGTypeID, const void *),
                 const void *context)
            : _options(options), _body(body), _context(context) {}

        bool unknown_result() override {
            return _options & IAGTypeApplyOptionsContinueAfterUnknownField;
        }
        bool visit_field(const IAG::swift::metadata &type, const IAG::swift::field_record &field,
                         size_t field_offset, size_t field_size) override {
            auto field_type = type.mangled_type_name_ref(field.MangledTypeName.get(), true, nullptr);
            if (!field_type) {
                return unknown_result();
            }
            return _body(field.FieldName.get(), field_offset, IAGTypeID(field_type), _context);
        }
        bool visit_case(const IAG::swift::metadata &type, const IAG::swift::field_record &field,
                        uint32_t index) override {
            auto field_type = type.mangled_type_name_ref(field.MangledTypeName.get(), true, nullptr);
            if (!field_type) {
                return unknown_result();
            }
            return _body(field.FieldName.get(), index, IAGTypeID(field_type), _context);
        }
    };

    VisitorC visitor(options, apply, apply_context);
    auto type = reinterpret_cast<const IAG::swift::metadata *>(typeID);
    switch (type->getKind()) {
    case ::swift::MetadataKind::Class:
        if (options & IAGTypeApplyOptionsEnumerateClassFields) {
            return type->visit_heap(visitor, IAG::LayoutDescriptor::HeapMode::Class);
        }
        return false;
    case ::swift::MetadataKind::Struct:
        if (!(options & IAGTypeApplyOptionsEnumerateClassFields) &&
            !(options & IAGTypeApplyOptionsEnumerateEnumCases)) {
            return type->visit(visitor);
        }
        return false;
    case ::swift::MetadataKind::Enum:
    case ::swift::MetadataKind::Optional:
        if (options & IAGTypeApplyOptionsEnumerateEnumCases) {
            return type->visit(visitor);
        }
        return false;
    case ::swift::MetadataKind::Tuple:
        if (!(options & IAGTypeApplyOptionsEnumerateClassFields) &&
            !(options & IAGTypeApplyOptionsEnumerateEnumCases)) {
            return type->visit(visitor);
        }
        return false;
    default:
        return false;
    }
}

#endif // __wasi__
