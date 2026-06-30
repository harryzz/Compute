#include "WeakAttributeID.h"

#include "AttributeID.h"
#include "Data/Table.h"
#include "Data/Zone.h"

namespace IAG {

bool WeakAttributeID::expired() const {
    uint64_t raw_page_seed = data::table::shared().raw_page_seed(_identifier.page_ptr());
    if (raw_page_seed & 0xff00000000) {
        auto zone_info = data::zone::info::from_raw_value(uint32_t(raw_page_seed));
        // [#12] A weak reference is expired if its source subgraph was DELETED — even if the page hasn't
        // been reclaimed yet (the deferred-teardown window). Previously only zone_id was compared (which
        // masks the deleted bit), so a weak ref to a mark_deleted-but-not-reclaimed subgraph read as live ->
        // stale read/update -> crash on wasm. (Was dead code: mark_deleted didn't even set the bit; see Zone.h.)
        if (zone_info.zone_id() == _seed && !zone_info.is_deleted()) {
            return false;
        }
    }
    return true;
}

const AttributeID WeakAttributeID::evaluate() const {
    return _identifier && !expired() ? _identifier : AttributeID(IAGAttributeNil);
};

} // namespace IAG
