# Fluidsynth externals for Max/MSP

This project includes a number of in-development Max/MSP externals which work with [Fluidsynth](https://www.fluidsynth.org) `2.4.x`. MacOS versions are currently only available at this stage.

## Project History

The development journey began with an attempt to update the original `fluidmax~` code by Norbert Schnell at IRCAM that was included in source distributions of fluidsynth-1.1.7. Further research led to the discovery of Volker Böhm's `fluidsynth~` external based on modified fluidsynth 1.0.8 code. After studying Volker's implementation[^1] included in his [max-thirdParty_externals collection](https://github.com/v7b1/max-thirdParty_externals), the project was restarted with a clear focus on developing externals which support fluidsynth 2.4.x.

[^1]: Volker Böhm's [fluidsynth~](https://github.com/v7b1/max-thirdParty_externals/tree/main/source/projects/fluidsynth_tilde) has a unique special power: self-contained compilation without external dependencies! It is included, for reference, in the `source/thirdparty/fluidsynth_tilde` folder, and can be built via `make thirdparty`.

## Included Externals

The project contains three externals:

1. `fs_tilde` -- the update
   - Updated version of the original Norbert Schnell `fluidmax~` external for Max 8/9.
   - Removed and replaced deprecated functions
   - Made compatible with fluidsynth 2.4.x

2. `fs4m_tilde` -- the rewrite
   - New implementation from scratch
   - Based on fluidsynth 2.4.x
   - Aiming for maximal feature coverage

3. `fsm_tilde` -- the minimal rewrite
   - New minimal implementation from scratch
   - Based on fluidsynth 2.4.x
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

alternatively, to build with static libraries (this will download and build fluidsynth):

```sh
make static
```
