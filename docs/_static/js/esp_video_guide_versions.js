var DOCUMENTATION_VERSIONS = {
    DEFAULTS: { has_targets: false,
                supported_targets: [ "esp32p4" ]
              },
    VERSIONS: [
        // latest
        { name: "latest", has_targets: true, supported_targets: [ "esp32p4" ] },

        // v1.0.0.0
        { name: "release-v1.0.0.0", has_targets: true, supported_targets: [ "esp32p4" ] },
    ],
    IDF_TARGETS: [
       { text: "ESP32-P4", value: "esp32p4"},
    ]
};
