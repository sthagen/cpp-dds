#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace dds {

inline namespace string_utils {

inline std::string_view sview(std::string_view::const_iterator beg,
                              std::string_view::const_iterator end) {
    if (beg == end) {
        return "";
    }
    return std::string_view(&*beg, static_cast<std::size_t>(std::distance(beg, end)));
}

inline std::string_view trim_view(std::string_view s) {
    auto iter = s.begin();
    auto end  = s.end();
    while (iter != end && std::isspace(*iter)) {
        ++iter;
    }
    auto riter = s.rbegin();
    auto rend  = std::make_reverse_iterator(iter);
    while (riter != rend && std::isspace(*riter)) {
        ++riter;
    }
    auto new_end = riter.base();
    return sview(iter, new_end);
}

inline bool ends_with(std::string_view s, std::string_view key) {
    auto found = s.rfind(key);
    return found != s.npos && found == s.size() - key.size();
}

inline bool starts_with(std::string_view s, std::string_view key) { return s.find(key) == 0; }

inline bool contains(std::string_view s, std::string_view key) { return s.find(key) != s.npos; }

inline std::vector<std::string_view> split_view(std::string_view str, std::string_view sep) {
    std::vector<std::string_view> ret;
    std::string_view::size_type   prev_pos = 0;
    auto                          pos      = prev_pos;
    while ((pos = str.find(sep, prev_pos)) != str.npos) {
        ret.emplace_back(str.substr(prev_pos, pos - prev_pos));
        prev_pos = pos + sep.length();
    }
    ret.emplace_back(str.substr(prev_pos));
    return ret;
}

inline std::vector<std::string> split(std::string_view str, std::string_view sep) {
    std::vector<std::string>    ret;
    std::string_view::size_type prev_pos = 0;
    auto                        pos      = prev_pos;
    while ((pos = str.find(sep, prev_pos)) != str.npos) {
        ret.emplace_back(str.substr(prev_pos, pos - prev_pos));
        prev_pos = pos + sep.length();
    }
    ret.emplace_back(str.substr(prev_pos));
    return ret;
}

inline std::string replace(std::string_view str, std::string_view key, std::string_view repl) {
    std::string                 ret;
    std::string_view::size_type pos      = 0;
    std::string_view::size_type prev_pos = 0;
    while (pos = str.find(key, pos), pos != key.npos) {
        ret.append(str.begin() + prev_pos, str.begin() + pos);
        ret.append(repl);
        prev_pos = pos += key.size();
    }
    ret.append(str.begin() + prev_pos, str.end());
    return ret;
}

inline std::vector<std::string>
replace(std::vector<std::string> strings, std::string_view key, std::string_view repl) {
    for (auto& item : strings) {
        item = replace(item, key, repl);
    }
    return strings;
}

template <typename Range>
inline std::string joinstr(std::string_view joiner, Range&& rng) {
    auto        iter = std::begin(rng);
    auto        end  = std::end(rng);
    std::string ret;
    while (iter != end) {
        ret.append(*iter);
        ++iter;
        if (iter != end) {
            ret.append(joiner);
        }
    }
    return ret;
}

}  // namespace string_utils

}  // namespace dds