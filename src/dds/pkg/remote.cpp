#include "./remote.hpp"

#include <dds/dym.hpp>
#include <dds/error/errors.hpp>
#include <dds/error/nonesuch.hpp>
#include <dds/pkg/db.hpp>
#include <dds/temp.hpp>
#include <dds/util/http/pool.hpp>
#include <dds/util/log.hpp>
#include <dds/util/result.hpp>

#include <boost/leaf/handle_exception.hpp>
#include <fansi/styled.hpp>
#include <fmt/ostream.h>
#include <neo/event.hpp>
#include <neo/io/stream/buffers.hpp>
#include <neo/io/stream/file.hpp>
#include <neo/scope.hpp>
#include <neo/sqlite3/exec.hpp>
#include <neo/sqlite3/iter_tuples.hpp>
#include <neo/sqlite3/single.hpp>
#include <neo/sqlite3/transaction.hpp>
#include <neo/url.hpp>
#include <neo/utility.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/transform.hpp>

using namespace dds;
using namespace fansi::literals;
namespace nsql = neo::sqlite3;

namespace {

struct remote_db {
    temporary_dir  _tempdir;
    nsql::database db;

    static remote_db download_and_open(http_client& client, const http_response_info& resp) {
        auto tempdir    = temporary_dir::create();
        auto repo_db_dl = tempdir.path() / "repo.db";
        fs::create_directories(tempdir.path());
        auto outfile = neo::file_stream::open(repo_db_dl, neo::open_mode::write);
        client.recv_body_into(resp, neo::stream_io_buffers(outfile));

        auto db = nsql::open(repo_db_dl.string());
        return {tempdir, std::move(db)};
    }
};

}  // namespace

pkg_remote pkg_remote::connect(std::string_view url_str) {
    DDS_E_SCOPE(e_url_string{std::string(url_str)});
    const auto url = neo::url::parse(url_str);

    auto& pool   = http_pool::global_pool();
    auto  db_url = url;
    while (db_url.path.ends_with("/"))
        db_url.path.pop_back();
    auto full_path      = fmt::format("{}/{}", db_url.path, "repo.db");
    db_url.path         = full_path;
    auto [client, resp] = pool.request(db_url, http_request_params{.method = "GET"});
    auto db             = remote_db::download_and_open(client, resp);

    auto name_st = db.db.prepare("SELECT name FROM dds_repo_meta");
    auto [name]  = nsql::unpack_single<std::string>(name_st);

    return {name, url};
}

void pkg_remote::store(nsql::database_ref db) {
    auto st = db.prepare(R"(
        INSERT INTO dds_pkg_remotes (name, url)
            VALUES (?, ?)
        ON CONFLICT (name) DO
            UPDATE SET url = ?2
    )");
    nsql::exec(st, _name, _base_url.to_string());
}

void pkg_remote::update_pkg_db(nsql::database_ref              db,
                               std::optional<std::string_view> etag,
                               std::optional<std::string_view> db_mtime) {
    dds_log(info,
            "Pulling repository contents for .cyan[{}] [{}`]"_styled,
            _name,
            _base_url.to_string());

    auto& pool = http_pool::global_pool();
    auto  url  = _base_url;
    while (url.path.ends_with("/"))
        url.path.pop_back();
    auto full_path      = fmt::format("{}/{}", url.path, "repo.db");
    url.path            = full_path;
    auto [client, resp] = pool.request(url,
                                       http_request_params{
                                           .method        = "GET",
                                           .prior_etag    = etag.value_or(""),
                                           .last_modified = db_mtime.value_or(""),
                                       });
    if (resp.not_modified()) {
        // Cache hit
        dds_log(info, "Package database {} is up-to-date", _name);
        client.discard_body(resp);
        return;
    }

    auto rdb = remote_db::download_and_open(client, resp);

    auto base_url_str = _base_url.to_string();
    while (base_url_str.ends_with("/")) {
        base_url_str.pop_back();
    }

    auto db_path = rdb._tempdir.path() / "repo.db";

    auto rid_st          = db.prepare("SELECT remote_id FROM dds_pkg_remotes WHERE name = ?");
    rid_st.bindings()[1] = _name;
    auto [remote_id]     = nsql::unpack_single<std::int64_t>(rid_st);
    rid_st.reset();

    dds_log(trace, "Attaching downloaded database");
    nsql::exec(db.prepare("ATTACH DATABASE ? AS remote"), db_path.string());
    neo_defer { db.exec("DETACH DATABASE remote"); };
    nsql::transaction_guard tr{db};
    dds_log(trace, "Clearing prior contents");
    nsql::exec(  //
        db.prepare(R"(
            DELETE FROM dds_pkgs
            WHERE remote_id = ?
        )"),
        remote_id);
    dds_log(trace, "Importing packages");
    nsql::exec(  //
        db.prepare(R"(
            INSERT INTO dds_pkgs
                (name, version, description, remote_url, remote_id)
            SELECT
                name,
                version,
                description,
                CASE
                    WHEN url LIKE 'dds:%@%' THEN
                        -- Convert 'dds:name@ver' to 'dds+<base-repo-url>/name@ver'
                        -- This will later resolve to the actual package URL
                        printf('dds+%s/%s', ?2, substr(url, 5))
                    ELSE
                        -- Non-'dds:' URLs are kept as-is
                        url
                END,
                ?1
            FROM remote.dds_repo_packages
        )"),
        remote_id,
        base_url_str);
    dds_log(trace, "Importing dependencies");
    db.exec(R"(
        INSERT OR REPLACE INTO dds_pkg_deps (pkg_id, dep_name, low, high)
            SELECT
                local_pkgs.pkg_id AS pkg_id,
                dep_name,
                low,
                high
            FROM remote.dds_repo_package_deps AS deps,
                 remote.dds_repo_packages AS pkgs USING(package_id),
                 dds_pkgs AS local_pkgs USING(name, version)
    )");
    // Validate our database
    dds_log(trace, "Running integrity check");
    auto fk_check = db.prepare("PRAGMA foreign_key_check");
    auto rows = nsql::iter_tuples<std::string, std::int64_t, std::string, std::string>(fk_check);
    bool any_failed = false;
    for (auto [child_table, rowid, parent_table, failed_idx] : rows) {
        dds_log(
            critical,
            "Database foreign_key error after import: {0}.{3} referencing {2} violated at row {1}",
            child_table,
            rowid,
            parent_table,
            failed_idx);
        any_failed = true;
    }
    auto int_check = db.prepare("PRAGMA main.integrity_check");
    for (auto [error] : nsql::iter_tuples<std::string>(int_check)) {
        if (error == "ok") {
            continue;
        }
        dds_log(critical, "Database errors after import: {}", error);
        any_failed = true;
    }
    if (any_failed) {
        throw_external_error<errc::corrupted_catalog_db>(
            "Database update failed due to data integrity errors");
    }

    // Save the cache info for the remote
    if (auto new_etag = resp.etag()) {
        nsql::exec(db.prepare("UPDATE dds_pkg_remotes SET db_etag = ? WHERE name = ?"),
                   *new_etag,
                   _name);
    }
    if (auto mtime = resp.last_modified()) {
        nsql::exec(db.prepare("UPDATE dds_pkg_remotes SET db_mtime = ? WHERE name = ?"),
                   *mtime,
                   _name);
    }
}

void dds::update_all_remotes(nsql::database_ref db) {
    dds_log(info, "Updating catalog from all remotes");
    auto repos_st = db.prepare("SELECT name, url, db_etag, db_mtime FROM dds_pkg_remotes");
    auto tups     = nsql::iter_tuples<std::string,
                                  std::string,
                                  std::optional<std::string>,
                                  std::optional<std::string>>(repos_st)
        | ranges::to_vector;

    for (const auto& [name, url, etag, db_mtime] : tups) {
        DDS_E_SCOPE(e_url_string{url});
        pkg_remote repo{name, neo::url::parse(url)};
        repo.update_pkg_db(db, etag, db_mtime);
    }

    dds_log(info, "Recompacting database...");
    db.exec("VACUUM");
}

void dds::remove_remote(pkg_db& pkdb, std::string_view name) {
    auto&                   db = pkdb.database();
    nsql::transaction_guard tr{db};
    auto get_rowid_st          = db.prepare("SELECT remote_id FROM dds_pkg_remotes WHERE name = ?");
    get_rowid_st.bindings()[1] = name;
    auto row                   = nsql::unpack_single_opt<std::int64_t>(get_rowid_st);
    if (!row) {
        BOOST_LEAF_THROW_EXCEPTION(  //
            make_user_error<errc::no_catalog_remote_info>("There is no remote with name '{}'",
                                                          name),
            [&] {
                auto all_st = db.prepare("SELECT name FROM dds_pkg_remotes");
                auto tups   = nsql::iter_tuples<std::string>(all_st);
                auto names  = tups | ranges::views::transform([](auto&& tup) {
                                 auto&& [n] = tup;
                                 return n;
                             })
                    | ranges::to_vector;
                return e_nonesuch{name, did_you_mean(name, names)};
            });
    }
    auto [rowid] = *row;
    nsql::exec(db.prepare("DELETE FROM dds_pkg_remotes WHERE remote_id = ?"), rowid);
}

void dds::add_init_repo(nsql::database_ref db) noexcept {
    std::string_view init_repo = "https://repo-1.dds.pizza";
    // _Do not_ let errors stop us from continuing
    bool okay = boost::leaf::try_catch(
        [&]() -> bool {
            try {
                auto remote = pkg_remote::connect(init_repo);
                remote.store(db);
                update_all_remotes(db);
                return true;
            } catch (...) {
                capture_exception();
            }
        },
        [](http_status_error err, http_response_info resp, neo::url url) {
            dds_log(error,
                    "An HTTP error occurred while adding the initial repository [{}]: HTTP Status "
                    "{} {}",
                    err.what(),
                    url.to_string(),
                    resp.status,
                    resp.status_message);
            return false;
        },
        [](e_sqlite3_error_exc e, neo::url url) {
            dds_log(error,
                    "Error accessing remote database while adding initial repository: {}: {}",
                    url.to_string(),
                    e.message);
            return false;
        },
        [](e_sqlite3_error_exc e) {
            dds_log(error, "Unexpected database error: {}", e.message);
            return false;
        },
        [](e_system_error_exc e, network_origin conn) {
            dds_log(error,
                    "Error communicating with [.br.red[{}://{}:{}]`]: {}"_styled,
                    conn.protocol,
                    conn.hostname,
                    conn.port,
                    e.message);
            return false;
        },
        [](boost::leaf::diagnostic_info const& diag) -> int {
            dds_log(critical, "Unhandled error while adding initial package repository: ", diag);
            throw;
        });
    if (!okay) {
        dds_log(warn, "We failed to add the initial package repository [{}]", init_repo);
        dds_log(warn, "No remote packages will be available until the above issue is resolved.");
        dds_log(
            warn,
            "The remote package repository can be added again with [.br.yellow[dds pkg repo add \"{}\"]]"_styled,
            init_repo);
    }
}
