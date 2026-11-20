# LOERIC Smart Keyboard (SL Link)

Transform a [Studiologic SL mk2](https://www.studiologic-music.com/products/slmk2/) into a smart keyboard for performing with
[LOERIC](https://github.com/amerotz/loeric), directly from the instrument.

LOERIC is a live performance rule system for Irish traditional music, adding expression,
ornamentation and real-time human interaction to musical performances. This project brings
LOERIC into the keyboard-playing experience: choose a tune, select an instrument model,
start a performance and adjust its behaviour using the SL's own display and physical controls.

The goal is to make LOERIC feel like part of the instrument, rather than a separate application
operated from a computer.

A headless JUCE application connects the SL mk2 to a LOERIC server, provides a local SoundFont
synth for playing the keyboard, and uses **SL Link** to present the interface on the keyboard's
display. The SL's encoders, buttons, joystick and LEDs become the controls for the combined system.

> NOTE: The application and LOERIC server run on a host computer, not on the SL keyboard itself.
> They can run on the same host or on separate machines connected over a local network.
> The keyboard provides the playing surface and hardware interface; no application window
> is required.

---

## Features

- **LOERIC at the keyboard**: browse tunes, instrument models and configurations, set repetitions,
  and start or stop performances without switching to the web interface.
- Real-time performance control: adjust tempo, transpose, responsiveness and LOERIC volume
  from the SL's physical controls.
- Local SoundFont synthesis: play the keyboard through FluidLite, using an embedded grand piano
  or a custom `.sf2` file.
- Integrated SL Link interface: a custom `LoericPage` with lists, knobs, status colours and
  LED feedback on the keyboard.
- Live server synchronisation: poll `/api/status` every second, follow parameter changes made
  elsewhere, and automatically reload tunes, instrument models and configurations after
  a server restart.
- Flexible deployment: communicate with a local or networked LOERIC server over HTTP.

---

## How it works

The system combines three components:

| Component | Role |
|---|---|
| Studiologic SL mk2 | Keyboard input, display, physical controls and LED feedback |
| LoericSmartKeyboard | Headless JUCE application providing the SL Link interface, local SoundFont synthesis and communication with LOERIC |
| LOERIC server | Musical performance system, controlled through its HTTP API |

The application connects two parts of the playing experience:

- Local keyboard sound: the keyboard is played through the application's SoundFont synth.
- LOERIC performance: the application controls the LOERIC server and displays its current state.

The local synth and LOERIC have separate volume controls.

### LOERIC server

A running [LOERIC](https://github.com/amerotz/loeric) server is required for LOERIC performance
features. See the LOERIC repository for installation and setup.

By default, the application connects to `http://localhost:8080`. Use `--loeric <url>` to connect
to a different host or port.

---

## Build

First clone the code:
```bash
git clone --recurse-submodules https://github.com/gregogiudici/Loeric_Smart_Keyboard.git
cd Loeric_Smart_Keyboard
```

Then, build the headless application:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The executable is written to:

```text
build/LoericSmartKeyboard_artefacts/<Config>/LoericSmartKeyboard
```

---

## Run

For example, to use the SL audio device and connect to a LOERIC server on the local network:

```bash
./LoericSmartKeyboard --audio-device "SL AUDIO" --loeric http://nexus-keyboard.local:8080
```

### Command-line options

| Option | Description |
|---|---|
| `--loeric <url>` | URL of the LOERIC server; default: `http://localhost:8080` |
| `--list-audio` | List audio output devices and exit |
| `--list-midi` | List MIDI input devices and exit |
| `--audio-device <name>` | Use the audio device whose name contains `<name>` |
| `--midi-device <name>` | Use the MIDI device whose name contains `<name>` |
| `--sample-rate <rate>` | Sample rate in Hz; default: `44100` |
| `--buffer-size <samples>` | Buffer size in samples; default: `256` |
| `--soundfont <file>` | Load an `.sf2` file instead of the embedded piano |
| `--dry-run` | Check devices, print the resolved configuration, then exit |
| `--help` | Show help and exit |

---

## Playing with LOERIC

Use the keyboard's interface to prepare and control a performance:

1. Select the Tune tab, browse the available tunes and press the joystick to confirm.
2. Select an instrument model in the Model tab.
3. Choose a configuration in the Config tab, or keep the pre-selected default.
4. Set the number of score repetitions in the Repeat tab.
5. Press APPLY to start.
6. Adjust tempo, transpose, responsiveness and volume while playing.
7. Press CANCEL to stop.

Tune, model, configuration and repetition settings are locked while a performance is running.
The real-time controls remain available.

---

## SL keyboard interface

The application uses a single SL Link page, laid out from top to bottom:

```text
┌──────────────────────────────────────────┐
│ ■            LOERIC                      │  Header: server status
│        PLAYING - Dan Dowd's              │
├──────────────────────────────────────────┤
│    (◯)      (◯)       (◯)       (◯)      │  Knobs Z1..Z4
│   Tempo  Transpose  Respons.    N/A      │
│                                          │
│            Dan Dowd's                    │  Content of the selected tab
│        3 / 12   press to select          │
│   [Tune]  [Model]   [Config]  [Repeat]   │  Tabs (SELECT_ZOOM 1..4)
└──────────────────────────────────────────┘
```

### Status colours

The header, separator, knobs, knob LEDs, selected tab and confirmed list item follow
the server status:

| Status | Colour | Meaning |
|---|---|---|
| DISCONNECTED | Coral | `/api/status` is not reachable |
| IDLE | Orange | Connected; no performance is running |
| PLAYING | Green | A performance is running; the tune name is shown |

### Controls

| Control | Action |
|---|---|
| Z1 | Tempo, 40–250 QPM (`/api/tempo`) |
| Z2 | Transpose, −24…+24 (`/api/change/groover/modules/transpose/steps`) |
| Z3 | Responsiveness, 0–1 (`/api/responsiveness`) |
| Z4 | Placeholder for a parameter sent through `/api/change/{path}`; currently shown as N/A |
| Encoder A | Local SoundFont player volume, 0–1; popup closes after 1 s |
| Encoder B | LOERIC volume, 0–1 (`/api/volume`); popup closes after 1 s |
| SELECT_ZOOM 1–4 | Select the Tune / Model / Config / Repeat tab |
| Joystick up/down or rotate | Scroll the list or change the number of repetitions |
| Joystick press | Confirm the highlighted list item |
| APPLY | Start a performance with the current selection and knob values |
| CANCEL | Stop the performance |

Knob changes are sent to the server during adjustment, with updates coalesced approximately
every 80 ms. Knobs also follow changes made elsewhere, such as in the web UI, except briefly
after a local adjustment.

### Tabs

The tabs contain settings for the next performance, passed to `/api/start`.
They are locked while playing: tab LEDs are off and the interface is greyed out,
with the message "Locked while playing".

| Tab | Content |
|---|---|
| Tune | Tunes from `/api/tunes`; dot-files are hidden. Confirming a tune loads its configurations. |
| Model | Instrument models from `/api/instruments` |
| Config | Configurations for the selected tune from `/api/configs/{tune}`; the default is pre-selected |
| Repeat | Number of score repetitions, from 1 to 99 |

A highlighted list item is white until confirmed. The confirmed item is drawn in the current
status colour.

### LEDs

- Z1–Z4 RGB LEDs: current status colour; off when disconnected or unassigned.
- SELECT_ZOOM LEDs: indicate the selected tab; all off while playing.
- APPLY LED: on when a performance can be started—tune and model selected—or is already playing.
- CANCEL LED: on while playing.