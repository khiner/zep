#pragma once

#include "core.h"

namespace utf8::unchecked {
template<typename octet_iterator>
octet_iterator append(uint32_t cp, octet_iterator result) {
    if (cp < 0x80)                        // one octet
        *(result++) = static_cast<uint8_t>(cp);
    else if (cp < 0x800) {                // two octets
        *(result++) = static_cast<uint8_t>((cp >> 6) | 0xc0);
        *(result++) = static_cast<uint8_t>((cp & 0x3f) | 0x80);
    } else if (cp < 0x10000) {              // three octets
        *(result++) = static_cast<uint8_t>((cp >> 12) | 0xe0);
        *(result++) = static_cast<uint8_t>(((cp >> 6) & 0x3f) | 0x80);
        *(result++) = static_cast<uint8_t>((cp & 0x3f) | 0x80);
    } else {                                // four octets
        *(result++) = static_cast<uint8_t>((cp >> 18) | 0xf0);
        *(result++) = static_cast<uint8_t>(((cp >> 12) & 0x3f) | 0x80);
        *(result++) = static_cast<uint8_t>(((cp >> 6) & 0x3f) | 0x80);
        *(result++) = static_cast<uint8_t>((cp & 0x3f) | 0x80);
    }
    return result;
}

template<typename octet_iterator>
uint32_t next(octet_iterator &it) {
    uint32_t cp = utf8::internal::mask8(*it);
    typename std::iterator_traits<octet_iterator>::difference_type length = utf8::internal::sequence_length(it);
    switch (length) {
        case 1:break;
        case 2:it++;
            cp = ((cp << 6) & 0x7ff) + ((*it) & 0x3f);
            break;
        case 3:++it;
            cp = ((cp << 12) & 0xffff) + ((utf8::internal::mask8(*it) << 6) & 0xfff);
            ++it;
            cp += (*it) & 0x3f;
            break;
        case 4:++it;
            cp = ((cp << 18) & 0x1fffff) + ((utf8::internal::mask8(*it) << 12) & 0x3ffff);
            ++it;
            cp += (utf8::internal::mask8(*it) << 6) & 0xfff;
            ++it;
            cp += (*it) & 0x3f;
            break;
    }
    ++it;
    return cp;
}

template<typename octet_iterator>
uint32_t prior(octet_iterator &it) {
    while (utf8::internal::is_trail(*(--it)));
    octet_iterator temp = it;
    return utf8::unchecked::next(temp);
}

template<typename octet_iterator, typename distance_type>
void advance(octet_iterator &it, distance_type n) {
    const distance_type zero(0);
    if (n < zero) { // backward
        for (distance_type i = n; i < zero; ++i)
            utf8::unchecked::prior(it);
    } else { // forward
        for (distance_type i = zero; i < n; ++i)
            utf8::unchecked::next(it);
    }
}

template<typename octet_iterator>
typename std::iterator_traits<octet_iterator>::difference_type
distance(octet_iterator first, octet_iterator last) {
    typename std::iterator_traits<octet_iterator>::difference_type dist;
    for (dist = 0; first < last; ++dist)
        utf8::unchecked::next(first);
    return dist;
}

// The iterator class
template<typename octet_iterator>
class iterator : public std::iterator<std::bidirectional_iterator_tag, uint32_t> {
    octet_iterator it;
public:
    iterator() = default;
    explicit iterator(const octet_iterator &octet_it) : it(octet_it) {}
    // the default "big three" are OK
    octet_iterator base() const { return it; }
    uint32_t operator*() const {
        octet_iterator temp = it;
        return utf8::unchecked::next(temp);
    }
    bool operator==(const iterator &rhs) const { return (it == rhs.it); }
    bool operator!=(const iterator &rhs) const { return !(operator==(rhs)); }

    iterator &operator++() {
        ::std::advance(it, utf8::internal::sequence_length(it));
        return *this;
    }
    const iterator operator++(int) {
        iterator temp = *this;
        ::std::advance(it, utf8::internal::sequence_length(it));
        return temp;
    }
    iterator &operator--() {
        utf8::unchecked::prior(it);
        return *this;
    }
    const iterator operator--(int) {
        iterator temp = *this;
        utf8::unchecked::prior(it);
        return temp;
    }
}; // class iterator

} // namespace utf8
