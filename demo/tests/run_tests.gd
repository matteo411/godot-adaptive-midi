extends SceneTree
## Headless checks of the binding: `godot --headless --path demo -s res://tests/run_tests.gd`.
## Exits 1 on any failure.

var failures := 0


func check(ok: bool, what: String) -> void:
	print(("PASS " if ok else "FAIL ") + what)
	if not ok:
		failures += 1


func rms_db(frames: PackedVector2Array) -> float:
	var s := 0.0
	for f in frames:
		s += f.x * f.x
	return 10.0 * log(s / maxf(frames.size(), 1) + 1e-20) / log(10.0)


func _init() -> void:
	check(ClassDB.class_exists("AdaptiveMusic"), "the extension's classes are registered")
	var bank := SoundFontBank.new()
	check(bank.load_file("res://test_data/sine.sf2") == OK, "a SoundFont loads")
	check(bank.get_preset_count() == 2, "it has its 2 presets")
	var song := MidiSong.new()
	check(song.load_file("res://test_data/two_sections.mid") == OK, "a MIDI song loads")
	check(is_equal_approx(song.get_bpm(), 120.0), "its tempo is read")
	song.set_section("a", 0, 8, "")
	song.set_section("b", 8, 16, "a")
	song.set_layer("low", PackedInt32Array([0]))
	song.set_layer("high", PackedInt32Array([1]))
	check(song.get_section_names() == PackedStringArray(["a", "b"]), "sections are set")
	var sting := MidiSong.new()
	sting.load_file("res://test_data/sting.mid")

	var stream := AdaptiveMusic.new()
	stream.bank = bank
	stream.song = song
	stream.stings = [sting]
	stream.start_section = "a"
	var pb := stream.instantiate_playback() as AdaptiveMusicPlayback
	check(pb != null, "the stream makes an AdaptiveMusicPlayback")
	pb.start(0.0)
	var mix_rate := AudioServer.get_mix_rate()
	var frames := pb.mix_audio(1.0, int(mix_rate))
	check(frames.size() == int(mix_rate), "1 s mixes")
	check(rms_db(frames) > -40.0, "and has sound (%.1f dB)" % rms_db(frames))
	check(absf(pb.get_beat() - 2.0) < 0.05, "two beats played (%.3f)" % pb.get_beat())
	check(pb.get_section() == "a", "in section a")

	pb.set_layer_gain("low", -80.0)
	frames = pb.mix_audio(1.0, int(mix_rate * 0.5))
	check(rms_db(frames.slice(int(mix_rate * 0.1))) < -100.0, "the low layer muted is silent")
	pb.set_layer_gain("low", 0.0)

	pb.queue_section("b", AdaptiveMusic.QUANTIZE_BAR)
	pb.mix_audio(1.0, int(mix_rate * 0.4))  # beat 3.8
	check(pb.get_section() == "a", "a queued section waits for the bar line")
	pb.mix_audio(1.0, int(mix_rate * 0.2))
	check(pb.get_section() == "b", "and starts on it")

	pb.set_tempo_scale(1.5)
	var before := pb.get_beat()
	pb.mix_audio(1.0, int(mix_rate))
	check(absf(pb.get_beat() - before - 3.0) < 0.05, "tempo x1.5: 3 beats a second")
	check(is_equal_approx(pb.get_bpm(), 180.0), "the bpm reads 180")

	pb.play_sting(0, 12, AdaptiveMusic.QUANTIZE_BEAT, -10.0)
	pb.mix_audio(1.0, int(mix_rate * 0.4))
	check(pb.is_sting_playing(), "a sting plays on the next beat")
	pb.stop()
	check(not pb.is_playing(), "stop")
	print("%d failure(s)" % failures)
	quit(1 if failures else 0)
