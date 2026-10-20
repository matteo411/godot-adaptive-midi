# godot-adaptive-midi

Adaptive music for Godot 4.5+, played live from MIDI songs through a SoundFont, on
desktop and mobile. A GDExtension in C++: no pre-rendered stems, no `AudioStreamGenerator`.

- **Layers:** name groups of MIDI channels and fade each in or out (`set_layer_gain`).
- **Tempo:** speed a song up or down while it plays, without changing its pitch (`set_tempo_scale`).
- **Sections:** name beat ranges and jump between them on the next beat, bar or section end (`queue_section`).
- **Stings:** short songs played over the music, transposed to its key and started on its beat, with the music ducked under them (`play_sting`).
- **Small:** a song is a few KB of MIDI; one `.sf2` bank serves every song.

The engine renders on Godot's audio thread (`AudioStreamPlaybackResampled._mix_resampled`) with
no allocation or locks; the controls reach it through a lock-free queue.

## Use

```gdscript
var bank := SoundFontBank.new()
bank.load_file("res://music/bank.sf2")
var song := MidiSong.new()
song.load_file("res://music/rally.mid")
song.set_section("verse", 0, 32)           # beats
song.set_section("fill", 32, 36, "verse")  # then back to the verse
song.set_layer("drums", PackedInt32Array([9]))

var stream := AdaptiveMusic.new()
stream.bank = bank
stream.song = song
$Player.stream = stream
$Player.play()
var music := $Player.get_stream_playback() as AdaptiveMusicPlayback
music.set_layer_gain("drums", -80.0, 2.0)                   # fade the drums out over 2 s
music.set_tempo_scale(1.05, 4.0)                            # 5% faster over 4 s
music.queue_section("fill", AdaptiveMusic.QUANTIZE_BAR)     # at the next bar line
```

## Build

```bash
git submodule update --init
pip install scons
scons platform=linux target=template_debug    # or android / ios / macos; template_release
scons tests && bin/amidi-tests                  # the engine's tests (no Godot needed)
scons cli                                       # bin/amidi-render: render a song to WAV
```
The binaries land in `demo/addons/adaptive_midi/bin/`. The binding's tests run in the demo project:
`godot --headless --path demo --import` then `godot --headless --path demo -s res://tests/run_tests.gd`.

**Known issue (Godot 4.6):** the first headless `--import` of a project with any GDExtension crashes
on exit (signal 11) after finishing its work; godot-cpp's own test project does the same. Run it
again: the second import, and everything after it, is clean.

## Licences

MIT (`LICENSE`). Bundled: [TinySoundFont](https://github.com/schellingb/TinySoundFont) (`tsf.h`, MIT,
Bernhard Schelling, based on SFZero by Steve Folta), [godot-cpp](https://github.com/godotengine/godot-cpp) (MIT),
[doctest](https://github.com/doctest/doctest) (MIT, tests only).
