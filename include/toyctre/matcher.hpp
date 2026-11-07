#pragma once

#include "toyctre/lib.hpp"
#include <string_view>

namespace toyctre {

bool match(const Regex &regex, std::string_view text);

} // namespace toyctre
