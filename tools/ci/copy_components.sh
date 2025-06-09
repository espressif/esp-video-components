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
