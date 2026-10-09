# Vendored JSON dependency

- Project: JSON for Modern C++ by Niels Lohmann and contributors
- Version: v3.11.3
- Upstream: https://github.com/nlohmann/json
- Header source: https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp
- License: MIT, preserved in `nlohmann/LICENSE.MIT` and the header
- No local modifications to the upstream files
- Compiled with `JSON_NOEXCEPTION`; all parsing uses `allow_exceptions=false`, and typed reads follow explicit type checks

This dependency is used only by the independent ScriptMotion JSON parser. It is included privately and does not appear in the public API.

SHA-256 (unmodified upstream downloads):

```text
9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6  nlohmann/json.hpp
86b998c792894ccb911a1cb7994f7a9652894e7a094c0b5e45be2f553f45cf14  nlohmann/LICENSE.MIT
```
