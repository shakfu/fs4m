# Towards a modern fluidsynth~ external for Max/MSP

This project started out as an effort to modernize the old Max 4 fluidsynth v1 external, called `fluidmax~`, originally written by Norbert Schnell which I discovered included in the `fluidsynth-1.1.7` distribution. The code for this external didn't compile in the case of Max 8 and Max 9, and needed refreshing, so I started this project and a few days into upgrading the fluidsynth code base to 2.4.4, I was pleased to find out that I got that it worked well in my initial tests.

The funny thing is that I had previously downloaded, installed and forgotten that I had installed into my Max Packages directory, Volker Böhm's update of the same fluidmax external, and subsequently discovered that my initial test successes were wholly due to his efforts and not mine. (-:

After disabling Volker's external from the Packages directory, I discovered that my modernization efforts did not work with fluidsynth 2.4.4 and indeed, I didn't have the source of Volker's external to discover why.. 

A few days later, I happily found the source (Thanks again Volker), kindly made available in his [max-thirdParty_externals collection](https://github.com/v7b1/max-thirdParty_externals), and noted that his fixed version used a modified version of fluidsynth 1.0.8.

Thereafter, I restarted work with the discrete objective of updating prior code (Norbert's and Volker's) to work with fluidsynth 2.4.4 and Max 8/9.

The result of the above adventure is that this project include several externals:

1. Volker's `fluidsynth_tilde`, for reference, which is located in the `sources/thirdparty` folder, and is based on a modified version of fluidsynth 1.0.8. It has the wonderful and special feature of compiling somehow without dependencies!

2. `fsm_tilde`: an update of Norbert's older fluidmax but with the removal of deprecated functions and which works with fluidsynth 2.4.4

3. `fs4m_tilde`: a simpler external which was written from scratch, and which is used for experimentation with fluidsynth 2.4.4 features in Max. 

I have included the code of the original fluidmax project in the `thirdparty` folder for reference.


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

- [x] was able to load `FluidR3_GM.sf2` and generate audio successfully

- [x] replace deprecated / disabled functions

- [ ] add new useful api methods

- [ ] improve crowded `.maxhelp` file

- [ ] make a self-contained bundle (with sf2 files and dylib dependencies)
