#ifdef _WIN32
#define timegm _mkgmtime
#endif

#include "core/Database.hpp"
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

void Database::load_from_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw DatabaseException("Failed to open: " + path);

    if (read_le<uint32_t>(f) != MAGIC)   throw DatabaseException("Invalid file format");
    if (read_le<uint16_t>(f) > VERSION)  throw DatabaseException("Unsupported file version");
    read_le<uint16_t>(f); // flags

    salt_.resize(CryptoEngine::SALT_SIZE);
    f.read((char*)salt_.data(), salt_.size());

    metadata_.argon2_params.memory_kb   = read_le<uint32_t>(f);
    metadata_.argon2_params.iterations  = read_le<uint32_t>(f);
    metadata_.argon2_params.parallelism = read_le<uint32_t>(f);

    std::vector<uint8_t> iv(CryptoEngine::IV_SIZE);
    f.read((char*)iv.data(), iv.size());

    uint32_t enc_len = read_le<uint32_t>(f);
    std::vector<uint8_t> enc(enc_len);
    f.read((char*)enc.data(), enc_len);

    std::vector<uint8_t> tag(CryptoEngine::TAG_SIZE);
    f.read((char*)tag.data(), tag.size());

    if (!f) throw DatabaseException("Truncated or corrupted file");

    SecureBytes key = crypto_.derive_key(master_password_, salt_, metadata_.argon2_params);

    std::vector<uint8_t> aad;
    aad.reserve(52);
    for (int i = 0; i < 4; i++) aad.push_back((MAGIC >> (8*i)) & 0xFF);
    aad.push_back(VERSION & 0xFF); aad.push_back((VERSION >> 8) & 0xFF);
    aad.push_back(0); aad.push_back(0);
    aad.insert(aad.end(), salt_.begin(), salt_.end());
    for (int i = 0; i < 4; i++) aad.push_back((metadata_.argon2_params.memory_kb   >> (8*i)) & 0xFF);
    for (int i = 0; i < 4; i++) aad.push_back((metadata_.argon2_params.iterations  >> (8*i)) & 0xFF);
    for (int i = 0; i < 4; i++) aad.push_back((metadata_.argon2_params.parallelism >> (8*i)) & 0xFF);

    std::vector<uint8_t> plain;
    try { plain = crypto_.decrypt_with_iv(enc, key, iv, tag, aad); }
    catch (const CryptoException& e) { throw DatabaseException("Decrypt failed: " + std::string(e.what())); }

    try { from_json(nlohmann::json::parse(std::string(plain.begin(), plain.end()))); }
    catch (const nlohmann::json::exception& e) { throw DatabaseException("Parse failed: " + std::string(e.what())); }
}

void Database::write_to_file(const std::string& path) {
    std::string js = to_json().dump();
    std::vector<uint8_t> plain(js.begin(), js.end());
    std::vector<uint8_t> iv = crypto_.generate_iv();
    SecureBytes key = crypto_.derive_key(master_password_, salt_, metadata_.argon2_params);

    std::vector<uint8_t> aad;
    aad.reserve(52);
    for (int i = 0; i < 4; i++) aad.push_back((MAGIC >> (8*i)) & 0xFF);
    aad.push_back(VERSION & 0xFF); aad.push_back((VERSION >> 8) & 0xFF);
    aad.push_back(0); aad.push_back(0);
    aad.insert(aad.end(), salt_.begin(), salt_.end());
    for (int i = 0; i < 4; i++) aad.push_back((metadata_.argon2_params.memory_kb   >> (8*i)) & 0xFF);
    for (int i = 0; i < 4; i++) aad.push_back((metadata_.argon2_params.iterations  >> (8*i)) & 0xFF);
    for (int i = 0; i < 4; i++) aad.push_back((metadata_.argon2_params.parallelism >> (8*i)) & 0xFF);

    std::vector<uint8_t> enc = crypto_.encrypt_with_iv(plain, key, iv, aad);
    std::vector<uint8_t> ct(enc.begin(), enc.end() - CryptoEngine::TAG_SIZE);
    std::vector<uint8_t> tag(enc.end() - CryptoEngine::TAG_SIZE, enc.end());

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) throw DatabaseException("Failed to write: " + path);

    write_le(f, MAGIC);
    write_le(f, VERSION);
    write_le<uint16_t>(f, 0);
    f.write((char*)salt_.data(), salt_.size());
    write_le(f, metadata_.argon2_params.memory_kb);
    write_le(f, metadata_.argon2_params.iterations);
    write_le(f, metadata_.argon2_params.parallelism);
    f.write((char*)iv.data(), iv.size());
    write_le(f, (uint32_t)ct.size());
    f.write((char*)ct.data(), ct.size());
    f.write((char*)tag.data(), tag.size());

    if (!f) throw DatabaseException("Write failed");
    metadata_.modified_at = std::chrono::system_clock::now();
}

nlohmann::json Database::to_json() const {
    nlohmann::json j;
    j["metadata"] = metadata_.to_json();
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& [id, a] : accounts_) arr.push_back(a.to_json());
    j["accounts"] = arr;
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
}

} // namespace pasgen
