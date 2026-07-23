# Orrery — MIDI + audio routing

Orrery ships as **two devices**, same engine:

- **Orrery** (instrument) — renders internal audio *and* emits MIDI. Goes in a
  track's instrument slot. Use it to hear Orrery standalone, or to drive other
  tracks via MIDI (see routing below).
- **Orrery MFX** (MIDI effect, VST3 + AU `aumi`) — drops **before an instrument
  in the same track** and feeds it directly, *in hosts that support plugin MIDI
  effects in the device chain*: **Reaper, Bitwig, Cubase, Studio One**. No audio
  of its own; the internal voices + virtual port are inert here.

  > **NOT Ableton Live.** Live does **not** host any third-party plugin (VST3 /
  > AU / CLAP) as a chain MIDI effect — that slot is reserved for Live's own
  > MIDI effects and Max for Live devices (Ableton manual, *Working with
  > Instruments and Effects* / *MIDI Tools*). Live categorizes Orrery MFX as an
  > audio effect (so it lands *after* an instrument, useless there). Live 12
  > "MIDI Tools" are a different, offline, AMXD-only format. For Ableton, use
  > the **instrument build + routing** below, or a **Max for Live wrapper**.

The routing below is for the **instrument** build (its MIDI on another track),
and is the Ableton path.

## Two MIDI-out paths (both always active)

1. **Plugin-API MIDI out** — standard note output on the plugin's event bus.
   Hosts that route plugin-generated MIDI (**Reaper, Bitwig, Cubase, Studio One,
   FL**) pick this up directly: set the destination track's MIDI input to
   Orrery's track/output. Sample-accurate.

2. **CoreMIDI virtual port "Orrery"** — Orrery opens its own system MIDI source
   named **`Orrery`**. This is the path for **Ableton Live**, which cannot route
   a plugin's API-level MIDI to other tracks (Live's *MIDI From* taps *before*
   the instrument). ~1 ms of extra latency (goes through CoreMIDI off the audio
   thread — RT-safe by design).

## Ableton Live — step by step

1. Put Orrery on a MIDI track (as an instrument).
2. **Preferences → Link/Tempo/MIDI → MIDI Ports:** find the input row named
   **`Orrery`** and switch its **Track** column **On**. (This input appears
   because Orrery created the virtual port.)
3. On the track you want to drive, set **MIDI From → `Orrery`**, and the channel
   chooser to the incoming channel (default **Ch 1**).
4. Arm/monitor that track (**In**) so it plays the incoming notes live, or record
   them.

Live docs for reference:
- Accessing the MIDI output of a VST plug-in — help.ableton.com/hc/en-us/articles/209070189
- Using virtual MIDI buses in Live — help.ableton.com/hc/en-us/articles/209071169

## Logic

Logic routes AU instrument MIDI out differently; the `Orrery` virtual port is
also visible there (Environment / track MIDI input). A dedicated AU
MIDI-processor (`aumi`) build is the cleaner long-term path for Logic and is a
roadmap item.

## Internal audio on/off

The **Internal Audio** parameter (default **on**) gates the fallback drum
voices. Turn it **off** to make Orrery a silent MIDI generator that only drives
other tracks; leave it **on** to also hear it standalone. MIDI flows on both
paths regardless of this setting.

## Notes / limitations

- **One `Orrery` port per plugin instance.** Multiple instances create multiple
  same-named ports (Live disambiguates them in the list). Per-instance port
  naming is a refinement.
- **auval** prints a benign debug-only `MessageManager` leak at teardown: the
  CoreMIDI subsystem instantiates JUCE's message-thread singleton, which auval's
  minimal host doesn't tear down. Real hosts own the MessageManager, so this
  does not occur in a DAW; AU validation SUCCEEDS. (DECISIONS #15.)
