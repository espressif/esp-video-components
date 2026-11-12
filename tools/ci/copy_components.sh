#!/bin/bash

TARGET_COMPONENT_PATH=esp_video/examples/common_components/example_video_common

# 1. Get the list of modified examples

EXAMPLE_PATHS=$( find esp_video/examples/ -type f -name CMakeLists.txt | grep -v "/components/" | grep -v "/common_components/" |  grep -v "/managed_components/"  | grep -v "/main/" | sort )

echo EXAMPLE_PATHS: $EXAMPLE_PATHS

# 2. For each example, copy the specific components to the example/components directory

for EXAMPLE_PATH in $EXAMPLE_PATHS; do
    DIR=$(dirname $EXAMPLE_PATH)
    mkdir -p $DIR/components
    cp -r $TARGET_COMPONENT_PATH $DIR/components/
done

# 3. Remove the existing configuration files in the example YML file

for EXAMPLE_PATH in $EXAMPLE_PATHS; do
    DIR=$(dirname $EXAMPLE_PATH)
    sed -i '/example_video_common/d' $DIR/main/idf_component.yml
done

# 4. Copy fast_startup_common to the fast_startup examples that use it

FAST_STARTUP_COMPONENT_PATH=esp_video/examples/fast_startup/common_components/fast_startup_common

for DIR in esp_video/examples/fast_startup/*/; do
    [ -f $DIR/main/idf_component.yml ] || continue
    grep -q fast_startup_common $DIR/main/idf_component.yml || continue
    mkdir -p $DIR/components
    cp -r $FAST_STARTUP_COMPONENT_PATH $DIR/components/
    sed -i '/fast_startup_common/d' $DIR/main/idf_component.yml
done
