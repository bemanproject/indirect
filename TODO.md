# TODOs

## feature parity with value_types repo

- [ ] compiler support for cpp14 and cpp17
- [ ] change file names according to beman
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
- [ ] enable bazel support
- [ ] move compile checks to new dir
- [ ] update README.md
  - [ ] add beman README status (unstable)
- [ ] check release-please
- [ ] ci integration + tests
- [ ] coordinate with original implementers on how to best cut over

## Questions

- currently there are three different libs:
  - indirect
  - polymorphic
  - value_types (both indirect and polymorphic)
