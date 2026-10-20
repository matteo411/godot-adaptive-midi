#!/usr/bin/env python
# godot-adaptive-midi: the GDExtension (default target), plus the core's tests and
# render CLI (`scons tests`, `scons cli`), which need no Godot.
import os

env = SConscript("godot-cpp/SConstruct")
env.Append(CPPPATH=["src", "thirdparty"])

core_sources = ["src/core/song.cpp", "src/core/bank.cpp", "src/core/engine.cpp"]

if "tests" in COMMAND_LINE_TARGETS or "cli" in COMMAND_LINE_TARGETS:
    host = Environment(CXXFLAGS=["-std=c++17", "-O2", "-g", "-Wall"], CPPPATH=["src", "thirdparty", "tests"])
    core = [host.Object("build/host/" + os.path.basename(s)[:-4], s) for s in core_sources]
    tests = host.Program("bin/amidi-tests", core + ["tests/test_engine.cpp"], LIBS=["m"])
    cli = host.Program("bin/amidi-render", core + ["src/cli/render.cpp"], LIBS=["m"])
    fixtures = host.Program("bin/amidi-fixtures", ["tests/make_fixtures.cpp"], LIBS=["m"])
    Alias("tests", [tests, fixtures])
    Alias("cli", cli)
else:
    sources = core_sources + Glob("src/godot/*.cpp")
    suffix = env["suffix"].replace(".dev", "").replace(".universal", "")
    if env["platform"] == "macos":
        library = env.SharedLibrary(
            "demo/addons/adaptive_midi/bin/libadaptive_midi.{}.{}.framework/libadaptive_midi.{}.{}".format(
                env["platform"], env["target"], env["platform"], env["target"]), source=sources)
    elif env["platform"] == "ios":  # static; tools/ios_xcframework.sh packs the device and simulator builds
        library = env.StaticLibrary("demo/addons/adaptive_midi/bin/libadaptive_midi.ios.{}{}.a".format(
            env["target"], ".simulator" if env["ios_simulator"] else ""), source=sources)
    else:
        library = env.SharedLibrary("demo/addons/adaptive_midi/bin/libadaptive_midi{}{}".format(suffix, env["SHLIBSUFFIX"]), source=sources)
    Default(library)
