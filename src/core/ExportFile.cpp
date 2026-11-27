#ifdef _WIN32
#define timegm _mkgmtime
#endif

#include "core/ExportFile.hpp"
#include "crypto/CryptoEngine.hpp"
#include "crypto/KeyDerivation.hpp"
#include <filesystem>
#include "util/AtomicFile.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <nlohmann/json.hpp>

namespace pasgen {

namespace {

std::string ts(std::chrono::system_clock::time_point tp) {
    auto t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::gmtime(&t);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::chrono::system_clock::time_point st(const std::string& str) {
    std::tm tm = {};
    std::istringstream ss(str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    if (ss.fail()) return std::chrono::system_clock::now();
    return std::chrono::system_clock::from_time_t(timegm(&tm));
}

template<typename T>
void put_le(std::vector<uint8_t>& v, T x) {
    for (size_t i = 0; i < sizeof(T); ++i) {
        v.push_back((uint8_t)(x & 0xFF));
        x = (T)(x >> 8);
    }
}

template<typename T>
T get_le(const std::vector<uint8_t>& v, size_t off) {
    T x = 0;
    for (size_t i = 0; i < sizeof(T); ++i) x |= (T)v[off + i] << (8 * i);
    return x;
}

// Same field layout as the .pif v3 header (core/Database.cpp) -- an
// authenticated header used verbatim as the cascade cipher's AAD -- just
// under ExportFile::MAGIC/VERSION instead of Database's.
constexpr size_t OFF_MAGIC   = 0;   // 4
constexpr size_t OFF_VERSION = 4;   // 2
constexpr size_t OFF_FLAGS   = 6;   // 2  reserved, authenticated
constexpr size_t OFF_KDF     = 8;   // 2
constexpr size_t OFF_SALT    = 10;  // 32
constexpr size_t OFF_MEM     = 42;  // 4
constexpr size_t OFF_ITER    = 46;  // 4
constexpr size_t OFF_PAR     = 50;  // 4
constexpr size_t OFF_PBK     = 54;  // 4
constexpr size_t OFF_IV      = 58;  // 12  outer AES-GCM IV
constexpr size_t OFF_NONCE   = 70;  // 12  inner ChaCha20-Poly1305 nonce
constexpr size_t OFF_LEN     = 82;  // 4   outer ciphertext length
constexpr size_t HEADER_SIZE = 86;

nlohmann::json payload_to_json(const ExportFile::Payload& p) {
    nlohmann::json j;
    j["source_vault_name"] = p.source_vault_name;
    j["exported_at"] = ts(p.exported_at);

    nlohmann::json carr = nlohmann::json::array();
    for (const auto& c : p.categories) carr.push_back(c.to_json());
    j["categories"] = carr;

    nlohmann::json aarr = nlohmann::json::array();
    for (const auto& a : p.accounts) aarr.push_back(a.to_json());
    j["accounts"] = aarr;
    return j;
}

ExportFile::Payload payload_from_json(const nlohmann::json& j) {
    ExportFile::Payload p;
    if (j.contains("source_vault_name")) p.source_vault_name = j["source_vault_name"].get<std::string>();
    if (j.contains("exported_at"))       p.exported_at = st(j["exported_at"].get<std::string>());

    if (j.contains("categories") && j["categories"].is_array())
        for (const auto& cj : j["categories"]) p.categories.push_back(Category::from_json(cj));

    if (j.contains("accounts") && j["accounts"].is_array())
        for (const auto& aj : j["accounts"]) p.accounts.push_back(Account::from_json(aj));

    return p;
}

} // namespace

void ExportFile::write(const std::string& path, const SecureString& export_password,
                        const Payload& payload) {
    std::string js = payload_to_json(payload).dump();
    std::vector<uint8_t> plain(js.begin(), js.end());
    SecureMemory::secure_zero(&js[0], js.size());

    CryptoEngine crypto;
    std::vector<uint8_t> salt  = crypto.generate_salt();
    std::vector<uint8_t> iv    = crypto.generate_iv();     // outer, AES-256-GCM
    std::vector<uint8_t> nonce = crypto.generate_nonce();  // inner, ChaCha20-Poly1305

    // A fresh export is an infrequent, deliberate action, and the resulting
    // file may sit on a USB drive or in a downloads folder, so it gets the
    // same calibrated cost as a "Standard" .pif rather than something
    // cheaper -- see Database::apply_encryption_level(Standard).
    // benchmark() already picks PBKDF2 when this build has no Argon2. The old
    // low_memory() fallback here was stamped Argon2id, so it could never work.
    Argon2Params kp = KeyDerivation::benchmark(std::chrono::milliseconds(750));
    const SecureBytes key = crypto.derive_key(export_password, salt, kp);

    const uint32_t declared_len = (uint32_t)(plain.size() + CryptoEngine::TAG_SIZE);

    std::vector<uint8_t> header;
    header.reserve(HEADER_SIZE);
    put_le<uint32_t>(header, MAGIC);
    put_le<uint16_t>(header, VERSION);
    put_le<uint16_t>(header, 0); // flags, reserved
    put_le<uint16_t>(header, (uint16_t)kp.kdf);
    header.insert(header.end(), salt.begin(), salt.end());
    put_le<uint32_t>(header, kp.memory_kb);
    put_le<uint32_t>(header, kp.iterations);
    put_le<uint32_t>(header, kp.parallelism);
    put_le<uint32_t>(header, kp.pbkdf2_iterations);
    header.insert(header.end(), iv.begin(), iv.end());
    header.insert(header.end(), nonce.begin(), nonce.end());
    put_le<uint32_t>(header, declared_len);
    if (header.size() != HEADER_SIZE)
        throw ExportException("Internal error: malformed header");

    std::vector<uint8_t> enc = crypto.encrypt_cascade(plain, key, iv, nonce, header);
    SecureMemory::secure_zero(plain.data(), plain.size());

    std::vector<uint8_t> ct(enc.begin(), enc.end() - CryptoEngine::TAG_SIZE);
    std::vector<uint8_t> tag(enc.end() - CryptoEngine::TAG_SIZE, enc.end());

    // Temp file + flush + rename (see util/AtomicFile.hpp), so an interrupted
    // save cannot leave a half-written file where the real one used to be.
    std::vector<uint8_t> file;
    file.reserve(header.size() + ct.size() + tag.size());
    file.insert(file.end(), header.begin(), header.end());
    file.insert(file.end(), ct.begin(), ct.end());
    file.insert(file.end(), tag.begin(), tag.end());
    try { write_file_atomic(path, file); }
    catch (const std::exception& e) { throw ExportException(e.what()); }
}

ExportFile::Payload ExportFile::read(const std::string& path, const SecureString& export_password) {
    std::ifstream f(path_from_utf8(path), std::ios::binary);
    if (!f) throw ExportException("Failed to open: " + path);

    f.seekg(0, std::ios::end);
    const std::streamoff file_size = f.tellg();
    f.seekg(0, std::ios::beg);
    if (file_size < (std::streamoff)(HEADER_SIZE + CryptoEngine::TAG_SIZE))
        throw ExportException("Truncated or corrupted file");

    std::vector<uint8_t> header(HEADER_SIZE);
    f.read((char*)header.data(), HEADER_SIZE);
    if (!f) throw ExportException("Truncated or corrupted file");

    if (get_le<uint32_t>(header, OFF_MAGIC) != MAGIC)
        throw ExportException("Not a pasgen export file (.eif)");
    if (get_le<uint16_t>(header, OFF_VERSION) != VERSION)
        throw ExportException("Unsupported export file version");

    const uint16_t kdf_raw = get_le<uint16_t>(header, OFF_KDF);
    if (kdf_raw != (uint16_t)KdfId::Argon2id && kdf_raw != (uint16_t)KdfId::Pbkdf2Sha256)
        throw ExportException("Unknown key-derivation id " + std::to_string(kdf_raw));

    std::vector<uint8_t> salt(header.begin() + OFF_SALT,
                              header.begin() + OFF_SALT + CryptoEngine::SALT_SIZE);

    KDFParams kp;
    kp.kdf               = (KdfId)kdf_raw;
    kp.memory_kb         = get_le<uint32_t>(header, OFF_MEM);
    kp.iterations        = get_le<uint32_t>(header, OFF_ITER);
    kp.parallelism       = get_le<uint32_t>(header, OFF_PAR);
    kp.pbkdf2_iterations = get_le<uint32_t>(header, OFF_PBK);
    // Untrusted file, fed straight into the KDF -- reject rather than clamp,
    // same reasoning as Database::load_v3.
    if (!kp.valid())
        throw ExportException("Refusing file: implausible KDF parameters (" + kp.describe() + ")");
    if (kp.kdf == KdfId::Argon2id && !KeyDerivation::argon2_available())
        throw ExportException("This file uses Argon2id, but this build has no Argon2 support.");

    std::vector<uint8_t> iv(header.begin() + OFF_IV, header.begin() + OFF_IV + CryptoEngine::IV_SIZE);
    std::vector<uint8_t> nonce(header.begin() + OFF_NONCE, header.begin() + OFF_NONCE + CryptoEngine::IV_SIZE);

    const uint32_t enc_len = get_le<uint32_t>(header, OFF_LEN);
    if ((std::streamoff)enc_len != file_size - (std::streamoff)(HEADER_SIZE + CryptoEngine::TAG_SIZE))
        throw ExportException("Truncated or corrupted file (payload length mismatch)");

    std::vector<uint8_t> enc(enc_len);
    f.read((char*)enc.data(), enc_len);
    std::vector<uint8_t> tag(CryptoEngine::TAG_SIZE);
    f.read((char*)tag.data(), tag.size());
    if (!f) throw ExportException("Truncated or corrupted file");

    CryptoEngine crypto;
    const SecureBytes key = crypto.derive_key(export_password, salt, kp);

    std::vector<uint8_t> plain;
    try { plain = crypto.decrypt_cascade(enc, key, iv, nonce, tag, header); }
    catch (const CryptoException&) {
        throw ExportException("Wrong export password, or the file is corrupted.");
    }

    Payload result;
    try { result = payload_from_json(nlohmann::json::parse(std::string(plain.begin(), plain.end()))); }
    catch (const nlohmann::json::exception& e) {
        SecureMemory::secure_zero(plain.data(), plain.size());
        throw ExportException("Parse failed: " + std::string(e.what()));
    }
    SecureMemory::secure_zero(plain.data(), plain.size());
    return result;
}

} // namespace pasgen
