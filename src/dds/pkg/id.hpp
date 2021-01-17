#pragma once

#include "./name.hpp"

#include <semver/version.hpp>

#include <string>
#include <string_view>
#include <tuple>

namespace dds {

struct e_pkg_id_str {
    std::string value;
};

/**
 * Represents a unique package ID. We store this as a simple name-version pair.
 *
 * In text, this is represented with an `@` symbol in between. The `parse` and
 * `to_string` method convert between this textual representation, and supports
 * full round-trips.
 */
struct pkg_id {
    /// The name of the package
    dds::name name;
    /// The version of the package
    semver::version version;

    /**
     * Parse the given string into a pkg_id object.
     */
    static pkg_id parse(std::string_view);

    /**d
     * Convert this pkg_id into its corresponding textual representation.
     * The returned string can be passed back to `parse()` for a round-trip
     */
    std::string to_string() const noexcept;

    friend bool operator<(const pkg_id& lhs, const pkg_id& rhs) noexcept {
        return std::tie(lhs.name, lhs.version) < std::tie(rhs.name, rhs.version);
    }
    friend bool operator==(const pkg_id& lhs, const pkg_id& rhs) noexcept {
        return std::tie(lhs.name, lhs.version) == std::tie(rhs.name, rhs.version);
    }
};

}  // namespace dds
