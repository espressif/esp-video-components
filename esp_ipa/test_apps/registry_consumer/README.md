# esp_ipa registry-consumer regression test

This app builds `esp_ipa` under its **namespaced** name (`espressif__esp_ipa`),
the way a component-registry consumer pulls it, rather than by the bare
`esp_ipa` short name that in-tree examples use.

Why it exists: `esp_ipa`'s prebuilt library is wired up in
`esp_ipa/CMakeLists.txt`. A require there that only resolves for the local short
name (e.g. listing `esp_ipa` itself) builds fine in-tree but breaks every
registry consumer at CMake configure with
`Failed to resolve component 'espressif__esp_ipa'`. The regular test apps can't
catch that class of bug because they consume the short name. This app does, and
is built for esp32p4 across the CI IDF matrix.

`override_path` points at the in-repo `esp_ipa` so the current branch's source
is tested while the namespace is preserved.
