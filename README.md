# Towards a modern fluidsynth~ external for Max/MSP

This project is about modernizing an old Max 4 fluidsynth v1 external (for Max/MSP originally by Norbert Schnell which I found included in `fluidsynth-1.1.7`, called fluidmax.  Subsequently, I happily discovered an effort at modernization of the same code by Volker Bohm of the same external [here](https://github.com/v7b1/max-thirdParty_externals) albeit using a modified version of FluidSynth v1.0.8 (and which works well in Max 8/9).

Current work on v2 modernization project is ongoing (macOS only for the time being). While initial effort was simply to get it work with modern Max 8 or 9 idioms, clean up the code, and remove / replace deprecated fluidsynth api calls. However, it is not currently functional as the audio is not working as expected. 

I have included the code of the original fluidmax project in the `thirdparty` folder and also Volker Bohm's working 64-bit update of the former which is compiled in this project.

If one can figure out what is causing this audio generation issue, a new fluidsynth~ v2 version will be provided here.

All development is done on a MacOS M1 Macbook laptop.


## Building on MacOS

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

- [ ] was able to load `FluidR3_GM.sf2` and generate audio successfully

- [x] replace deprecated / disabled functions

- [ ] add new useful api methods

- [ ] improve crowded `.maxhelp` file

- [ ] make a self-contained bundle (with sf2 files and dylib dependencies)
