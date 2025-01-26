# TODOs

## feature parity with value_types repo

- [x] change file names according to beman
- [ ] add compile tests
- [ ] add licenses everywhere
- [ ] check all TODO comments
- [ ] check install differences
- [ ] include coverage in cmake
- [ ] double check the custom xyz_add_library/test scripts
- [ ] check submodules in `./cmake/` dir
- [ ] check sanitizers
- [ ] `add_subdirectory(benchmarks)`
- [ ] `add_subdirectory(compile_checks)`
- [ ] check what the `configure_package_config_file` does
- [ ] move compile checks to new dir
- [ ] update README.md
  - [ ] add beman README status (unstable)
- [ ] check release-please
- [ ] ci integration + tests
- [ ] coordinate with original implementers on how to best cut over
- [ ] formatting of all files
- [ ] maybe: enable bazel support (original repository supports bazel)
- [ ] maybe: compiler support for cpp14 and cpp17 (original repository supported 14 and 17)

## Questions

- Should we provide on combined library, or split them up? Original repository has three different
  libs:
  - indirect
  - polymorphic
  - value_types (both indirect and polymorphic)
