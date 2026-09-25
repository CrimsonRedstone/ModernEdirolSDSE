# The SD-80 is old. Treat it that way.

The Edirol / Roland Studio Canvas SD-80 is a **2002 USB 1.1** sound module. It has two MIDI ports, a small buffer, and it will drop SysEx if you dump a full 32-part scene in one gulp. This plugin exists because a DAW piano roll cannot honestly drive 32 parts + Native MFX on that box without help.

**Tested only on Windows 11, and only as standalone + VST.** AU, CLAP, macOS and Linux are in the CMake tree. They have not been hardware-tested.

## Warnings you should actually read

- **Volume spikes.** Changing many parameters at once while MIDI is playing (live, host, or cassette) can jump the output. The PLAYER tab repeats this in small type. Don’t dump a 32-part scene mid-take.
- **Multi FX USB queue.** Each Multi FX A/B/C tweak sends a large Native COMMON SysEx block. That can stall USB for a long time. Multi FX defaults **OFF**. System reverb and chorus are fine. Hover A/B/C for the same warning. Leave them off unless you need them.
- **Physical volume knob.** OPTIONS → Module volume sends Universal SysEx Master Volume. It does **not** lock or bypass the hardware knob — both still work.

## First response to “it isn’t doing what the mixer says”


1. **SYNC HARDWARE.** Always. The header button pushes mode, FX, every part’s bank/PC/mix/tone and MFX routing. USB throttle is **0 ms** by default so that dump is a burst; OPTIONS can add 10–50 ms. Watch **USB queue** go to 0.
2. **OPTIONS → Part A USB / Part B USB.** Mute, solo, SEL, Deep Edit and the cassette all go out these two sockets. If they say **Part A USB: (none)** the plugin is moving faders in memory and the module never hears it. The plugin tries to auto-pick names containing EDIROL, SD-80 or Studio Canvas. Windows often lists them as `EDIROL SD-80` and `MIDIOUT2 (EDIROL SD-80)`.
3. Leave the hardware in **USB mode**. The rear-panel switch is not decorative.
4. Do not also route the same ports from the DAW’s own MIDI output unless *Host MIDI mirrors Part A/B* is what you want. Double-driving the module is how you get stuck notes and bank fights.
5. If a take is already rolling, do **not** hit Emergency Hardware Reset. That dump can stall MIDI for a few seconds.

## Mixer vs hardware on launch

Reload used to show a flat mixer while the module still had the previous sounds. Two things now happen instead:

1. The last session is restored (standalone `ApplicationProperties` + host plugin state).
2. The plugin **RQ1-dumps** documented Native addresses (MFX types, part map, output assign) and overlays unlocked parameters. Launch does **not** push mixer defaults onto the module.

Bank/program are MIDI CC + program change. The Owner’s Manual does not publish an RQ1 for the sounding patch, so those come from the last session. OPTIONS → **Pull from SD-80** repeats the dump. **SYNC HARDWARE** is the other direction: mixer → module.

## What Emergency Hardware Reset actually does

OPTIONS → **Emergency Hardware Reset** (sure-checkbox required):

- All Notes Off + All Sound Off + CC 121 on every channel of Part A **and** Part B
- Native On, then GS Reset, then Native On again
- Plugin session restored to factory defaults (patches, mix, FX, locks, skin)

It does **not**:

- Rewrite firmware
- Erase User patches stored on the module / memory card
- Change your audio interface

## FX: global vs per-part

From the Owner’s Manual:

| | What | Where |
|---|---|---|
| System reverb | GM2 reverb type + time | Mixer FX bar (Reverb column) |
| System chorus | GM2 chorus type + rate/depth/feedback | Mixer FX bar (Chorus column) |
| Multi FX A/B/C | Native insert, 90 algorithms + THROUGH | Mixer FX bar, COMMON source |
| Per-part reverb send | CC 91 | Deep Edit **Reverb** |
| Per-part chorus send | CC 93 | Deep Edit **Chorus** |
| Per-part MFX send | CC 94 + SysEx | Deep Edit **MFX Send** |
| Which MFX a part hits | output assign MFX + MFX select A/B/C | Deep Edit **Insert MFX** / **Out: MFX** |

Parts default to **Out: MFX** so insertion FX actually bite. Multi FX A/B/C default **OFF** (THROUGH) because enabling them floods the USB queue. THROUGH on Multi FX A/B/C means that slot is a bypass — pick an algorithm (Overdrive, Delay, …) **and** turn the slot on for P1–P4 to matter.

## Throttle

OPTIONS → **0 ms** (off, default) / 10 / 20 / 30 / 40 / 50 ms. Off sends mixer CCs as they happen. Raise it only if a dump drops on a flaky hub. Use a powered hub if you can; 2002 USB devices hate bus power plus a 32-part dump.

## Cassette PLAYER vs mixer drop

- Drop a `.mid` on the **mixer** → bank/PC/mix auto-assign, unlocked FX reset, then sync.
- Drop a `.mid` on the **PLAYER** cassette → it plays. It does **not** rewrite patches unless you press **Send setup to SD-80**.
- Mute/solo on the mixer silence cassette channels. That only reaches the module through Part A USB.

## STUDIO vs PLAYER / PLAYLIST

**STUDIO** writes new songs (and edits imported MIDI). **PLAYER** and **PLAYLIST** play files you already have. They stay separate tabs on purpose.

- STUDIO tracks 1–16 = Part A USB channels 1–16. Tracks 17–32 = Part B USB channels 1–16. Same 1:1 as the mixer.
- PLAY on STUDIO stops the cassette and the playlist so they cannot double-drive the module. PLAY also stamps mixer bank/PC/mix for every part that has clips, so A05 Fat Square sounds like Fat Square without a manual SYNC.
- Slide notes are not an SD-80 feature. STUDIO sets pitch-bend range 12 and streams pitch-bend while a slide is sounding.
- Automation is point curves (click to add, drag a point, right-click to delete), not freehand paint. [+] on a lane opens a CC picker (cutoff 74, reso 71, vol 7, pan 10, rev 91, cho 93, MFX 94). Streams are thinned to about 50 Hz per channel so USB 1.1 does not choke.
- Double-click a clip to edit it — the roll header says which pattern, the clip gets an EDIT badge. Drag clip edges to resize (body still moves). Notes stay inside the pattern; -/+ zoom, Ctrl+wheel, Alt+wheel for key size.
- Track headers have M / S (same mute/solo as the mixer). A foldable GLOBAL row holds tempo, master volume, reverb time, chorus rate and chorus depth. Adjacent clips use different pattern colours.
- Import splits a `.mid` per MIDI channel onto A01–B16, applies mixer patches (SYNC), and loads CC / tempo / master-vol into lanes. Export writes a 16-channel SMF for Part A. If Part B has clips, a sibling `-B.mid` is written too.

## PATCH (user tones)

The old SD-80 editor and the front panel are a poor way to stack four PCM tones. PATCH is its own tab.

What the hardware actually stores (Owner’s Manual p.31 and p.50, SD-90 MIDI Implementation model `00 48`):

- Of the panel-editable parameters, **only Native MFX** can be written into the module’s user slots (**Inst U-001–U-128**, **Drum U-001–U-016**, recall MSB **87** / **86**). Chorus, reverb and part parameters are gone at power-off.
- Tone pitch, TVF, TVA and LFO live in the **temporary patch** of a part. A program change or power-off wipes them. The published map has no user-flash write address for that block, so SEND does not invent one.
- Part CCs (cutoff 74, resonance 71, attack 73, decay 75, release 72) are **offsets**: 64 means “leave the sound alone”. Tone levels CC 80–83 are absolute.

SEND (SD-80 and SD-90, same model ID):

1. Stay in **Native**. SEND does not switch mode for you (Native On resets the module).
2. Pick a factory base sound. That patch owns the PCM waves — there is no published raw wave list, so wave group/number are not written.
3. SEND does bank/PC **then** DT1. Program change first, or it erases the temporary patch you just wrote.
4. Temporary patch part 1 is `11 00 00 00`. Each next part adds `00 20 00 00` in 7-bit address space (part 32 = `18 60 00 00`). Inside that: common, TMT at `00 10 00`, tones at `00 20 / 22 / 24 / 26`.
5. A full SEND is a lot of DT1s. USB throttle defaults to 0 ms. If the module drops the tail, raise the gap in OPTIONS or press SEND again. LIVE drags send one block (envelope or tone head), not the whole patch, at 20 Hz.

SD-20:

- Two PCM tones, no user flash, GM2 CC offsets only. SEND is disabled. Tones 3 and 4 are kept in the file and not transmitted. LIVE sends the offsets plus tone-level CCs.
- SAVE / LOAD `.mesd80patch` and the 144 local slots work the same as on an SD-80 if you would rather not write the module. SYX export still emits an SD-80/90 DT1 dump so the file is useful later.

