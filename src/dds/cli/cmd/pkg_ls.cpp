#include "../options.hpp"

#include <dds/pkg/cache.hpp>
#include <dds/sdist/dist.hpp>
#include <dds/util/result.hpp>

#include <boost/leaf/handle_exception.hpp>
#include <neo/assert.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/group_by.hpp>
#include <range/v3/view/transform.hpp>

#include <iostream>
#include <string_view>

namespace dds::cli::cmd {
static int _pkg_ls(const options& opts) {
    auto list_contents = [&](pkg_cache repo) {
        auto same_name
            = [](auto&& a, auto&& b) { return a.manifest.id.name == b.manifest.id.name; };

        auto all         = repo.iter_sdists();
        auto grp_by_name = all                             //
            | ranges::views::group_by(same_name)           //
            | ranges::views::transform(ranges::to_vector)  //
            | ranges::views::transform([](auto&& grp) {
                               assert(grp.size() > 0);
                               return std::pair(grp[0].manifest.id.name, grp);
                           });

        for (const auto& [name, grp] : grp_by_name) {
            dds_log(info, "{}:", name.str);
            for (const dds::sdist& sd : grp) {
                dds_log(info, "  - {}", sd.manifest.id.version.to_string());
            }
        }

        return 0;
    };

    return dds::pkg_cache::with_cache(opts.pkg_cache_dir.value_or(pkg_cache::default_local_path()),
                                      dds::pkg_cache_flags::read,
                                      list_contents);
}

int pkg_ls(const options& opts) {
    return boost::leaf::try_catch(
        [&] {
            try {
                return _pkg_ls(opts);
            } catch (...) {
                dds::capture_exception();
            }
        },
        [](boost::leaf::catch_<neo::sqlite3::error> e) {
            dds_log(error, "Unexpected database error: {}", e.value().what());
            return 1;
        });
}
}  // namespace dds::cli::cmd
