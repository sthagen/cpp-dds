#include "../options.hpp"

#include <dds/repoman/repoman.hpp>
#include <dds/util/result.hpp>

#include <boost/leaf/handle_exception.hpp>
#include <fmt/ostream.h>
#include <neo/sqlite3/error.hpp>

namespace dds::cli::cmd {

static int _repoman_remove(const options& opts) {
    auto repo = repo_manager::open(opts.repoman.repo_dir);
    for (auto& str : opts.repoman.remove.pkgs) {
        auto id = dds::pkg_id::parse(str);
        repo.delete_package(id);
    }
    return 0;
}

int repoman_remove(const options& opts) {
    return boost::leaf::try_catch(  //
        [&] {
            try {
                return _repoman_remove(opts);
            } catch (...) {
                dds::capture_exception();
            }
        },
        [](dds::e_system_error_exc e, dds::e_repo_delete_path tgz, dds::pkg_id pkid) {
            dds_log(error,
                    "Cannot delete requested package '{}' from repository {}: {}",
                    pkid.to_string(),
                    tgz.path,
                    e.message);
            write_error_marker("repoman-rm-no-such-package");
            return 1;
        },
        [](dds::e_system_error_exc e, dds::e_open_repo_db db) {
            dds_log(error, "Error while opening repository database {}: {}", db.path, e.message);
            return 1;
        });
}

}  // namespace dds::cli::cmd
