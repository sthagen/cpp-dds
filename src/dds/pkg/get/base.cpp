#include "./base.hpp"

#include <dds/pkg/id.hpp>
#include <dds/util/log.hpp>

#include <nlohmann/json.hpp>

using namespace dds;

// void remote_pkg_base::generate_auto_lib_files(const pkg_id& pid, path_ref root) const {
//     if (auto_lib.has_value()) {
//         dds_log(info, "Generating library data automatically");

//         auto pkg_strm         = open(root / "package.json5", std::ios::binary | std::ios::out);
//         auto man_json         = nlohmann::json::object();
//         man_json["name"]      = pid.name;
//         man_json["version"]   = pid.version.to_string();
//         man_json["namespace"] = auto_lib->namespace_;
//         pkg_strm << nlohmann::to_string(man_json);

//         auto lib_strm    = open(root / "library.json5", std::ios::binary | std::ios::out);
//         auto lib_json    = nlohmann::json::object();
//         lib_json["name"] = auto_lib->name;
//         lib_strm << nlohmann::to_string(lib_json);
//     }
// }

void remote_pkg_base::get_sdist(path_ref dest) const { get_raw_directory(dest); }
void remote_pkg_base::get_raw_directory(path_ref dest) const { do_get_raw(dest); }

neo::url remote_pkg_base::to_url() const { return do_to_url(); }

std::string remote_pkg_base::to_url_string() const { return to_url().to_string(); }