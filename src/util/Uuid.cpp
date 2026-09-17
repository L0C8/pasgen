#include "util/Uuid.hpp"
#include <random>
#include <sstream>

namespace pasgen {

std::string generate_uuid() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> d(0, 15), d2(8, 11);
    std::ostringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8;  i++) ss << d(gen);
    ss << "-";
    for (int i = 0; i < 4;  i++) ss << d(gen);
    ss << "-4";
    for (int i = 0; i < 3;  i++) ss << d(gen);
    ss << "-";
    ss << d2(gen);
    for (int i = 0; i < 3;  i++) ss << d(gen);
    ss << "-";
    for (int i = 0; i < 12; i++) ss << d(gen);
    return ss.str();
}

} // namespace pasgen
