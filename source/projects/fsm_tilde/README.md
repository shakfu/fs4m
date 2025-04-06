# fsm_tilde -- a minimal fluidsynth 2.4.4 external for Max/MSP

## Overview

`fsm~` is a Max/MSP external that provides FluidSynth synthesis capabilities directly in Max/MSP patches. It allows you to:

- Load and play SoundFont (.sf2) files
- Control MIDI parameters like notes, program changes, and control changes
- Adjust synthesis parameters like reverb, chorus, and gain
- Whatever feature are provided by the command handling infrastructure builtin to fluidsynth 2.4.x

## Features

- FluidSynth 2.4.x integration
- Support for SoundFont 2.0/2.1 files
- Real-time audio processing
- MIDI control interface
- Effects processing (reverb, chorus)

## Usage

Basic usage involves:

1. Creating a `fsm~` object in your Max patch
2. Connecting the audio outputs to your patch
3. Loading a SoundFont file using the `load` message
4. Sending MIDI messages to control synthesis

The external supports both signal and message inlets for audio and control respectively.

## Building

The project is currently MacOS only, and requires

```sh
brew install fluidsynth
```

To build, just type

```sh
make
```

alternatively, to build with static libraries:

```sh
make static
```
