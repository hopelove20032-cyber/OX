# OX – virtual stage instrument (VST3 for FL Studio, Windows)

41 voices in 8 groups, a Nord-Stage-style red panel, a clickable voice display and a 61-key keyboard.

| Group | Voices |
|---|---|
| Piano | Grand, Bright, Upright, Honky-Tonk, Rhodes, DX E.Piano, Clavinet |
| Mallets | Xylophone, Marimba, Vibraphone, Glockenspiel, Kalimba, Music Box, Celesta, Tubular Bells |
| Guitar | Steel, Nylon, Lead (overdrive), Muted, Banjo, Harp, Pluck Bass |
| Synth | Saw Lead, Super Saw, Square Lead, Analog Pad, Synth Bass |
| Fluid | Fluid Keys, Fluid Lead, Fluid Bell |
| Strings | String Ensemble, Synth Brass, Choir Ah |
| Winds | Ocarina, Flute, Pan Flute, Clarinet, Whistle |
| Organ | Jazz, Rock, Church |

Controls: Level, Tone, Attack, Release, Chorus, Reverb, pitch-bend wheel, mod wheel (vibrato; morphs Choir Ah to Oh).
The selected voice is saved with your FL Studio project and can be automated.

## Get the plugin (no compiler needed)

1. Create a free GitHub account and a new empty repository.
2. Upload everything in this folder (keep the `.github` folder).
3. Open the **Actions** tab, wait for "Build OX (Windows VST3)" to finish (about 10 minutes).
4. Open the finished run and download the artifact **OX-VST3-Windows**.

## Or build it yourself

Install Visual Studio 2022 ("Desktop development with C++"), CMake and Git, then double-click `build_windows.bat`.
The plugin appears in `build\OX_artefacts\Release\VST3\OX.vst3`.

## Install in FL Studio

1. Copy the `OX.vst3` folder to `C:\Program Files\Common Files\VST3\`
2. FL Studio: Options > Manage plugins > Find installed plugins
3. Add OX from the plugin database (Add > Instruments > OX), then play.

## Notes

- Voices are synthesised (physical modelling, modal, FM, subtractive), not recorded samples, so there are no large sound files.
- Adding voices: add a row to `Source/Patches.h`.
