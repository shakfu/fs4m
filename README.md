# fs4m: a project to modernize the fluidsynth~ external for Max/MSP

A project to modernize the old Ircam (max 4) `fluidmax~` fluidsynth for Max/MSP external by Norbert Schnell and update it for use by Max 8/9 and fluidsynth 2.4.x. The updated external is called `fluidsynth~` for consistency.

So far the prior`fluidmax~` external (included in `source/thirdparty`) has been revised to incorporate modern idioms and deprecated api methods has been removed. In addition, a key goal was to generate audio successfully from an `.sf2` sample archive. To date these steps have been largely done.

All development is done on a MacOS M1 Macbook laptop.

## Requires

```sh
brew install fluidsynth
```


## Project Status

- [x] initial code coversion

- [x] converted old binary `.help~` file to `.maxhelp`

- [x] was able to load `FluidR3_GM.sf2` and generate audio successfully

- [x] replace deprecated / disabled functions

- [ ] add new useful api methods

- [ ] improve crowded `.maxhelp` file

- [ ] make a self-contained bundle (with sf2 files and dylib dependencies)
