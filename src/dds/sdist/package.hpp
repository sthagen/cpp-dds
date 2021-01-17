#pragma once

#include <dds/deps.hpp>
#include <dds/pkg/id.hpp>
#include <dds/util/fs.hpp>
#include <dds/util/result.hpp>

#include <optional>
#include <string>
#include <vector>

namespace dds {

/**
 * Possible values for test_driver in a package.json5
 */
enum class test_lib {
    catch_,
    catch_main,
};

struct e_package_manifest_path {
    std::string value;
};

struct e_pkg_name_str {
    std::string value;
};

struct e_pkg_namespace_str {
    std::string value;
};

/**
 * Struct representing the contents of a `package.json5` file.
 */
struct package_manifest {
    /// The package ID, as determined by `Name` and `Version` together
    dds::pkg_id id;
    /// The declared `Namespace` of the package. This directly corresponds with the libman Namespace
    name namespace_;
    /// The `test_driver` that this package declares, or `nullopt` if absent.
    std::optional<test_lib> test_driver;
    /// The dependencies declared with the `Depends` fields, if any.
    std::vector<dependency> dependencies;

    /**
     * Load a package manifest from a file on disk.
     */
    static package_manifest load_from_file(path_ref);
    /**
     * @brief Load a package manifest from an in-memory string
     */
    static package_manifest load_from_json5_str(std::string_view, std::string_view input_name);

    /**
     * Find a package manifest contained within a directory. This will search
     * for a few file candidates and return the result from the first matching.
     * If none match, it will return nullopt.
     */
    static result<fs::path>         find_in_directory(path_ref);
    static result<package_manifest> load_from_directory(path_ref);
};

}  // namespace dds