#pragma once
#include <newton/analytics/grouped_hash_join.hpp>

namespace newton::analytics {

// Preserve exact duplicate multiplicity and borrowed right-row identity.
// The grouped posting lists avoid a separate unordered_multimap allocation
// per right row. Right rows with equal keys are stored contiguously in one
// small vector per distinct key; visitor ordering is unspecified.
template<class Left, class Right, class LeftKey, class RightKey, class Visitor>
void hash_inner_join(const Left& left, const Right& right,
                     LeftKey left_key, RightKey right_key, Visitor visitor) {
    hash_inner_join_grouped(left, right, left_key, right_key, visitor);
}
} // namespace newton::analytics
