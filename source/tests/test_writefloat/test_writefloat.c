/*

  An example of how to use FluidSynth.

  To compile it on Linux:
  $ gcc -o example example.c `pkg-config fluidsynth --libs`

  To compile it on Windows:
    ...


  Author: Peter Hanappe.
  This code is in the public domain. Use it as you like.

*/

#include <fluidsynth.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    fluid_settings_t *settings = NULL;
    fluid_synth_t *synth = NULL;
    fluid_audio_driver_t *adriver = NULL;
    int sfont_id;
    int i, key;
    enum { FRAMES = 256 };
    float buf[FRAMES * 2];

    /* Create the settings. */
    settings = new_fluid_settings();
    if(settings == NULL)
    {
        puts("Failed to create the settings!");
        goto err;
    }

    /* Change the settings if necessary*/

    /* Create the synthesizer. */
    synth = new_fluid_synth(settings);
    if(synth == NULL)
    {
        puts("Failed to create the synth!");
        goto err;
    }

    /* Load a SoundFont and reset presets (so that new instruments
     * get used from the SoundFont)
     * Depending on the size of the SoundFont, this will take some time to complete...
     */
    sfont_id = fluid_synth_sfload(synth, "examples/sf2/GM.sf2", 1);
    if(sfont_id == FLUID_FAILED)
    {
        puts("Loading the SoundFont failed!");
        goto err;
    }

    /* Initialize the random number generator */
    srand(getpid());

    for(i = 0; i < 12; i++)
    {
        /* Generate a random key */
        key = 60 + (int)(12.0f * rand() / (float) RAND_MAX);

        /* Play a note */
        fluid_synth_noteon(synth, 0, key, 80);

        /* Sleep for 1 second */
        sleep(0.1);

        /* Stop the note */
        fluid_synth_noteoff(synth, 0, key);
    }

    fluid_synth_write_float(synth, FRAMES, buf, 0, 2, buf, 1, 2);

    for (int i=0; i < FRAMES*2; i++) {
        printf("buf[%d] = %f\n", i, buf[i]);
    }

err:
    /* Clean up */
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);

    return 0;
}
