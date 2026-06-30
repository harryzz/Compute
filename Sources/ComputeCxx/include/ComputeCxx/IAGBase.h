#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <ComputeCxx/IAGTargetConditionals.h>

#ifndef __has_include
#define __has_include(x) 0
#endif
#ifndef __has_feature
#define __has_feature(x) 0
#endif
#ifndef __has_attribute
#define __has_attribute(x) 0
#endif
#ifndef __has_extension
#define __has_extension(x) 0
#endif

#define _IAG_STRINGIFY(_x) #_x

#if !defined(IAG_EXTERN_C_BEGIN)
#if defined(__cplusplus)
#define IAG_EXTERN_C_BEGIN extern "C" {
#define IAG_EXTERN_C_END   }
#else
#define IAG_EXTERN_C_BEGIN
#define IAG_EXTERN_C_END
#endif
#endif

#if __GNUC__
#define IAG_EXPORT extern __attribute__((__visibility__("default")))
#else
#define IAG_EXPORT extern
#endif

#if __GNUC__
#define IAG_INLINE static __inline__
#else
#define IAG_INLINE static inline
#endif

#ifndef IAG_RETURNS_RETAINED
#if __has_feature(attribute_cf_returns_retained)
#define IAG_RETURNS_RETAINED __attribute__((cf_returns_retained))
#else
#define IAG_RETURNS_RETAINED
#endif
#endif

#ifndef IAG_IMPLICIT_BRIDGING_ENABLED
#if __has_feature(arc_cf_code_audited)
#define IAG_IMPLICIT_BRIDGING_ENABLED _Pragma("clang arc_cf_code_audited begin")
#else
#define IAG_IMPLICIT_BRIDGING_ENABLED
#endif
#endif

#ifndef IAG_IMPLICIT_BRIDGING_DISABLED
#if __has_feature(arc_cf_code_audited)
#define IAG_IMPLICIT_BRIDGING_DISABLED _Pragma("clang arc_cf_code_audited end")
#else
#define IAG_IMPLICIT_BRIDGING_DISABLED
#endif
#endif

#if __has_attribute(objc_bridge) && __has_feature(objc_bridge_id) && __has_feature(objc_bridge_id_on_typedefs)

#ifdef __OBJC__
@class NSArray;
@class NSAttributedString;
@class NSString;
@class NSNull;
@class NSCharacterSet;
@class NSData;
@class NSDate;
@class NSTimeZone;
@class NSDictionary;
@class NSError;
@class NSLocale;
@class NSNumber;
@class NSSet;
@class NSURL;
#endif

#define IAG_BRIDGED_TYPE(T)        __attribute__((objc_bridge(T)))
#define IAG_BRIDGED_MUTABLE_TYPE(T)    __attribute__((objc_bridge_mutable(T)))
#define IAG_RELATED_TYPE(T,C,I)        __attribute__((objc_bridge_related(T,C,I)))
#else
#define IAG_BRIDGED_TYPE(T)
#define IAG_BRIDGED_MUTABLE_TYPE(T)
#define IAG_RELATED_TYPE(T,C,I)
#endif

// Whether Swift ARC actually owns the CF storage (IAGSubgraphStorage / IAGGraphStorage). This is true
// ONLY on Darwin/Swift, where CoreFoundation types are ARC-bridged: there Swift retains/releases the
// handle and struct-held refs (e.g. ViewList.Subgraph items, parentSubgraph), so the CF refcount is a
// real owner and CFRelease at end-of-life is correct. Note: the `objc_bridge` attribute condition
// above is ALSO satisfied by non-Apple clang (the attribute parses), yet off Darwin the Swift runtime
// does NOT ARC-manage these foreign CF types — so the attribute is NOT a valid predictor and `__APPLE__`
// is. WASM: Subgraph is imported as a Swift FOREIGN-REFERENCE type (IAGSubgraph.h IAG_SWIFT_SHARED_REFERENCE)
// whose retain/release map to real CFRetain/CFRelease, so ARC DOES refcount the storage there — combined
// with a graph-alive self-ref (CFRetain at create, released at Subgraph::clear_object) the count tracks
// true liveness and storage is freed faithfully (bug #14; measured premature-free-free in Phase-1).
// LINUX: no foreign-ref import yet (the CF type imports unmanaged), so the CF refcount is NOT driven by
// ARC -> keep storage IMMORTAL there until the import is extended. One source of truth for the gate:
#if defined(__APPLE__) || defined(__wasi__)
#define IAG_CF_STORAGE_SWIFT_MANAGED 1
#else
#define IAG_CF_STORAGE_SWIFT_MANAGED 0
#endif

#if __has_feature(assume_nonnull)
#define IAG_ASSUME_NONNULL_BEGIN _Pragma("clang assume_nonnull begin")
#define IAG_ASSUME_NONNULL_END   _Pragma("clang assume_nonnull end")
#else
#define IAG_ASSUME_NONNULL_BEGIN
#define IAG_ASSUME_NONNULL_END
#endif

#if !__has_feature(nullability)
#ifndef _Nullable
#define _Nullable
#endif
#ifndef _Nonnull
#define _Nonnull
#endif
#ifndef _Null_unspecified
#define _Null_unspecified
#endif
#endif

#if __has_attribute(enum_extensibility)
#define __IAG_ENUM_ATTRIBUTES __attribute__((enum_extensibility(open)))
#define __IAG_CLOSED_ENUM_ATTRIBUTES __attribute__((enum_extensibility(closed)))
#define __IAG_OPTIONS_ATTRIBUTES __attribute__((flag_enum,enum_extensibility(open)))
#else
#define __IAG_ENUM_ATTRIBUTES
#define __IAG_CLOSED_ENUM_ATTRIBUTES
#define __IAG_OPTIONS_ATTRIBUTES
#endif

#define __IAG_ENUM_FIXED_IS_AVAILABLE (__cplusplus && __cplusplus >= 201103L && (__has_extension(cxx_strong_enums) || __has_feature(objc_fixed_enum))) || (!__cplusplus && (__has_feature(objc_fixed_enum) || __has_extension(cxx_fixed_enum)))

#if __IAG_ENUM_FIXED_IS_AVAILABLE
#define IAG_ENUM(_type, _name) \
  _Pragma("clang diagnostic push") \
  _Pragma("clang diagnostic ignored \"-Welaborated-enum-base\"") \
  enum __IAG_ENUM_ATTRIBUTES _name : _type _name; \
  enum _name : _type \
  _Pragma("clang diagnostic pop")
#define IAG_CLOSED_ENUM(_type, _name) \
  _Pragma("clang diagnostic push") \
  _Pragma("clang diagnostic ignored \"-Welaborated-enum-base\"") \
  enum __IAG_CLOSED_ENUM_ATTRIBUTES _name : _type _name; \
  enum _name : _type \
  _Pragma("clang diagnostic pop")
#if (__cplusplus)
#define IAG_OPTIONS(_type, _name) __attribute__((availability(swift,unavailable))) _type _name; enum __IAG_OPTIONS_ATTRIBUTES : _name
#else
#define IAG_OPTIONS(_type, _name) \
  _Pragma("clang diagnostic push") \
  _Pragma("clang diagnostic ignored \"-Welaborated-enum-base\"") \
  enum __IAG_OPTIONS_ATTRIBUTES _name : _type _name; \
  enum _name : _type \
  _Pragma("clang diagnostic pop")
#endif
#else
#define IAG_ENUM(_type, _name) _type _name; enum
#define IAG_CLOSED_ENUM(_type, _name) _type _name; enum
#define IAG_OPTIONS(_type, _name) _type _name; enum
#endif

#if __has_attribute(swift_private)
#define IAG_REFINED_FOR_SWIFT __attribute__((swift_private))
#else
#define IAG_REFINED_FOR_SWIFT
#endif

#if __has_attribute(swift_name)
#define IAG_SWIFT_NAME(_name) __attribute__((swift_name(#_name)))
#else
#define IAG_SWIFT_NAME(_name)
#endif

#if __has_attribute(swift_wrapper)
#define IAG_SWIFT_STRUCT __attribute__((swift_wrapper(struct)))
#else
#define IAG_SWIFT_STRUCT
#endif

// Define mappings for calling conventions.

// Annotation for specifying a calling convention of
// a runtime function. It should be used with declarations
// of runtime functions like this:
// void runtime_function_name() IAG_SWIFT_CC(swift)
#define IAG_SWIFT_CC(CC) IAG_SWIFT_CC_##CC

// IAG_SWIFT_CC(c) is the C calling convention.
#define IAG_SWIFT_CC_c

// IAG_SWIFT_CC(swift) is the Swift calling convention.
#if __has_attribute(swiftcall)
#define IAG_SWIFT_CC_swift __attribute__((swiftcall))
#define IAG_SWIFT_CONTEXT __attribute__((swift_context))
#define IAG_SWIFT_ERROR_RESULT __attribute__((swift_error_result))
#define IAG_SWIFT_INDIRECT_RESULT __attribute__((swift_indirect_result))
#else
#define IAG_SWIFT_CC_swift
#define IAG_SWIFT_CONTEXT
#define IAG_SWIFT_ERROR_RESULT
#define IAG_SWIFT_INDIRECT_RESULT
#endif

#if __has_attribute(swift_attr)
#define IAG_SWIFT_SHARED_REFERENCE(_retain, _release)                        \
  __attribute__((swift_attr("import_reference")))                           \
  __attribute__((swift_attr(_IAG_STRINGIFY(retain:_retain))))       \
  __attribute__((swift_attr(_IAG_STRINGIFY(release:_release))))
#define IAG_SWIFT_RETURNS_RETAINED __attribute__((swift_attr("returns_retained")))
#define IAG_SWIFT_RETURNS_UNRETAINED __attribute__((swift_attr("returns_unretained")))
#else
#define IAG_SWIFT_SHARED_REFERENCE(_retain, _release)
#define IAG_SWIFT_RETURNS_RETAINED
#define IAG_SWIFT_RETURNS_UNRETAINED
#endif

#if __has_include(<ptrcheck.h>)
#include <ptrcheck.h>
#define IAG_COUNTED_BY(N) __counted_by(N)
#else
#define IAG_COUNTED_BY(N)
#endif
