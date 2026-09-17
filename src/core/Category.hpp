#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace pasgen {

class Category {
public:
    Category();
    explicit Category(const nlohmann::json& j);

    const std::string& id() const   { return id_; }
    const std::string& name() const { return name_; }
    int order() const               { return order_; }

    void set_name(const std::string& name)   { name_ = name; }
    void set_order(int order)                { order_ = order; }

    nlohmann::json to_json() const;
    static Category from_json(const nlohmann::json& j);

private:
    std::string id_;
    std::string name_;
    int order_ = 0;
};

} // namespace pasgen
