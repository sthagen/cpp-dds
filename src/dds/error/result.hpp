#pragma once

#include "./result_fwd.hpp"

#include <boost/leaf/error.hpp>
#include <boost/leaf/result.hpp>

namespace dds {

using boost::leaf::new_error;

struct e_human_message {
    std::string value;
};

}  // namespace dds
