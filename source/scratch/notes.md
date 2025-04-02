
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
