#pragma once

#include <string>
#include <vector>

#include "common/util.h"
#include "third_party/json11/json11.hpp"

namespace CommaApi {

const std::string BASE_URL = util::getenv("API_HOST", "https://api.commadotai.com");
std::vector<uint8_t> rsa_sign(const std::vector<uint8_t> &data);
std::string create_jwt(const json11::Json::object &payloads = {}, int expiry = 3600);

}  // namespace CommaApi
