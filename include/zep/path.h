#pragma once

#include "zep/stringutils.h"

#include <system_error>
#include <chrono>
#include <sstream>
#include <fstream>

namespace Zep {

struct ZepPath {
    using const_iterator = std::vector<std::string>::const_iterator;

    ZepPath(std::string strPath = {}) : path(std::move(strPath)) {}
    ZepPath(const char *pszZepPath) : path(pszZepPath) {}

    bool empty() const { return path.empty(); }

    ZepPath stem() const {
        auto str = filename().string();
        auto dot_index = str.find_last_of('.');
        return dot_index != std::string::npos ? str.substr(0, dot_index) : str;
    }

    // TODO Unit tests
    // (See SO solution for some examples)
    ZepPath filename() const {
        //https://stackoverflow.com/a/43283887/18942
        if (path.empty()) return {};

        auto len = path.length();
        auto index = path.find_last_of("/\\");

        if (index == std::string::npos) return path;

        if (index + 1 >= len) {
            len--;
            index = path.substr(0, len).find_last_of("/\\");

            if (len == 0) return path;
            if (index == 0) return path.substr(1, len - 1);
            if (index == std::string::npos) return path.substr(0, len);
            return path.substr(index + 1, len - index - 1);
        }

        return path.substr(index + 1, len - index);
    }

    bool has_filename() const { return !filename().string().empty(); }
    bool has_extension() const { return !extension().string().empty(); }

    ZepPath extension() const {
        if (!has_filename()) return {};

        auto str = filename().string();
        auto dot_index = str.find_last_of('.');
        if (dot_index != std::string::npos) return str.substr(dot_index, str.length() - dot_index);

        return {};
    }

    ZepPath parent_path() const {
        std::string strSplit;
        size_t sep = path.find_last_of("\\/");
        if (sep != std::string::npos) return path.substr(0, sep);

        return {""};
    }

    const char *c_str() const { return path.c_str(); }
    std::string string() const { return path; }

    bool operator==(const ZepPath &rhs) const { return path == rhs.string(); }
    bool operator!=(const ZepPath &rhs) const { return path != rhs.string(); }

    ZepPath operator/(const ZepPath &rhs) const {
        std::string temp = path;
        RTrim(temp, "\\/");
        return {temp.empty() ? rhs.string() : temp + "/" + rhs.string()};
    }

    operator std::string() const { return path; }

    bool operator<(const ZepPath &rhs) const { return path < rhs.string(); }

    std::vector<std::string>::const_iterator begin() const {
        std::string can = string_replace(path, "\\", "/");
        m_components = string_split(can, "/");
        return m_components.begin();
    }

    std::vector<std::string>::const_iterator end() const { return m_components.end(); }

private:
    mutable std::vector<std::string> m_components;
    std::string path;
};

ZepPath path_get_relative(const ZepPath &from, const ZepPath &to);

} // namespace Zep
