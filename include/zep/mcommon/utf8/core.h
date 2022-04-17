#pragma once

#include <iterator>

namespace utf8 {
// The typedefs for 8-bit and 32-bit unsigned integers
// You may need to change them to match your system.
// These typedefs have the same names as ones from cstdint, or boost/cstdint
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;

// Helper code - not intended to be directly called by the library users. May be changed at any time
namespace internal {

template<typename octet_type>
inline uint8_t mask8(octet_type oc) { return static_cast<uint8_t>(0xff & oc); }

template<typename octet_type>
inline bool is_trail(octet_type oc) { return ((utf8::internal::mask8(oc) >> 6) == 0x2); }

template<typename octet_iterator>
inline typename std::iterator_traits<octet_iterator>::difference_type
sequence_length(octet_iterator lead_it) {
    uint8_t lead = utf8::internal::mask8(*lead_it);
    if (lead < 0x80) return 1;
    if ((lead >> 5) == 0x6) return 2;
    if ((lead >> 4) == 0xe) return 3;
    if ((lead >> 3) == 0x1e) return 4;
    return 0;
}

} // namespace internal

} // namespace utf8
