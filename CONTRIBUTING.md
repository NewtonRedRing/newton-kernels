# Contributing

Please open an issue describing the problem before making substantial API changes. Keep each pull request focused and add a regression test for every bug fix. Use C++20, the `.clang-format` style and the `newton` namespace. Public API changes need documentation, examples and CHANGELOG entries.

Every contribution must pass a release and debug build, `ctest`, compiler warnings and (where available) the sanitizer preset. Do not introduce mandatory network downloads or unlicensed copied code. Include benchmark inputs, exact commands and machine details when claiming performance improvements. Contributors confirm they have the right to license their changes under MIT.
