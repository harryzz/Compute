#pragma once

#include <Utilities/Base.h>
#include <Utilities/SwiftBridging.h>

UTIL_ASSUME_NONNULL_BEGIN

namespace util {

class Heap;

uint64_t string_hash(char const *str);

class UntypedTable {
  public:
    using key_type = const void *_Nonnull;
    using nullable_key_type = const void *_Nullable;
    using value_type = const void *_Nullable;
    using size_type = uint64_t;
    using hasher = uint64_t (*)(void const *);
    using key_equal = bool (*)(void const *, void const *);
    using key_callback = void (*)(const key_type);
    using value_callback = void (*)(const value_type);
    using entry_callback = void (*)(const key_type, const value_type, void *context);

  private:
    struct HashNode {
        HashNode *next;
        key_type key;
        value_type value;
        uint64_t hash_value;
    };
    using Bucket = HashNode *_Nonnull;

    hasher _hash;
    key_equal _compare;
    key_callback _did_remove_key;
    value_callback _did_remove_value;
    Heap *_heap;
    HashNode *_spare_node;
    Bucket *_Nonnull _buckets;
    uint64_t _count;
    uint64_t _bucket_mask;
    uint32_t _bucket_mask_width;
    bool _is_heap_owner;
    bool _compare_by_pointer;

    // Managing buckets
    void create_buckets();
    void grow_buckets();

  public:
    static UntypedTable *create();
    static void destroy(UntypedTable *value);

    UntypedTable();
    UntypedTable(hasher _Nullable custom_hasher, key_equal _Nullable custom_compare,
                 key_callback _Nullable did_remove_key, value_callback _Nullable did_remove_value,
                 Heap *_Nullable heap);
    ~UntypedTable();

    // non-copyable
    UntypedTable(const UntypedTable &) = delete;
    UntypedTable &operator=(const UntypedTable &) = delete;

    // non-movable
    UntypedTable(UntypedTable &&) = delete;
    UntypedTable &operator=(UntypedTable &&) = delete;

    // Lookup
    bool empty() const noexcept { return _count == 0; };
    size_type count() const noexcept { return _count; };
    value_type lookup(key_type key, nullable_key_type *_Nullable found_key) const noexcept;
    void for_each(entry_callback body, void *context) const;

    // Modifiers
    bool insert(const key_type key, const value_type value);
    bool remove(const key_type key);
    bool remove_ptr(const key_type key);
} SWIFT_UNSAFE_REFERENCE;

#if !SWIFT_TESTING

template <typename Key, typename Value> class Table : public UntypedTable {
  public:
    using key_type = Key;
    using value_type = Value;
    using hasher = uint64_t (*)(const key_type);
    using key_equal = bool (*)(const key_type, const key_type);
    using key_callback = void (*)(const key_type);
    using value_callback = void (*)(const value_type);
    using entry_callback = void (*)(const key_type, const value_type, void *context);

    Table() : UntypedTable() {};
    Table(hasher _Nullable custom_hasher, key_equal _Nullable custom_compare, key_callback _Nullable did_remove_key,
          value_callback _Nullable did_remove_value, Heap *_Nullable heap)
        : UntypedTable(reinterpret_cast<UntypedTable::hasher>(custom_hasher),
                       reinterpret_cast<UntypedTable::key_equal>(custom_compare),
                       reinterpret_cast<UntypedTable::key_callback>(did_remove_key),
                       reinterpret_cast<UntypedTable::value_callback>(did_remove_value), heap) {};

    // Lookup

    value_type lookup(const key_type key, key_type *_Nullable found_key) const noexcept {
        // Size-safe (see insert): `*(void**)&key` would read sizeof(void*) bytes from a possibly-narrower
        // key_type (e.g. data::ptr = 4 bytes vs 64-bit void*) -> ASAN stack-buffer-overflow.
        void *k = nullptr;
        __builtin_memcpy(&k, &key, sizeof(key_type) < sizeof(void *) ? sizeof(key_type) : sizeof(void *));
        auto result = UntypedTable::lookup(k,
                                           reinterpret_cast<UntypedTable::nullable_key_type *_Nullable>(found_key));
        value_type out{};
        __builtin_memcpy(&out, &result, sizeof(value_type) < sizeof(result) ? sizeof(value_type) : sizeof(result));
        return out;
    };

    void for_each(entry_callback _Nonnull body, void *_Nullable context) const {
#if defined(__wasi__)
        // [wasm] Don't type-pun the callback. UntypedTable invokes its callback with the untyped
        // signature (const void*, const void*, void*) = (i32, i32, i32). Reinterpret-casting a typed
        // entry_callback to that and call_indirect-ing it traps "indirect call type mismatch" whenever
        // key_type/value_type aren't i32 — e.g. a uint64_t key (Table<uint64_t, Context*>) is i64, so
        // Graph::call_update's iteration faulted. Pass a trampoline matching the untyped signature and
        // reconstruct the typed args inside (keys/values round-trip through pointer-width, exactly as
        // insert/lookup already store them via *(void**)&). Same "no type-pun on wasm" rule as the
        // closure shims.
        struct Tramp {
            entry_callback body;
            void *ctx;
        } tramp{body, context};
        UntypedTable::for_each(
            [](const void *k, const void *v, void *c) {
                auto *t = static_cast<Tramp *>(c);
                key_type key = (key_type)(uintptr_t)k;
                value_type val;
                __builtin_memcpy(&val, &v, sizeof(value_type) < sizeof(v) ? sizeof(value_type) : sizeof(v));
                t->body(key, val, t->ctx);
            },
            &tramp);
#else
        UntypedTable::for_each((UntypedTable::entry_callback)body, context);
#endif
    };

    // Modifying entries

    bool insert(const key_type key, const value_type value) {
        // Don't type-pun via `*(void**)&` — that reads sizeof(void*) bytes regardless of the actual
        // type size, so a key/value_type narrower than void* (e.g. data::ptr = 4 bytes vs a 64-bit
        // void*) reads past the end of the local (ASAN: stack-buffer-overflow). Zero-init a void* and
        // copy only the type's bytes (same size-safe rule as for_each above).
        void *k = nullptr, *v = nullptr;
        __builtin_memcpy(&k, &key, sizeof(key_type) < sizeof(void *) ? sizeof(key_type) : sizeof(void *));
        __builtin_memcpy(&v, &value, sizeof(value_type) < sizeof(void *) ? sizeof(value_type) : sizeof(void *));
        return UntypedTable::insert(k, v);
    };
    bool remove(const key_type key) {
        void *k = nullptr;
        __builtin_memcpy(&k, &key, sizeof(key_type) < sizeof(void *) ? sizeof(key_type) : sizeof(void *));
        return UntypedTable::remove(k);
    };
    bool remove_ptr(const key_type key) { return UntypedTable::remove_ptr(key); };
};

#endif

} // namespace util

UTIL_ASSUME_NONNULL_END
