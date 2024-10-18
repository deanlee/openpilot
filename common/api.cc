
#include "common/api.h"

#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <cassert>
#include <iostream>

#include "common/params.h"
#include "system/hardware/hw.h"

namespace CommaApi {

// Base64 URL-safe character set (uses '-' and '_' instead of '+' and '/')
static const std::string base64url_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789-_";

std::string base64url_encode(const std::string &in) {
  std::string out;
  int val = 0, valb = -6;
  for (unsigned char c : in) {
    val = (val << 8) + c;
    valb += 8;
    while (valb >= 0) {
      out.push_back(base64url_chars[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }
  return out;
}

RSA *get_rsa_private_key() {
  static std::unique_ptr<RSA, decltype(&RSA_free)> rsa_private(nullptr, RSA_free);
  if (!rsa_private) {
    FILE *fp = fopen(Path::rsa_file().c_str(), "rb");
    if (!fp) {
      std::cerr << "No RSA private key found, please run manager.py or registration.py" << std::endl;
      return nullptr;
    }
    rsa_private.reset(PEM_read_RSAPrivateKey(fp, NULL, NULL, NULL));
    fclose(fp);
  }
  return rsa_private.get();
}

std::string rsa_sign(const std::string &data) {
  RSA *rsa_private = get_rsa_private_key();
  if (!rsa_private) return {};

  std::vector<unsigned char> sig(RSA_size(rsa_private));
  unsigned int sig_len;
  int ret = RSA_sign(NID_sha256, (const unsigned char *)data.data(), data.size(),
                     sig.data(), &sig_len, rsa_private);
  assert(ret == 1);
  assert(sig.size() == sig_len);
  return std::string(sig.begin(), sig.begin() + sig_len);
}

std::string create_jwt2(const json11::Json &payloads, int expiry) {
  std::string header = "{\"alg\":\"RS256\"}";

  int now = std::chrono::seconds(std::time(nullptr)).count();
  std::string dongleId = Params().get("DongleId");
  std::string payload = "{\"exp\":" + std::to_string(now + expiry) +
                        ",\"iat\":" + std::to_string(now) +
                        ",\"identity\":\"" + dongleId + "\"" +
                        ",\"nbf\":" + std::to_string(now) + "}";
  // Merge additional payloads
  // for (const auto &item : payloads.object_items()) {
  //   payload[item.first] = item.second;
  // }

  // Encode header and payload
  std::string jwt = base64url_encode(header) + '.' + base64url_encode(payload);
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256(reinterpret_cast<const unsigned char *>(jwt.data()), jwt.size(), hash);
  std::string signature = rsa_sign(std::string(reinterpret_cast<char *>(hash), SHA256_DIGEST_LENGTH));
  return jwt + "." + base64url_encode(signature);
}

std::string create_token(bool jwt, const json11::Json &payloads, int expiry) {
  if (jwt) {
    return create_jwt2(payloads, expiry);
  } else {
    std::string token_json = util::read_file(util::getenv("HOME") + "/.comma/auth.json");
    std::string err;
    auto json = json11::Json::parse(token_json, err);
    if (!err.empty()) {
      std::cerr << "Error parsing JSON: " << err << std::endl;
      return "";
    }
    // Extract the access token
    return json["access_token"].string_value();
  }
}

}  // namespace CommaApi
