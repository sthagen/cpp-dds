#include "../options.hpp"

#include "./build_common.hpp"

#include <dds/build/builder.hpp>
#include <dds/error/errors.hpp>
#include <dds/pkg/db.hpp>
#include <dds/pkg/remote.hpp>
#include <dds/toolchain/from_json.hpp>

using namespace dds;

namespace dds::cli::cmd {

static int _build(const options& opts) {
    if (!opts.build.add_repos.empty()) {
        auto cat = opts.open_pkg_db();
        for (auto& str : opts.build.add_repos) {
            auto repo = pkg_remote::connect(str);
            repo.store(cat.database());
        }
    }

    if (opts.build.update_repos || !opts.build.add_repos.empty()) {
        update_all_remotes(opts.open_pkg_db().database());
    }

    auto builder = create_project_builder(opts);
    builder.build({
        .out_root          = opts.out_path.value_or(fs::current_path() / "_build"),
        .existing_lm_index = opts.build.lm_index,
        .emit_lmi          = {},
        .tweaks_dir        = opts.build.tweaks_dir,
        .toolchain         = opts.load_toolchain(),
        .parallel_jobs     = opts.jobs,
    });

    return 0;
}

int build(const options& opts) {
    return handle_build_error([&] { return _build(opts); });
}

}  // namespace dds::cli::cmd
