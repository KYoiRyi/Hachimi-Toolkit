#!/bin/bash
set -e

# Setup Android NDK path
if [ -z "$ANDROID_NDK_HOME" ]; then
    echo "ANDROID_NDK_HOME is not set."
    exit 1
fi

PLUGIN_DIR="plugins"
BUILD_OUTPUT_DIR="build/plugins"

mkdir -p $BUILD_OUTPUT_DIR

for plugin in $PLUGIN_DIR/*; do
    if [ -d "$plugin" ] && [ -f "$plugin/CMakeLists.txt" ]; then
        plugin_name=$(basename $plugin)
        echo "Building plugin: $plugin_name"
        
        rm -rf "$plugin/build"
        mkdir -p "$plugin/build"
        pushd "$plugin/build"
        
        cmake .. \
            -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
            -DANDROID_ABI=arm64-v8a \
            -DANDROID_PLATFORM=android-24 \
            -DCMAKE_BUILD_TYPE=Release
            
        make -j$(nproc)
        
        popd
        
        # Copy output library
        find "$plugin/build" -name "*.so" -exec cp {} "$BUILD_OUTPUT_DIR/" \;
    fi
done

echo "All plugins built successfully."
