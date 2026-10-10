#ifndef ITEM_PICKUP_RESULT_H
#define ITEM_PICKUP_RESULT_H

// The public AddPlayerItem Boolean still means "attach this weapon". Containers
// need to distinguish consumed duplicates from items retaining physical rounds.
enum class ItemPickupResult { Rejected, Attached, Consumed, Retained };

inline bool ItemPickupTransferred(ItemPickupResult result)
{
    return result != ItemPickupResult::Rejected;
}

#endif
