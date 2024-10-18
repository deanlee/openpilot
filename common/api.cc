
#include "common/api.h"

#include <openssl/pem.h>
#include <openssl/rsa.h>

#include "system/hardware/hw.h"
#include "common/params.h"
namespace CommaApi {

static const std::string base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";

std::string base64_encode(const std::string &input) {
  std::string output;
  int val = 0, valb = -6;

  for (unsigned char c : input) {
    val = (val << 8) + c;
    valb += 8;
    while (valb >= 0) {
      output.push_back(base64_chars[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }

  if (valb > -6) {
    output.push_back(base64_chars[((val << 8) >> valb) & 0x3F]);
  }

  while (output.size() % 4) {
    output.push_back('=');
  }

  return output;
}

RSA *get_rsa_private_key() {
  static std::unique_ptr<RSA, decltype(&RSA_free)> rsa_private(nullptr, RSA_free);
  if (!rsa_private) {
    FILE *fp = fopen(Path::rsa_file().c_str(), "rb");
    if (!fp) {
      printf("No RSA private key found, please run manager.py or registration.py\n");
      return nullptr;
    }
    rsa_private.reset(PEM_read_RSAPrivateKey(fp, NULL, NULL, NULL));
    fclose(fp);
  }
  return rsa_private.get();
}

std::vector<uint8_t> rsa_sign(const std::vector<uint8_t> &data) {
  RSA *rsa_private = get_rsa_private_key();
  if (!rsa_private) return {};

  // SHA-256 hash the data
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256(data.data(), data.size(), hash);

  // Prepare buffer for signature
  std::vector<uint8_t> signature(RSA_size(rsa_private));
  unsigned int sig_len;

  // Sign the hash
  int ret = RSA_sign(NID_sha256, hash, SHA256_DIGEST_LENGTH, signature.data(), &sig_len, rsa_private);
  if (ret != 1) {
    // std::cerr << "RSA_sign failed: " << ERR_reason_error_string(ERR_get_error()) << std::endl;
    return {};
  }

  // Resize the signature to the actual length
  signature.resize(sig_len);
  RSA_free(rsa_private);  // Free the RSA key
  return signature;
}

std::string create_jwt(const json11::Json::object &payloads, int expiry) {
  json11::Json header = json11::Json::object{{"alg", "RS256"}};

  auto now = std::chrono::system_clock::now();
  double t = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
  // auto t = QDateTime::currentSecsSinceEpoch();
  json11::Json payload = json11::Json::object{
      {"identity", Params().get("DongleId")},
      {"nbf", t},
      {"iat", t},
      {"exp", t + expiry}};

  // for (const auto &it : payloads) {
  //   payload.object[it.first] = it.second;
  // }

  std::string jwt = base64_encode(header.dump()) + '.' + base64_encode(payload.dump());
  auto hash = SHA256(reinterpret_cast<const unsigned char *>(jwt.c_str()), jwt.size(), nullptr);
  std::string signature  ;//rsa_sign((std::vector(reinterpret_cast<uint8_t>(hash), SHA256_DIGEST_LENGTH)));
  return jwt + "." + base64_encode(signature);
}

}  // namespace CommaApi
