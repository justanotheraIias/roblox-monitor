#include <iostream>
#include "json.hpp"
using json = nlohmann::json;

int main() {
    json j;
    j["test"] = "hello";
    std::cout << j.dump() << std::endl;
    return 0;
}