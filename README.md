# Towards a modern fluidsynth~ external for Max/MSP

This project modernizes the original `fluidmax~` external for Max/MSP, originally created by Norbert Schnell at IRCAM and included in the `fluidsynth-1.1.7` distribution. The goal is to provide a fully functional fluidsynth implementation that works with Max 8/9 and fluidsynth 2.4.4.

## Project History

The development journey began with an attempt to update the original (Norbert Schnell) fluidmax code, but led to the discovery of Volker Böhm's existing working update of the former. After studying Volker's implementation (based on fluidsynth 1.0.8) from his [max-thirdParty_externals collection](https://github.com/v7b1/max-thirdParty_externals), the project was restarted with a clear focus on supporting fluidsynth 2.4.4.

## Included Externals

The project contains four externals:

1. `fluidsynth_tilde` (Volker Böhm's implementation, included only for reference)
   - Located in `source/thirdparty`
   - Based on reduced and modified fluidsynth 1.0.8
   - Special feature: self-contained compilation without external dependencies
   - Will not be modified further.

2. `fm_tilde` -- the update
   - Updated version of the original Norbert Schnell `fluidmax~` external for Max 8/9.
   - Removed and replaced deprecated functions
   - Changes to be make compatible with fluidsynth 2.4.4

3. `fs4m_tilde` -- the rewrite
   - New implementation from scratch
   - Based on fluidsynth 2.4.4
   - Aiming for maximal feature coverage

4. `fsm_tilde` -- the minimal rewrite
   - New minimal implementation from scratch
   - Based on fluidsynth 2.4.4
   - Hooks into fluidsynth's builtin command handling infrastructure


## Building and Development

- Primary development platform: MacOS M1 Macbook
- Build requirements:
  - Xcode
  - fluidsynth (via Homebrew)

To build with shared libraries:

```sh
brew install fluidsynth
```

then just

```sh
make
```

alternatively, to build with static libraries:

```sh
make static
```


## Project Status

- [x] initial code coversion

- [x] converted old binary `.help~` file to `.maxhelp`

- [x] was able to load `FluidR3_GM.sf2` and generate audio successfully

- [x] replace deprecated / disabled functions

- [ ] add new useful api methods

- [ ] improve crowded `.maxhelp` file

- [ ] make a self-contained bundle (with sf2 files and dylib dependencies)
