# A modern fluidsynth~ external for Max/MSP

`fs4m` is a project to modernize the old Ircam (Max 4) `fluidmax~` fluidsynth external by Norbert Schnell and update it for use in Max 8/9 and fluidsynth 2.4.x. The updated external is called `fluidsynth~` for consistency.

So far code from the prior`fluidmax~` external (included in `source/thirdparty`) has been revised to incorporate modern idioms and deprecated api calls has been removed.

Audio generation is still pending.

All development is done on a MacOS M1 Macbook laptop.


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

- [ ] was able to load `FluidR3_GM.sf2` and generate audio successfully

- [x] replace deprecated / disabled functions

- [ ] add new useful api methods

- [ ] improve crowded `.maxhelp` file

- [ ] make a self-contained bundle (with sf2 files and dylib dependencies)
