#include "zep/stringutils.h"

namespace Zep {

std::unordered_map<uint32_t, std::string> &StringId::GetStringLookup() {
    static std::unordered_map<uint32_t, std::string> stringLookup;
    return stringLookup;
}

std::string string_tolower(const std::string &str) {
    std::string copy = str;
    std::transform(copy.begin(), copy.end(), copy.begin(), [](char ch) {
        return (char) ::tolower(int(ch));
    });
    return copy;
}

std::string string_replace(std::string subject, const std::string &search, const std::string &replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos) {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
    return subject;
}

void string_replace_in_place(std::string &subject, const std::string &search, const std::string &replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos) {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
}

// CM: I can't remember where this came from; please let me know if you do!
// I know it is open source, but not sure who wrote it.
uint32_t murmur_hash(const void *key, int len, uint32_t seed) {
    // 'm' and 'r' are mixing constants generated offline.
    // They're not really 'magic', they just happen to work well.
    const unsigned int m = 0x5bd1e995;
    const int r = 24;

    // Initialize the hash to a 'random' value
    unsigned int h = seed ^ len;

    // Mix 4 bytes at a time into the hash
    const auto *data = (const unsigned char *) key;

    while (len >= 4) {
#ifdef PLATFORM_BIG_ENDIAN
        unsigned int k = (data[0]) + (data[1] << 8) + (data[2] << 16) + (data[3] << 24);
#else
        unsigned int k = (data[3]) + (data[2] << 8) + (data[1] << 16) + (data[0] << 24);
#endif

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    // Handle the last few bytes of the input array
    switch (len) {
        case 3:h ^= data[2] << 16;
        case 2:h ^= data[1] << 8;
        case 1:h ^= data[0];
            h *= m;
    };

    // Do a few final mixes of the hash to ensure the last few
    // bytes are well-incorporated.

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

std::vector<std::string> string_split(const std::string &text, const char *delims) {
    std::vector<std::string> tok;
    string_split(text, delims, tok);
    return tok;
}

// String split with multiple delims
// https://stackoverflow.com/a/7408245/18942
void string_split(const std::string &text, const char *delims, std::vector<std::string> &tokens) {
    tokens.clear();
    std::size_t start = text.find_first_not_of(delims), end;

    while ((end = text.find_first_of(delims, start)) != std::string::npos) {
        tokens.push_back(text.substr(start, end - start));
        start = text.find_first_not_of(delims, end);
    }
    if (start != std::string::npos)
        tokens.push_back(text.substr(start));
}

std::string string_slurp_if(std::string::const_iterator &itr, std::string::const_iterator itrEnd, char first, char last) {
    if (itr == itrEnd) return "";

    auto itrCurrent = itr;
    if (*itrCurrent == first) {
        while ((itrCurrent != itrEnd) && *itrCurrent != last) itrCurrent++;

        if ((itrCurrent != itrEnd) && *itrCurrent == last) {
            itrCurrent++;
            auto ret = std::string(itr, itrCurrent);
            itr = itrCurrent;
            return ret;
        }
    }
    return "";
};

StringId::StringId(const char *pszString) {
    id = murmur_hash(pszString, (int) strlen(pszString), 0);
    StringId::GetStringLookup()[id] = pszString;
}

StringId::StringId(const std::string &str) {
    id = murmur_hash(str.c_str(), (int) str.length(), 0);
    StringId::GetStringLookup()[id] = str;
}

const StringId &StringId::operator=(const char *pszString) {
    id = murmur_hash(pszString, (int) strlen(pszString), 0);
    StringId::GetStringLookup()[id] = pszString;
    return *this;
}

const StringId &StringId::operator=(const std::string &str) {
    id = murmur_hash(str.c_str(), (int) str.length(), 0);
    StringId::GetStringLookup()[id] = str;
    return *this;
}

} // namespace Zep
