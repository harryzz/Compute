#pragma once

#include "ComputeCxx/IAGBase.h"

#include "Page.h"
#include "Pointer.h"
#include "Table.h"

IAG_ASSUME_NONNULL_BEGIN

namespace IAG {
namespace data {

class zone {
  public:
    class info {
      private:
        enum {
            zone_id_mask = 0x7fffffff,
            deleted = 0x80000000,
        };
        uint32_t _value;

        struct raw_tag {};
        // [#12] Raw-preserving constructor. The `info(uint32_t zone_id)` ctor below MASKS off the deleted
        // bit (it takes a clean zone_id), so routing `with_deleted()` / `from_raw_value()` through it silently
        // stripped the deleted bit — making mark_deleted() a no-op and the deleted-subgraph expiry check dead.
        // (Latent on 64-bit Apple where pages aren't recycled; a use-after-free crash on wasm32.)
        info(uint32_t value, raw_tag) : _value(value) {};

      public:
        info(uint32_t zone_id) : _value(zone_id & zone_id_mask) {};

        uint32_t zone_id() const { return _value & zone_id_mask; };
        bool is_deleted() const { return (_value & deleted) != 0; };
        info with_deleted() const { return info(_value | deleted, raw_tag{}); };

        uint32_t to_raw_value() const { return _value; };
        static info from_raw_value(uint32_t value) { return info(value, raw_tag{}); };
    };

  private:
    typedef struct _bytes_info {
        ptr<struct _bytes_info> next;
        uint32_t size;
    } bytes_info;

    vector<std::unique_ptr<void, table::malloc_zone_deleter>, 0, uint32_t> _malloc_buffers;
    ptr<page> _first_page;
    ptr<bytes_info> _free_bytes;
    info _info;

    ptr<void> alloc_slow(uint32_t size, uint32_t alignment_mask);

  public:
    zone();
    ~zone();

    uint32_t zone_id() const { return _info.zone_id(); };
    void mark_deleted() { _info = _info.with_deleted(); };
    uint32_t page_seed() const { return _info.to_raw_value(); };

    page_ptr_list pages() const { return page_ptr_list(_first_page); };

    void clear();
    void realloc_bytes(ptr<void> *buffer, uint32_t size, uint32_t new_size, uint32_t alignment_mask);

    // Paged memory
    ptr<void> alloc_bytes(uint32_t size, uint32_t alignment_mask);
    ptr<void> alloc_bytes_recycle(uint32_t size, uint32_t alignment_mask);

    // Persistent memory
    void *alloc_persistent(size_t size);

    // Printing
    static void print_header();
    void print();
};

} // namespace data
} // namespace IAG

IAG_ASSUME_NONNULL_END
