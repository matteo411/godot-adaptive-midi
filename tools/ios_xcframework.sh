#!/bin/sh
# Builds the iOS libraries (device arm64 + simulator) for one target and packs them, and
# godot-cpp's, into the xcframeworks the .gdextension names. macOS with Xcode only.
#   tools/ios_xcframework.sh template_debug|template_release
set -e
T="$1"
BIN=demo/addons/adaptive_midi/bin
scons platform=ios arch=universal ios_simulator=yes target="$T"
scons platform=ios arch=arm64 ios_simulator=no target="$T"
rm -rf "$BIN/libadaptive_midi.ios.$T.xcframework" "$BIN/libgodot-cpp.ios.$T.xcframework"
xcodebuild -create-xcframework -library "$BIN/libadaptive_midi.ios.$T.a" -library "$BIN/libadaptive_midi.ios.$T.simulator.a" \
	-output "$BIN/libadaptive_midi.ios.$T.xcframework"
xcodebuild -create-xcframework -library "godot-cpp/bin/libgodot-cpp.ios.$T.arm64.a" -library "godot-cpp/bin/libgodot-cpp.ios.$T.universal.simulator.a" \
	-output "$BIN/libgodot-cpp.ios.$T.xcframework"
rm -f "$BIN"/libadaptive_midi.ios.$T*.a
