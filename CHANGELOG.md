# Changelog

## 1.8.12 — Smaller Aeterna, calmer loops, type, Send knobs

- Aeterna is a step smaller on the docked roll and in the full-screen roll, so she sits on the note instead of covering the lane.
- She holds a looping pose (ballet, dance, walk, spin) for a few seconds before changing. Small pitch moves no longer cut the loop. A leap still does.
- Text across the window is one point larger.
- Deep-edit Send knobs use the same row height and the same knob size as Filter, Envelope, and Vibrato.

## 1.8.11 — Wider keys, Part B beside the playlist, record arm

- The player piano roll shows the whole octave range of the file, at least five octaves, so notes outside C3-C6 are visible again.
- The playlist only arms Part A. Part B is a second MIDI you turn on so both play together (32 channels). Click the Part B card to load it. CLEAR still empties both.
- STUDIO has REC. R next to M and S arms a lane. Live playing is written only onto armed lanes.
- Aeterna stands on a note in the chord, not off to the side. A long note is ballet or a bow. Drums pick the groovy sheets. A lead picks a dance. Lane changes are short.
- Deep edit titles are centered, and the knobs share one size.

## 1.8.10 — Playlist under the cassette, Aeterna glides

- The playlist fills the space under the cassette. The piano roll keeps the full height beside them. PLAY and STOP on that list are gone (the cassette already has them). CLEAR empties the cassette and the playlist.
- Aeterna is smaller on the docked roll. The full-screen size stays close to the one that already looked right. She glides between notes: a small step is ballet, a longer one walks or dashes, a leap jumps or somersaults. She does not teleport.

## 1.8.9 — Aeterna stays on the notes, playlist on PLAYER

- Aeterna stays on the incoming melody, including in the full-screen roll. She no longer launches off the keyboard or hops to a new note every instant. A real pitch change is a slide or a jump. Drum hits only nod her.
- PLAYLIST is part of the PLAYER tab: cassette on the left, piano roll on the right, the two decks and the queue along the bottom. One dropped .mid still loads a tape. Several arm the playlist.

## 1.8.8 — Windows compile

- Note velocity is a byte. The piano-roll score casts it before clamping, so 1.8.7 builds on MSVC.

## 1.8.7 — Pause keeps the keys, Aeterna On / Off

- Pause keeps the piano-roll keyboard as well as the notes. The keys stay on the busy four octaves, so a drum hit cannot squash them, and they do not jump when you pause. Stop still returns to the start. Aeterna stays where she was.
- Aeterna is only On or Off. The button reads Aeterna On / Aeterna Off. On, she follows the melody (the highest note that is about to arrive; the bass if the tune is quiet). A wide leap flips, a small one steps or dashes, a long note is ridden down, and a hard hit lands in a trick. Drum hits nod her without pulling her off the line. In a gap she dances in time with the tempo. Still hidden at 0:00 until a playlist song has already played.

## 1.8.6 — Pause, Aeterna on the notes, scale tune, KEYS

- Pause keeps the piano-roll notes on screen. Stop still returns to the start.
- Aeterna stands on a note that is still coming in, not on the keyboard. ON hops between notes. PLAYFUL dances (cheer, dab, ballet, spin, and the other sheets). At 0:00 of a fresh file she stays hidden. After a playlist song has already played she can stay.
- Mixer deep edit is grouped: Filter, Envelope, Vibrato, Send. SCALE is this part's tune, C to B, -64 to +63 cents.
- KEYS, next to the tabs, opens an on-screen piano. Off until you turn it on. The computer keys play the selected part (Z is C, like FL). OPTIONS has a velocity curve (Linear, Soft, Loud, Fixed) for live MIDI and those keys. The cassette is unchanged.

## 1.8.5 — Aeterna on the piano roll

- Aeterna can stand on the PLAYER piano roll (and the popped roll). The AETERNA button cycles off, on, and playful. On: she idles, walks, and hops between notes. Playful: she also dances, cheers, and sits when there is a gap. She does not take clicks. The choice is remembered.

## 1.8.4 — Space plays and pauses

- Space plays and pauses. On PLAYER it follows whatever the cassette is showing (the tape, the playlist, or STUDIO). The popped piano roll does the same. Playlist and Studio tabs use their own transport. The rest of the window does too, unless a text field is focused.

## 1.8.3 — Player roll, mixer list, Options text

- STUDIO playback shows on the PLAYER piano roll the same way a playlist does (Part A or Part B). Cassette PLAY / PAUSE / STOP follow the songwriter while that roll is up.
- Mixer deep edit lists sounds by map and category. Changing the map, the drum filter, or the category does not change the part. The sound changes when you click a row.
- PART A 1-16 and PART B 17-32 sit on the mixer bar with MUTE ALL, not in the tab row.
- OPTIONS text stays readable (no squashed letters, no stray dashes). The standalone audio panel is taller.
- The piano-roll keyboard starts and ends on white keys, and the black keys sit in the gaps. Empty tape included.
- Beat lines on the roll move toward the keyboard with the notes. The note shapes are unchanged.

## 1.8.2 — STUDIO timeline and PATCH browser

- The arrange bar numbers sit on their own ruler. Click that ruler (or drag along it) to move the playhead. GLOBAL no longer shares the row with the numbers.
- The piano roll has a short EDITING title, then its own bar ruler, then the keys. Click that ruler to move the playhead. A scrollbar under the roll moves through the pattern.
- CC lanes and GLOBAL start folded after IMPORT. The fold control is an arrow: down opens the lanes that already have points (or volume, if none do), up closes them. Right-click the arrow to pick one lane.
- Shift+wheel, the horizontal scrollbar, or a middle-drag pans the arrange and the piano roll. Middle-click does not create notes, clips, or automation.
- Alt+wheel on a selected piano-roll note changes its velocity. With nothing selected, Alt+wheel still changes key height.
- PATCH lists factory sounds by map and category (Classical, Contemporary, Solo, Enhanced, Special 1, Special 2, User). Filter narrows the list. INFO explains layers, the filter, chorus, reverb, SEND, SAVE, and STORE.

## 1.8.1 — PATCH screen

- Each layer is a labeled slider: loudness, tune, pan, which key strengths play it, and whether it goes through the effect or straight out.
- The line under the toolbar says what the control under the pointer will do.
- FILTER / VOLUME instead of TVF / TVA. The curve is labeled start, attack, decay, sustain, release. Cutoff, resonance, and envelope amount are sliders.
- Chorus and reverb say what more of them does. The 144 slots sit in their own row: click, STORE, double-click to load.

## 1.8.0 — PATCH editor

- New **PATCH** tab (not STUDIO, PLAYER, or PLAYLIST). Four PCM tones: on/off, level, coarse/fine, pan, velocity window, MFX or dry.
- Drag the TVF or TVA envelope points. Filter type and LFO wave sit under the plot.
- **SEND** (SD-80 and SD-90 only) loads the chosen factory base sound, then writes the part's temporary patch with Roland DT1 (model `00 48`, 7-bit checksum). Native mode. It does not write the module's user flash — the panel Write Patch only stores MFX.
- **SAVE / LOAD** `.mesd80patch` and a 144-slot local librarian work on SD-20 as well as SD-80 / SD-90. SD-20 has no user memory: SEND stays off, LIVE sends part CC offsets (64 = no change) and tone levels. Tones 3 and 4 stay in the file.
- **SYX** exports the DT1 dump. STORE / double-click recalls a local slot. The patch and the librarian come back with the DAW session.

## 1.7.1 — STUDIO editor

- Automation is point curves (click to add, drag a point, right-click to delete). [+] opens a CC picker: cutoff, reso, vol, pan, rev, cho, MFX. No painting.
- Double-click a clip: piano roll header says EDITING Pxx on A05, clip gets an EDIT badge.
- Drag clip edges to resize (FL-style). Body still moves. Longer than the pattern loops.
- Piano roll only accepts notes inside the pattern. Drag the end marker to change length. -/+ zoom, Ctrl+wheel zoom, Alt+wheel key size, drag the splitter to grow the roll.
- Track headers have M / S (same mute/solo as the mixer).
- GLOBAL row at the top: Tempo, Master Vol, Rev Time, Cho Rate, Cho Depth.
- PLAY pushes mixer bank/PC/mix for used parts so A05 Fat Square actually sounds like Fat Square.
- IMPORT splits MIDI per channel onto A01-B16, applies mixer patches (SYNC), and loads CC / tempo / master-vol into lanes.
- Adjacent clips use different pattern colours so P1-P5 are easy to tell apart.

## 1.7.0 — STUDIO songwriter

- New **STUDIO** tab (separate from PLAYER cassette and PLAYLIST ping-pong): 32 lanes bound 1:1 to mixer / SD-80 parts A01-A16 and B01-B16.
- FL-style piano roll with velocity graph. Shift-click a note for slide; playback turns that into a pitch-bend stream (range 12).
- Ableton-style unfoldable CC lanes (cutoff 74, reso 71, vol 7, pan 10, rev 91, cho 93, MFX 94). CC stream is thinned to about 50 Hz per channel.
- PLAY / STOP, BPM, SONG / PATTERN. Double-click a clip to edit that pattern on that part. Click a piano key to preview on the SD-80.
- Save / load `.mesd80song`. Import `.mid` onto the selected lane. Export writes Part A `.mid` and `-B.mid` if Part B has clips (bank/PC stamped from the mixer). Last song is restored with skins and mixer state.

## 1.6.6 — USB throttle off

- USB throttle defaults to **0 ms** (off). Resonance, bank/PC and the rest of the mixer CCs go out as they happen, same idea as FL MIDI Out and Roland's own editor.
- OPTIONS still has 10 / 20 / 30 / 40 / 50 ms if a dump ever drops. Existing sessions that were stuck on the old 30 ms default are moved to off once.
- At 0 ms the queue drains in a burst instead of one message per gap, so SYNC and playlist setup no longer crawl.

## 1.6.5 — Falling piano-roll

- PLAYER piano-roll is a falling highway: notes come toward a keyboard at the bottom (same idea as that SD-20 demo, coloured bars instead of gem shapes).
- Sounding notes get a Guitar Hero glow on the key. POP OUT opens a detached window; FULL goes fullscreen. Minimize or close docks it back under the cassette.

## 1.6.4 — Player follows playlist

- Playlist files appear on the PLAYER cassette and piano-roll. PLAY / PAUSE / STOP on the cassette drive the playlist until you load a tape, which disarms it.
- LOOP while armed: whole playlist, then this song, then off. Send setup is locked while a playlist is armed (setup is pushed when each file loads onto a deck).
- Piano-roll follows Part B when that deck is playing (legend B1-B16).

## 1.6.3 — Standalone quit panic

- Closing the standalone (title-bar X or Alt+F4) stops the cassette and playlist, then sends all-notes-off / all-sound-off / reset-controllers on channels 1–16 to both USB ports, waits a beat so the 2002 module can eat it, and only then exits. No dialog.

## 1.6.2 — PLAYLIST decks

- PLAYLIST tab: Part A / Part B are deck cards with a status pill, elapsed time and a live progress bar. Queue rows mark the next file. Same ping-pong engine.

- PLAYLIST tab: Part A / Part B are deck cards with a status pill, elapsed time and a live progress bar. Queue rows mark the next file. Same ping-pong engine.

## 1.6.1 — MSVC: playlist ADD chooser

- JUCE 9.0.1 has `FileBrowserComponent::canSelectMultipleItems`, not `canSelectMultipleFiles` (C2039). Drag-and-drop onto PLAYLIST already accepted several files.

## 1.6.0 — FL piano-roll channels, PLAYLIST, no DEMOS

- Live MIDI in a DAW defaults to **Part A as-played** so an FL pattern (channel colours = MIDI channels) hits A1-A16 like the cassette. Follow SEL remains for a keyboard. Mixer drop also sets this. Player piano-roll colours are display-only and never reroute notes.
- **PLAYLIST** tab: drop several `.mid` files. First arms Part A, second arms Part B. PLAY starts A; when A ends B plays and the next file is patched onto A, then they swap until the queue is empty.
- Removed the DEMOS tab (it did not start songs on hardware).

## 1.5.5 — Docs: drop FL Studio ports 8 & 9

- Removed the v1 FL Studio “ports 8 & 9” routing section from the README. Part A/B USB in OPTIONS is the current setup.

## 1.5.4 — MSVC piano-roll compile + one log

- Extra closing brace in the piano-roll timer (`else` after the function already closed) was C2059 on MSVC.
- **`build.bat`** is the only Windows compile script. One log: `logs\build.log`. No `01 Compile.bat`, `run.bat`, `build.log` copies, or `BUILD_STATUS.txt`.

## 1.5.3 — Cassette-reel icon

- App icon is a graphite cassette with amber + teal reels (`Assets/icon.png`). CMake `ICON_BIG` / `ICON_SMALL` bake it into the Windows `.exe` / `.vst3` and the macOS bundle. The standalone also calls `setIcon` so Linux and the window peer match.
- Replaces the default JUCE / Windows application icon.

## 1.5.2 — Piano-roll polish

- Empty bay is empty (no fake preview notes). Loaded SMF is drawn in full; the playhead travels with time.
- Cassette and piano-roll frames share the same inset so they line up.
- 16 discrete channel colours (no A3/A8 collision). Sounding notes and piano keys light together, interpolated at 60 Hz.
- ASCII-only captions (the middle-dot had become `Ã·` on MSVC).

## 1.5.1 — Windows one-click build log

- **`build.bat`** is the one-click Windows compile (same idea as a full tool-check + always-on log). It writes `logs\build-*.log`, copies the latest to `build.log` and `build_log.txt`, and drops `BUILD_STATUS.txt`. On failure the window stays open and tells you to paste `build.log`. `01 Compile.bat` and `run.bat` call it.

## 1.5.0 — PLAYER piano-roll

- **Colourised MIDI display** fills the space under the cassette. Each SMF channel (Part A 1–16) has its own colour; mixer mute/solo dims that colour. A slow preview pattern plays when no tape is loaded so the bay is never an empty hole.

## 1.4.1 — MSVC JUCE 9 Song Select

- JUCE 9.0.1 has no `MidiMessage::songSelect` (C2039). DEMOS now send raw MIDI `F3 nn`.

## 1.4.0 — JUCE 9.0.1, SEL keyboard, demos, Multi FX warning

### Bugs

- **Keyboard ignored SEL.** Live MIDI default is **Follow SEL** again (param 0). Click SEL, play; Shift+SEL fans out. OPTIONS still has Part A / Part B as-played for 16-channel piano rolls. Sessions that stored the old default (0) now Follow SEL.
- **Demo buttons did nothing.** They sat behind the USB throttle and used one guessed SysEx address. They now send **immediately** on Part A and Part B: Native On, two DT1 demo addresses, Song Select, MIDI Start, MMC Play. Stop sends MIDI Stop + MMC Stop + All Notes Off. If USB ports are closed the footer says so. The panel DEMO key remains the documented Owner's Manual path.
- **Multi FX red warning** sits in the Multi FX third of the FX bar, not next to MUTE ALL.

### Stack

- Fetches **JUCE 9.0.1** (was 8.0.8). Delete the `build` folder once so CMake re-downloads. CLAP is still clap-juce-extensions — 9.0.1 has no native CLAP client.

## 1.3.1 — MSVC standalone MIDI out

- Standalone called `AudioDeviceManager::setMidiOutputDeviceEnabled`, which does not exist in JUCE 8.0.8. MIDI output is a single default: `setDefaultMidiOutputDevice({})` (none). Keyboards still use `setMidiInputDeviceEnabled`. Part A/B USB stay on the processor.

## 1.3.0 — Hardware session, OPTIONS audio, DEMOS

Standalone audio/MIDI, mixer lock, host MIDI routing, and a DEMOS tab. Full GitHub-ready notes also live in `github/CHANGELOG.md`.

### Bugs

- OPTIONS audio/MIDI is the real JUCE device selector in standalone (driver, ASIO, sample rate, buffer, MIDI keyboard inputs). VST still greys it out with “Audio Settings Controlled by Host”. The redundant “Standalone: choose Driver…” footnote is gone.
- The selector is created on a timer if the standalone holder is not ready at editor construction, reserved ~460 px, and lives in a scrolling OPTIONS page so it cannot overlap USB/mode/skin.
- Standalone MIDI keyboards work again: MIDI Input is shown in the selector, non-SD-80 inputs are auto-enabled, SD-80 USB outs stay on Part A / Part B (not as host MIDI outs).
- PLAYER Loop is a LOOP tape button in the cassette row (PLAY / PAUSE / STOP / LOOP / LOAD / Send setup), not a stray checkbox.
- Reloading no longer flattens the mixer while the hardware still has sounds. Session is persisted; launch restores it, then RQ1-dumps documented Native addresses (MFX types, part map, output assign). **Pull from SD-80** repeats the dump. Bank/program are not a published RQ1 — those come from the last session. Launch does **not** push defaults onto the module.
- Right-click a loaded mixer instrument name (or a patch list row) to lock bank+program so it survives import, presets and dumps.
- Host MIDI in a DAW defaults to **Part A as-played** (channel 1→A1 … 16→A16). OPTIONS → Host MIDI destination: Part A / Part B / Follow SEL.

### Features

- **DEMOS** tab between PLAYER and OPTIONS. Demo 1 / 2 / 3 / Stop send Native SysEx `00 00 01 00`. The panel DEMO key is the documented path; some firmware ignores SysEx — the tab says so.
- OPTIONS **Module volume** sends Universal SysEx Master Volume. The physical knob still works; it cannot be locked out.
- PLAYER footer warns that massive multi-parameter changes while MIDI is playing can spike volume on this 2002 USB module.
- Multi FX defaults **OFF**. Hover warning on A/B/C; red line on the mixer. Tweaking Multi FX can flood the USB queue.
- GitHub disclaimer: tested only on **Windows 11**, only **standalone and VST**. AU/CLAP/other OS untested.

## 1.2.1 — MSVC + CLAP


- MSVC C3318: FX column layout used `auto mfxCols[3]` which Visual C++ rejects. Now `juce::Rectangle<int> mfxCols[3]`.
- **CLAP** via unofficial [clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions) (JUCE 8.0.8 has no native CLAP; that is JUCE 9). Target `ModernEdirolSD80_CLAP`. Disable with `-DMESD80_CLAP=OFF`.
- **AU** stays in CMake `FORMATS` but only exists on macOS / Xcode. Windows cannot build Audio Units.
- Windows compile script is now `01 Compile.bat` (writes `build_log.txt`). `run.bat` calls it.

## 1.2.0 — Mixer, FX, standalone chrome

Hardware-test bugs from v1.1 plus mixer / options / player features. Full GitHub-ready notes also live in `github/CHANGELOG.md`.

### Bugs

- Mute / Solo while the cassette plays now work even if volume CC was being ignored: notes for silenced parts are dropped in `processBlock`. They still need **Part A USB / Part B USB** open — the plugin auto-selects EDIROL / SD-80 ports when it sees them.
- USB combo labels read **Part A USB: (none)** / **Part B USB: (none)** instead of a bare `(none)`.
- System FX + Multi FX bar is five fixed columns (Reverb | Chorus | MFX A | MFX B | MFX C). Knobs sit under the control they belong to. A/B/C on-toggles stay 28 px and no longer stretch.
- Each Multi FX has four parameter knobs (P1–P4) sent as Native COMMON SysEx. THROUGH buttons display as **Multi FX A / B / C**.
- Global vs per-part FX: GM2 reverb/chorus are system-wide; Native MFX A/B/C are insert FX (part output assign MFX); Deep Edit has per-part reverb / chorus / MFX send + MFX select + output assign.
- Mixer MIDI import resets **unlocked** global FX and unused-part sends, then syncs hardware. Locked parameters stay.
- Cassette reels both rotate the same direction at 60 Hz.
- Standalone no longer shows JUCE’s top-left Options or top-right Settings. Audio I/O lives in the plugin OPTIONS tab (`AudioDeviceSelectorComponent`).
- Donate line is a bold boxed button.
- Extra-host-MIDI-out footnote removed.
- USB / mode / throttle line moved off the header into OPTIONS.

### Features

- MUTE ALL / UNMUTE ALL / UNSOLO ALL. Shift+mute = mute or unmute all; Shift+solo = unsolo all.
- OPTIONS → SHORTCUTS.
- Shift+SEL multi-select; live MIDI fans out to every selected part.
- PLAYER Loop tape checkbox.
- Skin **As God Intended** (SD-80 silver chassis).
- `github/` folder: README, CHANGELOG, BUILD, HARDWARE, LICENSE for a GitHub homepage.
- VST OPTIONS: greyed “Audio Settings Controlled by Host” mock. Standalone: real device selector.
- Emergency Hardware Reset (sure-checkbox). Reset effects + sync (sure + don’t-show-again).

## 1.1.1 — MSVC compile

- Lockable buttons inherit JUCE constructors (MSVC could not construct `ToggleButton`/`TextButton` from a label).
- SEL routing no longer calls `MidiMessage::withChannel` (not in JUCE 8.0.8).
- Patch category compare uses `juce::String` so MSVC is not ambiguous.
- GS "808 Tom" bank was 808 (uint8 overflow) — corrected to PC 118 / LSB 1.

## 1.1.0 — Crimson Redstone

Bugfixes from the v1 hardware test:

- Part A / Part B labels use ASCII `1-16` / `17-32` so they no longer render as tofu.
- Deep Edit is a right-hand sidebar; knobs stay inside the panel.
- Patch picking is a category list + patch list (searchable), not one overloaded combo.
- MFX types open as grouped submenus (Drive, Delay, Amp / Multi, …).
- **SEL** rewrites live MIDI (notes, CCs, pitch, aftertouch) onto the selected part’s channel and USB port.
- Mute / Solo are real toggles with red / green on-colours; mute zeros volume, any solo silences the rest.
- MFX actually hits the sound: parts default to output assign MFX, COMMON source is set, Native is required.
- Standalone enables **ASIO** (SDK fetched at configure) alongside WASAPI.
- Dual SD-80 ports are **Part A USB / Part B USB**.

Extra features:

- Donation link to https://crimsonredstone.bandcamp.com/
- Credits: Crimson Redstone
- Eight persistable skins (Studio, Scarlett/Flandre, Baguette/Teto, Leek/Miku, Hakurei, Lunatic, Sakura, Matcha)
- PLAYER tab cassette deck
- Right-click lock on every adjuster
