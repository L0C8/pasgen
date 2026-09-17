#include "core/Category.hpp"
#include "util/Uuid.hpp"

namespace pasgen {

Category::Category() {
    id_ = generate_uuid();
}

Category::Category(const nlohmann::json& j) { *this = from_json(j); }

nlohmann::json Category::to_json() const {
    return {{"id", id_}, {"name", name_}, {"order", order_}};
}

Category Category::from_json(const nlohmann::json& j) {
    Category c;
    if (j.contains("id"))    c.id_    = j["id"].get<std::string>();
    if (j.contains("name"))  c.name_  = j["name"].get<std::string>();
    if (j.contains("order")) c.order_ = j["order"].get<int>();
    return c;
}

} // namespace pasgen
