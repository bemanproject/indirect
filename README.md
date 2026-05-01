# beman.indirect: Vocabulary types for composite class design

<!--
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-->

<!-- markdownlint-disable-next-line line-length -->
![Library Status](https://raw.githubusercontent.com/bemanproject/beman/refs/heads/main/images/badges/beman_badge-beman_library_under_development.svg) ![Continuous Integration Tests](https://github.com/bemanproject/indirect/actions/workflows/ci_tests.yml/badge.svg) ![Lint Check (pre-commit)](https://github.com/bemanproject/indirect/actions/workflows/pre-commit-check.yml/badge.svg) [![Coverage](https://coveralls.io/repos/github/bemanproject/indirect/badge.svg?branch=main)](https://coveralls.io/github/bemanproject/indirect?branch=main) ![Standard Target](https://github.com/bemanproject/beman/blob/main/images/badges/cpp29.svg) [![Compiler Explorer Example](https://img.shields.io/badge/Try%20it%20on%20Compiler%20Explorer-grey?logo=compilerexplorer&logoColor=67c52a)](https://www.example.com)

`beman.indirect` implements `std::indirect` and `std::polymorphic`, vocabulary types for
composite class design. These types provide value semantics for owned heap-allocated objects:

- **`indirect<T>`**: Owns a heap-allocated `T` with deep-copy semantics and const-propagation.
- **`polymorphic<T>`**: Owns a heap-allocated object derived from `T` with polymorphic deep-copy via type erasure.

**Implements**: `std::indirect` and `std::polymorphic` proposed in [P3019R14](https://wg21.link/P3019R14).

**Status**: [Under development and not yet ready for production use.](https://github.com/bemanproject/beman/blob/main/docs/beman_library_maturity_model.md#under-development-and-not-yet-ready-for-production-use)

## License

`beman.indirect` is licensed under the Apache License v2.0 with LLVM Exceptions.

## Usage

```cpp
#include <beman/indirect/indirect.hpp>
#include <beman/indirect/polymorphic.hpp>

// indirect: value-semantic heap-allocated int
beman::indirect::indirect<int> i(42);
auto copy = i;    // deep copy
*copy = 100;      // original unchanged

// polymorphic: value-semantic polymorphic ownership
struct Shape { virtual double area() const = 0; virtual ~Shape() = default; /* ... */ };
struct Circle : Shape { double r; double area() const override { return 3.14 * r * r; } /* ... */ };
beman::indirect::polymorphic<Shape> shape(Circle{5.0});
auto shape_copy = shape;  // deep copy preserves dynamic type
```

### Recursive variants

`std::variant` cannot directly contain itself, so recursive data structures
traditionally use `std::unique_ptr`. But `unique_ptr` compares by pointer
identity (not by value), forces null checks, and doesn't copy. `indirect<T>`
solves all three: it provides value semantics with deep copy, value-based
equality, and is never null (outside of moved-from state).

```cpp
#include <beman/indirect/indirect.hpp>

#include <map>
#include <string>
#include <variant>
#include <vector>

using beman::indirect::indirect;

struct json_value {
    struct null_t {
        bool operator==(const null_t&) const = default;
    };

    using array_t  = indirect<std::vector<json_value>>;
    using object_t = indirect<std::map<std::string, json_value>>;

    std::variant<null_t, bool, double, std::string, array_t, object_t> data;

    json_value() : data(null_t{}) {}
    json_value(double d) : data(d) {}
    json_value(const char* s) : data(std::string(s)) {}
    json_value(std::string s) : data(std::move(s)) {}
    json_value(std::vector<json_value> a) : data(array_t{std::in_place, std::move(a)}) {}
    json_value(std::map<std::string, json_value> o) : data(object_t{std::in_place, std::move(o)}) {}

    bool operator==(const json_value&) const = default;
};

json_value person(std::map<std::string, json_value>{
    {"name", json_value("Alice")},
    {"scores", json_value(std::vector<json_value>{
        json_value(10.0),
        json_value(20.0),
    })},
});

auto copy = person;   // deep copy of the entire tree
assert(person == copy); // value-based equality through the recursive structure
```

Full runnable examples can be found in [`examples/`](examples/).

## Dependencies

### Build Environment

This project requires at least the following to build:

* A C++ compiler that conforms to the C++17 standard or greater
* CMake 3.30 or later
* (Test Only) GoogleTest

You can disable building tests by setting CMake option `BEMAN_INDIRECT_BUILD_TESTS` to
`OFF` when configuring the project.

### Supported Platforms

| Compiler   | Version | C++ Standards | Standard Library  |
|------------|---------|---------------|-------------------|
| GCC        | 16-13   | C++26-C++17   | libstdc++         |
| GCC        | 12-11   | C++23-C++17   | libstdc++         |
| Clang      | 22-19   | C++26-C++17   | libstdc++, libc++ |
| Clang      | 18      | C++26-C++17   | libc++            |
| Clang      | 18      | C++23-C++17   | libstdc++         |
| Clang      | 17      | C++26-C++17   | libc++            |
| Clang      | 17      | C++20, C++17  | libstdc++         |
| AppleClang | latest  | C++26-C++17   | libc++            |
| MSVC       | latest  | C++23, C++17  | MSVC STL          |

## Development

See the [Contributing Guidelines](CONTRIBUTING.md).

## Integrate beman.indirect into your project

### Build

You can build indirect using a CMake workflow preset:

```bash
cmake --workflow --preset gcc-release
```

To list available workflow presets, you can invoke:

```bash
cmake --list-presets=workflow
```

For details on building beman.indirect without using a CMake preset, refer to the
[Contributing Guidelines](CONTRIBUTING.md).

### Installation

#### Vcpkg

The preferred way to install indirect is via vcpkg. To do so, after installing vcpkg
itself, you need to add support for the Beman project's [vcpkg
registry](https://github.com/bemanproject/vcpkg-registry) by configuring a
`vcpkg-configuration.json` file (which indirect [provides](vcpkg-configuration.json)).

Then, simply run `vcpkg install beman-indirect`.

#### Manual

To install beman.indirect globally after building with the `gcc-release` preset, you can
run:

```bash
sudo cmake --install build/gcc-release
```

Alternatively, to install to a prefix, for example `/opt/beman`, you can run:

```bash
sudo cmake --install build/gcc-release --prefix /opt/beman
```

This will generate the following directory structure:

```txt
/opt/beman
├── include
│   └── beman
│       └── indirect
│           ├── indirect.hpp
│           └── ...
└── lib
    └── cmake
        └── beman.indirect
            ├── beman.indirect-config-version.cmake
            ├── beman.indirect-config.cmake
            └── beman.indirect-targets.cmake
```

### CMake Configuration

If you installed beman.indirect to a prefix, you can specify that prefix to your CMake
project using `CMAKE_PREFIX_PATH`; for example, `-DCMAKE_PREFIX_PATH=/opt/beman`.

You need to bring in the `beman.indirect` package to define the `beman::indirect` CMake
target:

```cmake
find_package(beman.indirect REQUIRED)
```

You will then need to add `beman::indirect` to the link libraries of any libraries or
executables that include `beman.indirect` headers.

```cmake
target_link_libraries(yourlib PUBLIC beman::indirect)
```

### Using beman.indirect

To use `beman.indirect` in your C++ project,
include an appropriate `beman.indirect` header from your source code.

```c++
#include <beman/indirect/indirect.hpp>
```

> [!NOTE]
>
> `beman.indirect` headers are to be included with the `beman/indirect/` prefix.
> Altering include search paths to spell the include target another way (e.g.
> `#include <indirect.hpp>`) is unsupported.
