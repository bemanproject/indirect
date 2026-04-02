// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// A simple JSON-like value type using indirect to enable recursive variants.
//
// std::variant cannot directly hold itself, so recursive data structures
// traditionally require std::unique_ptr. However, unique_ptr compares by
// pointer identity (not by value), forces nullptr checks, and doesn't copy.
// indirect<T> solves all three: it provides value semantics with deep copy,
// value-based equality, and is never null (outside of moved-from state).

#include <beman/indirect/indirect.hpp>

#include <iostream>
#include <map>
#include <string>
#include <variant>
#include <vector>

using beman::indirect::indirect;

// A recursive JSON-like value: null, bool, double, string, array, or object.
// The array and object cases refer back to json_value, which is only possible
// because indirect<T> is a complete, fixed-size type regardless of T's size.
struct json_value {
    struct null_t {
        friend bool operator==(const null_t&, const null_t&) { return true; }
        friend bool operator!=(const null_t&, const null_t&) { return false; }
    };

    using array_t  = indirect<std::vector<json_value>>;
    using object_t = indirect<std::map<std::string, json_value>>;

    std::variant<null_t, bool, double, std::string, array_t, object_t> data;

    // Convenience constructors
    json_value() : data(null_t{}) {}
    json_value(bool b) : data(b) {}
    json_value(double d) : data(d) {}
    json_value(const char* s) : data(std::string(s)) {}
    json_value(std::string s) : data(std::move(s)) {}
    json_value(std::vector<json_value> a) : data(array_t{std::in_place, std::move(a)}) {}
    json_value(std::map<std::string, json_value> o) : data(object_t{std::in_place, std::move(o)}) {}

    friend bool operator==(const json_value& lhs, const json_value& rhs) { return lhs.data == rhs.data; }
    friend bool operator!=(const json_value& lhs, const json_value& rhs) { return !(lhs == rhs); }
};

std::ostream& operator<<(std::ostream& os, const json_value& v) {
    std::visit(
        [&os](const auto& val) {
            using V = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<V, json_value::null_t>) {
                os << "null";
            } else if constexpr (std::is_same_v<V, bool>) {
                os << (val ? "true" : "false");
            } else if constexpr (std::is_same_v<V, double>) {
                os << val;
            } else if constexpr (std::is_same_v<V, std::string>) {
                os << '"' << val << '"';
            } else if constexpr (std::is_same_v<V, json_value::array_t>) {
                os << '[';
                for (std::size_t i = 0; i < val->size(); ++i) {
                    if (i > 0)
                        os << ", ";
                    os << (*val)[i];
                }
                os << ']';
            } else if constexpr (std::is_same_v<V, json_value::object_t>) {
                os << '{';
                bool first = true;
                for (const auto& [k, v] : *val) {
                    if (!first)
                        os << ", ";
                    os << '"' << k << "\": " << v;
                    first = false;
                }
                os << '}';
            }
        },
        v.data);
    return os;
}

int main() {
    // Build a small JSON structure:
    //   {"name": "Alice", "scores": [10, 20, 30], "active": true}
    json_value person(std::map<std::string, json_value>{
        {"name", json_value("Alice")},
        {"scores",
         json_value(std::vector<json_value>{
             json_value(10.0),
             json_value(20.0),
             json_value(30.0),
         })},
        {"active", json_value(true)},
    });

    std::cout << "person: " << person << "\n";

    // Deep copy — the entire tree is duplicated.
    auto copy = person;
    std::cout << "copy:   " << copy << "\n";

    // Value-based equality works through the entire recursive structure.
    std::cout << "equal:  " << (person == copy ? "yes" : "no") << "\n";

    return 0;
}
