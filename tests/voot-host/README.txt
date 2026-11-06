Retained focused Windows x64 host fixtures
sort: stable-radix helper versus std::stable_sort, 6000 generated lists.
palette: source-derived conversion fragment versus original reference, 2304 cases.
dispatch: emitted MMU-off lookup fragment and original fallback, 200000 cases.
socket: source-matched readiness method exercised against actual loopback UDP.
These fixtures preserve the isolated R&D checks. They are not full guest CPU,
whole-emulator, game-logic, GPU scanout or physical-controller conformance tests.
All fixture inputs are synthetic; no ROM, BIOS, EEPROM or gameplay captures.
Configure this directory separately with CMake, build Release, then run CTest.
