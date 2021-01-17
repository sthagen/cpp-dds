#if __linux__ || __FreeBSD__

#include "./paths.hpp"

#include <dds/util/env.hpp>
#include <dds/util/log.hpp>

#include <cstdlib>

using namespace dds;

fs::path dds::user_home_dir() {
    static auto ret = []() -> fs::path {
        return fs::absolute(dds::getenv("HOME", [] {
            dds_log(error, "No HOME environment variable set!");
            return "/";
        }));
    }();
    return ret;
}

fs::path dds::user_data_dir() {
    static auto ret = []() -> fs::path {
        return fs::absolute(
            dds::getenv("XDG_DATA_HOME", [] { return user_home_dir() / ".local/share"; }));
    }();
    return ret;
}

fs::path dds::user_cache_dir() {
    static auto ret = []() -> fs::path {
        return fs::absolute(
            dds::getenv("XDG_CACHE_HOME", [] { return user_home_dir() / ".cache"; }));
    }();
    return ret;
}

fs::path dds::user_config_dir() {
    static auto ret = []() -> fs::path {
        return fs::absolute(
            dds::getenv("XDG_CONFIG_HOME", [] { return user_home_dir() / ".config"; }));
    }();
    return ret;
}

#endif
