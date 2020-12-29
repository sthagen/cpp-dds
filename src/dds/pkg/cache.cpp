#include "./cache.hpp"

#include <dds/error/errors.hpp>
#include <dds/pkg/db.hpp>
#include <dds/sdist/dist.hpp>
#include <dds/solve/solve.hpp>
#include <dds/util/log.hpp>
#include <dds/util/paths.hpp>
#include <dds/util/string.hpp>

#include <neo/ref.hpp>
#include <range/v3/action/sort.hpp>
#include <range/v3/action/unique.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/transform.hpp>

using namespace dds;

using namespace ranges;

void pkg_cache::_log_blocking(path_ref dirpath) noexcept {
    dds_log(warn, "Another process has the package cache directory locked [{}]", dirpath.string());
    dds_log(warn, "Waiting for cache to be released...");
}

void pkg_cache::_init_cache_dir(path_ref dirpath) noexcept { fs::create_directories(dirpath); }

fs::path pkg_cache::default_local_path() noexcept { return dds_data_dir() / "pkg"; }

pkg_cache pkg_cache::_open_for_directory(bool writeable, path_ref dirpath) {
    auto try_read_sdist = [](path_ref p) -> std::optional<sdist> {
        if (starts_with(p.filename().string(), ".")) {
            return std::nullopt;
        }
        try {
            return sdist::from_directory(p);
        } catch (const std::runtime_error& e) {
            dds_log(error,
                    "Failed to load source distribution from directory '{}': {}",
                    p.string(),
                    e.what());
            return std::nullopt;
        }
    };

    auto entries =
        // Get the top-level `name-version` dirs
        fs::directory_iterator(dirpath)  //
        | neo::lref                      //
        //  Convert each dir into an `sdist` object
        | ranges::views::transform(try_read_sdist)  //
        //  Drop items that failed to load
        | ranges::views::filter([](auto&& opt) { return opt.has_value(); })  //
        | ranges::views::transform([](auto&& opt) { return *opt; })          //
        | to<sdist_set>();

    return {writeable, dirpath, std::move(entries)};
}

void pkg_cache::add_sdist(const sdist& sd, if_exists ife_action) {
    neo_assertion_breadcrumbs("Importing sdist archive", sd.manifest.id.to_string());
    if (!_write_enabled) {
        dds_log(critical,
                "DDS attempted to write into a cache that wasn't opened with a write-lock. This "
                "is a hard bug and should be reported. For the safety and integrity of the local "
                "cache, we'll hard-exit immediately.");
        std::terminate();
    }
    auto sd_dest = _root / sd.manifest.id.to_string();
    if (fs::exists(sd_dest)) {
        auto msg = fmt::
            format("Package '{}' (Importing from [{}]) is already available in the local cache",
                   sd.manifest.id.to_string(),
                   sd.path.string());
        if (ife_action == if_exists::throw_exc) {
            throw_user_error<errc::sdist_exists>(msg);
        } else if (ife_action == if_exists::ignore) {
            dds_log(warn, msg);
            return;
        } else {
            dds_log(info, msg + " - Replacing");
        }
    }
    auto tmp_copy = sd_dest;
    tmp_copy.replace_filename(".tmp-import");
    if (fs::exists(tmp_copy)) {
        fs::remove_all(tmp_copy);
    }
    fs::create_directories(tmp_copy.parent_path());
    fs::copy(sd.path, tmp_copy, fs::copy_options::recursive);
    if (fs::exists(sd_dest)) {
        fs::remove_all(sd_dest);
    }
    fs::rename(tmp_copy, sd_dest);
    _sdists.insert(sdist::from_directory(sd_dest));
    dds_log(info, "Source distribution '{}' successfully exported", sd.manifest.id.to_string());
}

const sdist* pkg_cache::find(const pkg_id& pkg) const noexcept {
    auto found = _sdists.find(pkg);
    if (found == _sdists.end()) {
        return nullptr;
    }
    return &*found;
}

std::vector<pkg_id> pkg_cache::solve(const std::vector<dependency>& deps,
                                     const pkg_db&                  ctlg) const {
    return dds::solve(
        deps,
        [&](std::string_view name) -> std::vector<pkg_id> {
            auto mine = ranges::views::all(_sdists)  //
                | ranges::views::filter(
                            [&](const sdist& sd) { return sd.manifest.id.name == name; })
                | ranges::views::transform([](const sdist& sd) { return sd.manifest.id; });
            auto avail = ctlg.by_name(name);
            auto all   = ranges::views::concat(mine, avail) | ranges::to_vector;
            ranges::sort(all, std::less{});
            ranges::unique(all, std::less{});
            return all;
        },
        [&](const pkg_id& pkg_id) {
            auto found = find(pkg_id);
            if (found) {
                return found->manifest.dependencies;
            }
            return ctlg.dependencies_of(pkg_id);
        });
}