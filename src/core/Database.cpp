#ifdef _WIN32
#define timegm _mkgmtime
#endif

#include "core/Database.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

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
void write_le(std::ostream& os, T v) {
    for (size_t i = 0; i < sizeof(T); ++i) { os.put((char)(v & 0xFF)); v >>= 8; }
}

template<typename T>
T read_le(std::istream& is) {
    T v = 0;
    for (size_t i = 0; i < sizeof(T); ++i) {
        unsigned char b = 0;
        is.read((char*)&b, 1);
        v |= (T)b << (8 * i);
    }
    return v;
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

// .pif v2 header layout. Every field here is covered by the GCM tag, because
// the AAD is the verbatim header block rather than a reconstruction of it.
constexpr size_t OFF_MAGIC   = 0;   // 4
constexpr size_t OFF_VERSION = 4;   // 2
constexpr size_t OFF_FLAGS   = 6;   // 2  reserved (keyfile etc.), authenticated
constexpr size_t OFF_KDF     = 8;   // 2  KdfId
constexpr size_t OFF_CIPHER  = 10;  // 2  cipher construction id
constexpr size_t OFF_SALT    = 12;  // 32
constexpr size_t OFF_MEM     = 44;  // 4
constexpr size_t OFF_ITER    = 48;  // 4
constexpr size_t OFF_PAR     = 52;  // 4
constexpr size_t OFF_PBK     = 56;  // 4
constexpr size_t OFF_IV      = 60;  // 12  outer AES-GCM IV
constexpr size_t OFF_NONCE   = 72;  // 12  inner ChaCha20-Poly1305 nonce
constexpr size_t OFF_LEN     = 84;  // 4   outer ciphertext length
constexpr size_t HEADER_SIZE = 88;

} // namespace

nlohmann::json DatabaseMetadata::to_json() const {
    return {{"version", version}, {"name", name},
            {"created_at", ts(created_at)}, {"modified_at", ts(modified_at)}};
}

DatabaseMetadata DatabaseMetadata::from_json(const nlohmann::json& j) {
    DatabaseMetadata m;
    if (j.contains("version"))     m.version     = j["version"].get<std::string>();
    if (j.contains("name"))        m.name        = j["name"].get<std::string>();
    if (j.contains("created_at"))  m.created_at  = st(j["created_at"].get<std::string>());
    if (j.contains("modified_at")) m.modified_at = st(j["modified_at"].get<std::string>());
    return m;
}

Database::Database() {
    metadata_.created_at = metadata_.modified_at = std::chrono::system_clock::now();
}

Database::~Database() = default;

std::unique_ptr<Database> Database::open(const std::string& path, const SecureString& pw) {
    auto db = std::make_unique<Database>();
    db->master_password_ = pw;
    db->file_path_ = path;
    db->load_from_file(path);
    db->is_dirty_ = false;
    return db;
}

std::unique_ptr<Database> Database::create(const std::string& path, const SecureString& pw) {
    auto db = std::make_unique<Database>();
    db->master_password_ = pw;
    db->file_path_ = path;
    db->salt_ = db->crypto_.generate_salt();
    db->key_cached_ = false;
    // Tune the work factor to this machine so a faster CPU yields a
    // proportionally more expensive database to attack, instead of everyone
    // sharing one conservative hardcoded cost.
    db->metadata_.argon2_params = KeyDerivation::benchmark(std::chrono::milliseconds(750));
    db->metadata_.name = "New Database";
    db->is_dirty_ = true;
    db->save();
    return db;
}

void Database::save()                       { if (file_path_.empty()) throw DatabaseException("No file path"); write_to_file(file_path_); is_dirty_ = false; }
void Database::save_as(const std::string& p) { file_path_ = p; save(); }

void Database::change_master_password(const SecureString& pw) {
    master_password_ = pw;
    salt_ = crypto_.generate_salt();
    key_cached_ = false;
    is_dirty_ = true;
}

void Database::add_account(Account a)    { accounts_[a.id()] = std::move(a); is_dirty_ = true; }
void Database::update_account(const Account& a) {
    auto it = accounts_.find(a.id());
    if (it != accounts_.end()) { it->second = a; is_dirty_ = true; }
}
void Database::remove_account(const std::string& id) {
    if (accounts_.erase(id)) is_dirty_ = true;
}

Account*       Database::get_account(const std::string& id) {
    auto it = accounts_.find(id); return it != accounts_.end() ? &it->second : nullptr;
}
const Account* Database::get_account(const std::string& id) const {
    auto it = accounts_.find(id); return it != accounts_.end() ? &it->second : nullptr;
}

std::vector<Account*> Database::get_all_accounts() {
    std::vector<Account*> r; r.reserve(accounts_.size());
    for (auto& [id, a] : accounts_) r.push_back(&a);
    return r;
}
std::vector<const Account*> Database::get_all_accounts() const {
    std::vector<const Account*> r; r.reserve(accounts_.size());
    for (const auto& [id, a] : accounts_) r.push_back(&a);
    return r;
}
std::vector<const Account*> Database::search_accounts(const std::string& q) const {
    std::vector<const Account*> r;
    for (const auto& [id, a] : accounts_) if (a.matches_search(q)) r.push_back(&a);
    return r;
}

void Database::set_name(const std::string& n) { metadata_.name = n; is_dirty_ = true; }

std::string Database::add_category(const std::string& name) {
    Category c;
    c.set_name(name);
    c.set_order((int)categories_.size());
    std::string id = c.id();
    categories_.push_back(std::move(c));
    is_dirty_ = true;
    return id;
}

void Database::rename_category(const std::string& id, const std::string& name) {
    for (auto& c : categories_) {
        if (c.id() == id) { c.set_name(name); is_dirty_ = true; return; }
    }
}

void Database::remove_category(const std::string& id) {
    auto it = std::find_if(categories_.begin(), categories_.end(),
                            [&](const Category& c) { return c.id() == id; });
    if (it == categories_.end()) return;
    categories_.erase(it);
    for (auto& [aid, a] : accounts_) {
        if (a.category_id() == id) a.set_category_id("");
    }
    for (size_t i = 0; i < categories_.size(); ++i) categories_[i].set_order((int)i);
    normalize_category_order("");
    is_dirty_ = true;
}

void Database::reorder_category(const std::string& id, int new_index) {
    auto it = std::find_if(categories_.begin(), categories_.end(),
                            [&](const Category& c) { return c.id() == id; });
    if (it == categories_.end()) return;
    Category moved = *it;
    categories_.erase(it);
    new_index = std::clamp(new_index, 0, (int)categories_.size());
    categories_.insert(categories_.begin() + new_index, moved);
    for (size_t i = 0; i < categories_.size(); ++i) categories_[i].set_order((int)i);
    is_dirty_ = true;
}

std::vector<Account*> Database::get_accounts_in_category(const std::string& category_id) {
    std::vector<Account*> r;
    for (auto& [id, a] : accounts_) {
        if (a.category_id() == category_id) r.push_back(&a);
    }
    std::sort(r.begin(), r.end(), [](Account* a, Account* b) { return a->order() < b->order(); });
    return r;
}

void Database::move_account(const std::string& account_id, const std::string& target_category_id, int target_index) {
    Account* acc = get_account(account_id);
    if (!acc) return;
    std::string old_category = acc->category_id();

    std::vector<Account*> dest;
    for (auto& [id, a] : accounts_) {
        if (id != account_id && a.category_id() == target_category_id) dest.push_back(&a);
    }
    std::sort(dest.begin(), dest.end(), [](Account* a, Account* b) { return a->order() < b->order(); });

    target_index = std::clamp(target_index, 0, (int)dest.size());
    dest.insert(dest.begin() + target_index, acc);

    acc->set_category_id(target_category_id);
    for (size_t i = 0; i < dest.size(); ++i) dest[i]->set_order((int)i);

    if (old_category != target_category_id) normalize_category_order(old_category);

    is_dirty_ = true;
}

void Database::normalize_category_order(const std::string& category_id) {
    std::vector<Account*> accts;
    for (auto& [id, a] : accounts_) {
        if (a.category_id() == category_id) accts.push_back(&a);
    }
    std::stable_sort(accts.begin(), accts.end(), [](Account* a, Account* b) { return a->order() < b->order(); });
    for (size_t i = 0; i < accts.size(); ++i) accts[i]->set_order((int)i);
}

const SecureBytes& Database::session_key() {
    // Argon2id at 256 MiB costs roughly a second. Re-deriving it on every save
    // (as the previous code did) would make saving painful and would push a
    // user toward weakening the parameters, so the key is cached for the
    // lifetime of the unlocked database and invalidated whenever the password
    // or salt changes.
    if (!key_cached_) {
        key_cache_ = crypto_.derive_key(master_password_, salt_, metadata_.argon2_params);
        key_cached_ = true;
    }
    return key_cache_;
}

void Database::load_from_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw DatabaseException("Failed to open: " + path);

    f.seekg(0, std::ios::end);
    const std::streamoff file_size = f.tellg();
    f.seekg(0, std::ios::beg);
    if (file_size < (std::streamoff)(HEADER_SIZE + CryptoEngine::TAG_SIZE))
        throw DatabaseException("Truncated or corrupted file");

    // Read the header as one opaque block. It is used verbatim as the AAD, so
    // there is no way for the authenticated bytes to drift from the parsed
    // ones the way a reconstructed AAD can.
    std::vector<uint8_t> header(HEADER_SIZE);
    f.read((char*)header.data(), HEADER_SIZE);
    if (!f) throw DatabaseException("Truncated or corrupted file");

    if (get_le<uint32_t>(header, OFF_MAGIC) != MAGIC)
        throw DatabaseException("Not a pasgen database");

    const uint16_t ver = get_le<uint16_t>(header, OFF_VERSION);
    if (ver != VERSION)
        throw DatabaseException(
            "Unsupported database version " + std::to_string(ver) +
            " (this build writes version " + std::to_string(VERSION) + ")");

    const uint16_t kdf_raw = get_le<uint16_t>(header, OFF_KDF);
    if (kdf_raw != (uint16_t)KdfId::Argon2id && kdf_raw != (uint16_t)KdfId::Pbkdf2Sha256)
        throw DatabaseException("Unknown key-derivation id " + std::to_string(kdf_raw));

    const uint16_t cipher_id = get_le<uint16_t>(header, OFF_CIPHER);
    if (cipher_id != CIPHER_CASCADE)
        throw DatabaseException("Unsupported cipher id " + std::to_string(cipher_id));

    salt_.assign(header.begin() + OFF_SALT,
                 header.begin() + OFF_SALT + CryptoEngine::SALT_SIZE);

    KDFParams kp;
    kp.kdf               = (KdfId)kdf_raw;
    kp.memory_kb         = get_le<uint32_t>(header, OFF_MEM);
    kp.iterations        = get_le<uint32_t>(header, OFF_ITER);
    kp.parallelism       = get_le<uint32_t>(header, OFF_PAR);
    kp.pbkdf2_iterations = get_le<uint32_t>(header, OFF_PBK);

    // These come from an untrusted file and are handed straight to the KDF.
    // Reject rather than clamp: clamping would let a tampered header quietly
    // downgrade the work factor.
    if (!kp.valid())
        throw DatabaseException("Refusing file: implausible KDF parameters (" + kp.describe() + ")");
    if (kp.kdf == KdfId::Argon2id && !KeyDerivation::argon2_available())
        throw DatabaseException(
            "This database uses Argon2id, but this build has no Argon2 support.");
    metadata_.argon2_params = kp;

    std::vector<uint8_t> iv(header.begin() + OFF_IV,
                            header.begin() + OFF_IV + CryptoEngine::IV_SIZE);
    std::vector<uint8_t> nonce(header.begin() + OFF_NONCE,
                               header.begin() + OFF_NONCE + CryptoEngine::IV_SIZE);

    const uint32_t enc_len = get_le<uint32_t>(header, OFF_LEN);
    // Validate the declared length against the file before allocating, so a
    // short file cannot induce a multi-gigabyte allocation.
    if ((std::streamoff)enc_len != file_size - (std::streamoff)(HEADER_SIZE + CryptoEngine::TAG_SIZE))
        throw DatabaseException("Truncated or corrupted file (payload length mismatch)");

    std::vector<uint8_t> enc(enc_len);
    f.read((char*)enc.data(), enc_len);

    std::vector<uint8_t> tag(CryptoEngine::TAG_SIZE);
    f.read((char*)tag.data(), tag.size());
    if (!f) throw DatabaseException("Truncated or corrupted file");

    const SecureBytes& key = session_key();

    std::vector<uint8_t> plain;
    try { plain = crypto_.decrypt_cascade(enc, key, iv, nonce, tag, header); }
    catch (const CryptoException& e) { throw DatabaseException("Decrypt failed: " + std::string(e.what())); }

    try { from_json(nlohmann::json::parse(std::string(plain.begin(), plain.end()))); }
    catch (const nlohmann::json::exception& e) { throw DatabaseException("Parse failed: " + std::string(e.what())); }

    SecureMemory::secure_zero(plain.data(), plain.size());
}

void Database::write_to_file(const std::string& path) {
    std::string js = to_json().dump();
    std::vector<uint8_t> plain(js.begin(), js.end());
    SecureMemory::secure_zero(&js[0], js.size());

    std::vector<uint8_t> iv    = crypto_.generate_iv();     // outer, AES-256-GCM
    std::vector<uint8_t> nonce = crypto_.generate_nonce();  // inner, ChaCha20-Poly1305
    const SecureBytes& key = session_key();
    const KDFParams& kp = metadata_.argon2_params;

    // GCM ciphertext is the same length as the plaintext, so the length field
    // is known before encrypting and can be authenticated along with the rest
    // of the header.
    std::vector<uint8_t> header;
    header.reserve(HEADER_SIZE);
    put_le<uint32_t>(header, MAGIC);
    put_le<uint16_t>(header, VERSION);
    put_le<uint16_t>(header, 0);                       // flags, reserved
    put_le<uint16_t>(header, (uint16_t)kp.kdf);
    put_le<uint16_t>(header, CIPHER_CASCADE);
    header.insert(header.end(), salt_.begin(), salt_.end());
    put_le<uint32_t>(header, kp.memory_kb);
    put_le<uint32_t>(header, kp.iterations);
    put_le<uint32_t>(header, kp.parallelism);
    put_le<uint32_t>(header, kp.pbkdf2_iterations);
    header.insert(header.end(), iv.begin(), iv.end());
    header.insert(header.end(), nonce.begin(), nonce.end());
    // The outer layer encrypts the inner ciphertext *and* the inner tag, so the
    // stored payload is one tag longer than the plaintext.
    put_le<uint32_t>(header, (uint32_t)(plain.size() + CryptoEngine::TAG_SIZE));
    if (header.size() != HEADER_SIZE)
        throw DatabaseException("Internal error: malformed header");

    std::vector<uint8_t> enc = crypto_.encrypt_cascade(plain, key, iv, nonce, header);
    SecureMemory::secure_zero(plain.data(), plain.size());

    std::vector<uint8_t> ct(enc.begin(), enc.end() - CryptoEngine::TAG_SIZE);
    std::vector<uint8_t> tag(enc.end() - CryptoEngine::TAG_SIZE, enc.end());

    // Write to a temporary alongside the target and rename, so an interrupted
    // save cannot leave a half-written vault where the real one used to be.
    const std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) throw DatabaseException("Failed to write: " + tmp);
        f.write((char*)header.data(), header.size());
        f.write((char*)ct.data(), ct.size());
        f.write((char*)tag.data(), tag.size());
        f.flush();
        if (!f) throw DatabaseException("Write failed");
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        throw DatabaseException("Failed to replace database file: " + path);
    }

    metadata_.modified_at = std::chrono::system_clock::now();
}

nlohmann::json Database::to_json() const {
    nlohmann::json j;
    j["metadata"] = metadata_.to_json();
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& [id, a] : accounts_) arr.push_back(a.to_json());
    j["accounts"] = arr;

    nlohmann::json carr = nlohmann::json::array();
    for (const auto& c : categories_) carr.push_back(c.to_json());
    j["categories"] = carr;
    return j;
}

void Database::from_json(const nlohmann::json& j) {
    if (j.contains("metadata")) {
        auto m = DatabaseMetadata::from_json(j["metadata"]);
        metadata_.version     = m.version;
        metadata_.name        = m.name;
        metadata_.created_at  = m.created_at;
        metadata_.modified_at = m.modified_at;
    }
    accounts_.clear();
    if (j.contains("accounts") && j["accounts"].is_array()) {
        for (const auto& aj : j["accounts"]) {
            Account a = Account::from_json(aj);
            accounts_[a.id()] = std::move(a);
        }
    }

    categories_.clear();
    if (j.contains("categories") && j["categories"].is_array()) {
        for (const auto& cj : j["categories"]) categories_.push_back(Category::from_json(cj));
    }

    // Ensure a dense, gap-free per-category order even for databases saved
    // before this field existed (all accounts default to order 0).
    normalize_category_order("");
    for (const auto& c : categories_) normalize_category_order(c.id());
}

} // namespace pasgen
