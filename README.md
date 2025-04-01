# A modern fluidsynth~ external for Max/MSP

`fs4m` is a project to modernize the old Ircam (Max 4) `fluidmax~` fluidsynth external by Norbert Schnell and update it for use in Max 8/9 and fluidsynth 2.4.x. The updated external is called `fluidsynth~` for consistency.

So far code from the prior`fluidmax~` external (included in `source/thirdparty`) has been revised to incorporate modern idioms and deprecated api calls has been removed. In addition, a key goal was to generate audio successfully from an `.sf2` sample archive, which has been achieved.

All development is done on a MacOS M1 Macbook laptop.

## Overview

`fluidsynth~` is a Max/MSP external that provides FluidSynth synthesis capabilities directly in Max/MSP patches. It allows you to:

- Load and play SoundFont (.sf2) files
- Control MIDI parameters like notes, program changes, and control changes
- Adjust synthesis parameters like reverb, chorus, and gain
- Tune instruments and modify polyphony settings
- Monitor and debug synthesis state through various info and print methods

The external is designed to be a modern replacement for the older `fluidmax~` external, with updated API support and improved functionality.

## Features

- Full FluidSynth 2.4.x integration
- Support for SoundFont 2.0/2.1 files
- Real-time audio processing
- MIDI control interface
- Effects processing (reverb, chorus)
- Tuning system support
- Comprehensive monitoring and debugging tools

## Usage

Basic usage involves:

1. Creating a `fluidsynth~` object in your Max patch
2. Loading a SoundFont file using the `load` message
3. Sending MIDI messages to control synthesis
4. Connecting the audio outputs to your patch

The external supports both signal and message inlets for audio and control respectively.

## Development

The project is developed on macOS (M1) with the following goals:

- Modernize the codebase
- Improve documentation
- Add new features
- Create self-contained bundles
- Enhance the user interface

## Building on MaxOS

Requires (Xcode) and

```sh
brew install fluidsynth
```

then just

```sh
make
```

## Project Status

- [x] initial code coversion

- [x] converted old binary `.help~` file to `.maxhelp`

- [x] was able to load `FluidR3_GM.sf2` and generate audio successfully

- [x] replace deprecated / disabled functions

- [ ] add new useful api methods

- [ ] improve crowded `.maxhelp` file

- [ ] make a self-contained bundle (with sf2 files and dylib dependencies)
