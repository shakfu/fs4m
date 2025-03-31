/**
        @file
        fs4m~: fluidsynth object for Max
*/

#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"

#include <stdlib.h>
#include <unistd.h>

#include <fluidsynth.h>

typedef struct _fs4m
{
    t_pxobject ob; // the object itself (t_pxobject in MSP instead of t_object)
    double offset; // the value of a property of our object

    fluid_settings_t *settings;
    fluid_synth_t *synth;
    int sfont_id;
    int mute;
} t_fs4m;

// method prototypes
void* fs4m_new(t_symbol* s, long argc, t_atom* argv);
void fs4m_init(t_fs4m* x);
void fs4m_free(t_fs4m* x);
void fs4m_assist(t_fs4m* x, void* b, long m, long a, char* s);
// void fs4m_float(t_fs4m* x, double f);
void fs4m_bang(t_fs4m* x);
void fs4m_mute(t_fs4m* x);
void fs4m_dsp64(t_fs4m* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags);
void fs4m_perform64(t_fs4m* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam);

// global class pointer variable
static t_class* fs4m_class = NULL;

//***********************************************************************************************

void ext_main(void* r)
{
    t_class* c = class_new("fs4m~", (method)fs4m_new, (method)fs4m_free, (long)sizeof(t_fs4m), 0L, A_GIMME, 0);

    class_addmethod(c, (method)fs4m_bang,  "bang",     0);
    class_addmethod(c, (method)fs4m_mute,  "mute",     0);
    // class_addmethod(c, (method)fs4m_float, "float", A_FLOAT, 0);
    class_addmethod(c, (method)fs4m_dsp64, "dsp64", A_CANT, 0);
    class_addmethod(c, (method)fs4m_assist, "assist", A_CANT, 0);

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    fs4m_class = c;
}

void* fs4m_new(t_symbol* s, long argc, t_atom* argv)
{
    t_fs4m* x = (t_fs4m*)object_alloc(fs4m_class);

    if (x) {
        dsp_setup((t_pxobject*)x, 1); // MSP inlets: arg is # of inlets and is REQUIRED!
        // use 0 if you don't need inlets
        outlet_new((t_pxobject*)x, "signal"); // signal outlet (note "signal" rather than NULL)
        outlet_new((t_pxobject*)x, "signal");
        x->offset = 0.0;
        x->settings = NULL;
        x->synth = NULL;
        x->mute = 0;
        fs4m_init(x);
    }
    return (x);
}

void fs4m_init(t_fs4m* x)
{
    x->settings = new_fluid_settings();
    if(x->settings == NULL)
    {
        object_error((t_object*)x, "Failed to create the settings!");
        goto err;
    }
    fluid_settings_setnum(x->settings, "synth.gain", 0.600000);
    fluid_settings_setnum(x->settings, "synth.sample-rate", sys_getsr());

    /* Change the settings if necessary*/

    /* Create the synthesizer. */
    x->synth = new_fluid_synth(x->settings);
    if(x->synth == NULL)
    {
        object_error((t_object*)x, "Failed to create the synth!");
        goto err;
    }

    /* Load a SoundFont and reset presets (so that new instruments
     * get used from the SoundFont)
     * Depending on the size of the SoundFont, this will take some time to complete...
     */
    x->sfont_id = fluid_synth_sfload(x->synth, "/usr/local/share/sounds/sf2/FluidR3_GM.sf2", 1);
    if(x->sfont_id == FLUID_FAILED)
    {
        object_error((t_object*)x, "Loading the SoundFont failed!");
        goto err;
    }

    post("fluidsynth initialized");
    return;

err:
    /* Clean up */
    // delete_fluid_audio_driver(adriver);
    delete_fluid_synth(x->synth);
    delete_fluid_settings(x->settings);
}

void fs4m_free(t_fs4m* x)
{
    delete_fluid_synth(x->synth);
    delete_fluid_settings(x->settings);
    dsp_free((t_pxobject*)x);
}

void fs4m_assist(t_fs4m* x, void* b, long m, long a, char* s)
{
    if (m == ASSIST_INLET) { // inlet
        sprintf(s, "I am inlet %ld", a);
    }
    else { // outlet
        sprintf(s, "I am outlet %ld", a);
    }
}

// void fs4m_float(t_fs4m* x, double f)
// {
//     x->offset = f;
// }

void fs4m_mute(t_fs4m* x)
{
    if (x->mute == 0) {
        x->mute = 1;
        post("mute == 1");
    } else {
        x->mute = 0;
        post("mute == 0");
    }
}

void fs4m_bang(t_fs4m* x)
{
    int key = 0;

    /* Initialize the random number generator */
    srand(getpid());

    for(int i = 0; i < 12; i++)
    {
        /* Generate a random key */
        key = 60 + (int)(12.0f * rand() / (float) RAND_MAX);

        /* Play a note */
        fluid_synth_noteon(x->synth, 0, key, 80);

        /* Sleep for 1 second */
        sleep(0.3);

        /* Stop the note */
        fluid_synth_noteoff(x->synth, 0, key);
    }
}



// static void fluidmax_dsp(fluidmax_t* self, t_signal** sp, short* count)
// {
//     int n_tick = sp[0]->s_n;

//     dsp_add(fluidmax_perform, 4, self, sp[0]->s_vec, sp[1]->s_vec, n_tick);
// }

// registers a function for the signal chain in Max
void fs4m_dsp64(t_fs4m* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags)
{
    post("my sample rate is: %f", samplerate);

    // instead of calling dsp_add(), we send the "dsp_add64" message to the object representing the dsp chain
    // the arguments passed are:
    // 1: the dsp64 object passed-in by the calling function
    // 2: the symbol of the "dsp_add64" message we are sending
    // 3: a pointer to your object
    // 4: a pointer to your 64-bit perform method
    // 5: flags to alter how the signal chain handles your object -- just pass 0
    // 6: a generic pointer that you can use to pass any additional data to your perform method

    object_method(dsp64, gensym("dsp_add64"), x, fs4m_perform64, 0, NULL);
}


// this is the 64-bit perform method audio vectors
void fs4m_perform64(t_fs4m* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam)
{
    t_double* outL = outs[0]; // we get audio for each outlet of the object from the **outs argument
    t_double* outR = outs[1];
    int n = (int)sampleframes;

    if (x->mute == 0) {
        fluid_synth_write_float(x->synth, n, outL, 0, 1, outR, 0, 1);
    } else {
        for (int i = 0; i < n; i++) {
            outL[i] = outR[i] = 0.0;
        }
    }
}

// void fs4m_perform64(t_fs4m* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam)
// {
//     t_double* inL = ins[0]; // we get audio for each inlet of the object from the **ins argument
//     t_double* outL = outs[0]; // we get audio for each outlet of the object from the **outs argument
//     int n = sampleframes;

//     // this perform method simply copies the input to the output, offsetting the value
//     while (n--) {
//         *outL++ = *inL++ + x->offset;
//     }
// }
