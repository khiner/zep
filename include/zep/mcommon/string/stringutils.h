#pragma once

#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace Zep {

std::string string_replace(std::string subject, const std::string &search, const std::string &replace);
void string_replace_in_place(std::string &subject, const std::string &search, const std::string &replace);

// trim from beginning of string (left)
inline std::string &LTrim(std::string &s, const char *t = " \t\n\r\f\v") {
    s.erase(0, s.find_first_not_of(t));
    return s;
}

// trim from end of string (right)
inline std::string &RTrim(std::string &s, const char *t = " \t\n\r\f\v") {
    s.erase(s.find_last_not_of(t) + 1);
    return s;
}

// trim from both ends of string (left & right)
inline std::string &Trim(std::string &s, const char *t = " \t\n\r\f\v") {
    return LTrim(RTrim(s, t), t);
}

std::string string_tolower(const std::string &str);

struct StringId {
    uint32_t id = 0;
    StringId() = default;
    explicit StringId(const char *pszString);
    explicit StringId(const std::string &str);
    explicit StringId(uint32_t _id) { id = _id; }

    bool operator==(const StringId &rhs) const {
        return id == rhs.id;
    }
    const StringId &operator=(const char *pszString);
    const StringId &operator=(const std::string &str);
    bool operator<(const StringId &rhs) const {
        return id < rhs.id;
    }

    operator uint32_t() const { return id; }

    std::string ToString() const {
        auto itr = GetStringLookup().find(id);
        return itr == GetStringLookup().end() ? "murmur:" + std::to_string(id) : itr->second;
    }

    static std::unordered_map<uint32_t, std::string> &GetStringLookup();
};

inline std::ostream &operator<<(std::ostream &str, StringId id) {
    str << id.ToString();
    return str;
}

void string_split(const std::string &text, const char *delims, std::vector<std::string> &tokens);
std::vector<std::string> string_split(const std::string &text, const char *delims);

inline void string_eat_char(std::string::const_iterator &itr, std::string::const_iterator &itrEnd) { if (itr != itrEnd) itr++; }
std::string string_slurp_if(std::string::const_iterator &itr, std::string::const_iterator itrEnd, char first, char last);
inline bool utf8_is_trailing(uint8_t ch) { return (ch >> 6) == 0x2; }
inline long utf8_codepoint_length(uint8_t ch) { return ((0xE5000000 >> ((ch >> 3) & 0x1e)) & 3) + 1; }

} // namespace Zep

namespace std {
template<>
struct hash<Zep::StringId> {
    std::size_t operator()(const Zep::StringId &k) const {
        // Compute individual hash values for first,
        // second and third and combine them using XOR
        // and bit shifting:
        return std::hash<uint32_t>()(k.id);
    }
};
} // namespace std
