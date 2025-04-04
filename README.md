# Towards a modern fluidsynth~ External for Max/MSP

This project modernizes the original `fluidmax~` external for Max/MSP, originally created by Norbert Schnell at IRCAM and included in the `fluidsynth-1.1.7` distribution. The goal is to provide a fully functional fluidsynth implementation that works with Max 8/9 and fluidsynth 2.4.4.

## Project History

The development journey began with an attempt to update the original (Norbert Schnell) fluidmax code, but led to the discovery of Volker Böhm's existing working update of the former. After studying Volker's implementation (based on fluidsynth 1.0.8) from his [max-thirdParty_externals collection](https://github.com/v7b1/max-thirdParty_externals), the project was restarted with a clear focus on supporting fluidsynth 2.4.4.

## Included Externals

The project contains three externals:

1. `fluidsynth_tilde` (Volker Böhm's reference implementation)
   - Located in `sources/thirdparty`
   - Based on fluidsynth 1.0.8
   - Special feature: self-contained compilation without external dependencies

2. `fsm_tilde`
   - Updated version of the original `fluidmax~` external for Max 8/9.
   - Removes deprecated functions
   - Compatible with fluidsynth 2.4.4

3. `fs4m_tilde`
   - New implementation from scratch
   - Used for testing fluidsynth 2.4.4 features
   - Simpler architecture

## Building and Development

- Primary development platform: MacOS M1 Macbook
- Build requirements:
  - Xcode
  - fluidsynth (via Homebrew)

First

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
