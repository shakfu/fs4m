# Towards a modern fluidsynth~ external for Max/MSP

This project modernizes the original `fluidmax~` external for Max/MSP, originally created by Norbert Schnell at IRCAM and included in the `fluidsynth-1.1.7` distribution. The goal is to provide a fully functional fluidsynth implementation that works with Max 8/9 and `fluidsynth 2.4.4`.

## Project History

The development journey began with an attempt to update the original (Norbert Schnell) fluidmax code, but led to the discovery of Volker Böhm's existing working update of the former. After studying Volker's implementation[^1] (based on fluidsynth 1.0.8) from his [max-thirdParty_externals collection](https://github.com/v7b1/max-thirdParty_externals), the project was restarted with a clear focus on supporting fluidsynth 2.4.4

[^1]: Volker Böhm's [fluidsynth~](https://github.com/v7b1/max-thirdParty_externals/tree/main/source/projects/fluidsynth_tilde) is based on a modified fluidsynth 1.0.8 code-base, and has a unique special power: self-contained compilation without external dependencies

## Included Externals

The project contains three externals:

1. `fm_tilde` -- the update
   - Updated version of the original Norbert Schnell `fluidmax~` external for Max 8/9.
   - Removed and replaced deprecated functions
   - Made compatible with fluidsynth 2.4.4

2. `fs4m_tilde` -- the rewrite
   - New implementation from scratch
   - Based on fluidsynth 2.4.4
   - Aiming for maximal feature coverage

3. `fsm_tilde` -- the minimal rewrite
   - New minimal implementation from scratch
   - Based on fluidsynth 2.4.4
   - Hooks into fluidsynth's builtin command handling infrastructure
   - Aiming to keep as small as possible (even as single-header lib...)

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
