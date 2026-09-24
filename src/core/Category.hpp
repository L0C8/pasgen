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
    // Index into Palette::category the user picked for this category's dot
    // and header text, or -1 to fall back to the old behavior of cycling by
    // position in the sidebar.
    int color_index() const         { return color_index_; }

    void set_name(const std::string& name)   { name_ = name; }
    void set_order(int order)                { order_ = order; }
    void set_color_index(int color_index)    { color_index_ = color_index; }

    nlohmann::json to_json() const;
    static Category from_json(const nlohmann::json& j);

private:
    std::string id_;
    std::string name_;
    int order_ = 0;
    int color_index_ = -1;
};

} // namespace pasgen
