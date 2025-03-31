# fs4m: fluidsynth for max

An ongoing experiment to modernize the old Ircam (max 4) `fluidmax~` fluidsynth for max external by Norbert Schnell and update it for use by Max 8/9 and fluidsynth 2.4.x

So far this consists of trying to get a minimal external working with the modern fluidsynth (`fs4m~`) and converting `fluidmax~` to modern idioms.

All development is being done on a MacOS M1 macbook.

## Project Status

### fs4m~

- [ ] initial audio test still not working


### fluidmax~

- [x] initial code coversion

- [x] converted old binary `.help~` file to `.maxhelp`

- [x] was able to load `FluidR3_GM.sf2` and generate audio successfully

- [ ] replace deprecated / disabled functions

