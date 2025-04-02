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

#define BUFFERED 0

#define fm_error(x, ...) object_error((t_object*)(x), __VA_ARGS__);
#define fm_post(x, ...) object_post((t_object*)(x), __VA_ARGS__);

typedef struct _fm
{
    t_pxobject ob; // the object itself (t_pxobject in MSP instead of t_object)

    fluid_settings_t *settings;
    fluid_synth_t *synth;
    double * left_buffer;
    double * right_buffer;
    int sfont_id;
    int mute;
} t_fm;

// method prototypes
void* fm_new(t_symbol* s, long argc, t_atom* argv);
void fm_init(t_fm* x);
void fm_free(t_fm* x);
void fm_assist(t_fm* x, void* b, long m, long a, char* s);
// void fm_float(t_fm* x, double f);
void fm_bang(t_fm* x);
void fm_noteon(t_fm* x);
void fm_noteoff(t_fm* x);
void fm_mute(t_fm* x);
void fm_load(t_fm* x, t_symbol* sfont);
void fm_dsp64(t_fm* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags);
void fm_perform64(t_fm* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam);

// global class pointer variable
static t_class* fm_class = NULL;

//***********************************************************************************************

void ext_main(void* r)
{
    t_class* c = class_new("fs4m~", (method)fm_new, (method)fm_free, (long)sizeof(t_fm), 0L, A_GIMME, 0);

    class_addmethod(c, (method)fm_bang,     "bang",     0);
    class_addmethod(c, (method)fm_mute,     "mute",     0);
    class_addmethod(c, (method)fm_noteon,   "on",       0);
    class_addmethod(c, (method)fm_noteoff,  "off",      0);
    // class_addmethod(c, (method)fm_float, "float",    A_FLOAT, 0);
    class_addmethod(c, (method)fm_load,     "load",     A_SYM,  0);
    class_addmethod(c, (method)fm_dsp64,    "dsp64",    A_CANT, 0);
    class_addmethod(c, (method)fm_assist,   "assist",   A_CANT, 0);

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    fm_class = c;
}

void* fm_new(t_symbol* s, long argc, t_atom* argv)
{
    t_fm* x = (t_fm*)object_alloc(fm_class);

    if (x) {
        dsp_setup((t_pxobject*)x, 1); // use 0 if you don't need inlets
        outlet_new((t_pxobject*)x, "signal");
        outlet_new((t_pxobject*)x, "signal");
        x->settings = NULL;
        x->synth = NULL;
        x->left_buffer = NULL;
        x->right_buffer = NULL;
        x->mute = 0;
        fm_init(x);
    }
    return (x);
}

void fm_init(t_fm* x)
{
    x->settings = new_fluid_settings();
    if(x->settings == NULL)
    {
        fm_error(x, "Failed to create the settings!");
        return;
    }
    fluid_settings_setnum(x->settings, "synth.gain", 0.600000);
    fluid_settings_setnum(x->settings, "synth.sample-rate", sys_getsr());

    x->synth = new_fluid_synth(x->settings);
    if(x->synth == NULL)
    {
        fm_error(x, "Failed to create the synth!");
        return;
    }

    x->sfont_id = fluid_synth_sfload(x->synth, "/usr/local/share/sounds/sf2/FluidR3_GM.sf2", 1);
    if(x->sfont_id == FLUID_FAILED)
    {
        fm_error(x, "Loading the SoundFont failed!");
        return;
    }

    double srate, midi_event_latency;
    int period_size;
    
    fluid_settings_getint(x->settings, "audio.period-size", &period_size);
    fluid_settings_getnum(x->settings, "synth.sample-rate", &srate);
    
    midi_event_latency = period_size / srate;
    if(midi_event_latency >= 0.05)
    {
        fm_post(x, 
            "You have chosen 'audio.period-size' to be %d samples. "
            "Given a sample rate of %.1f this results in a latency of %.1f ms, "
            "which will cause MIDI events to be poorly quantized (=untimed) "
            "in the synthesized audio (also known as the 'drunken-drummer' "
            "syndrome). To avoid that, you're strongly advised to increase "
            "'audio.periods' instead, while keeping 'audio.period-size' small "
            "enough to make this warning disappear.", 
            period_size, srate, midi_event_latency * 1000.0);
    }

    double gain;
    fluid_settings_getnum(x->settings, "synth.gain", &gain);
    fm_post(x, "gain: %f", gain);
    fm_post(x, "srate: %f", srate);
    fm_post(x, "fluidsynth initialized");
}

void fm_free(t_fm* x)
{
    delete_fluid_synth(x->synth);
    delete_fluid_settings(x->settings);
    dsp_free((t_pxobject*)x);
}

void fm_assist(t_fm* x, void* b, long m, long a, char* s)
{
    if (m == ASSIST_INLET) { // inlet
        sprintf(s, "I am inlet %ld", a);
    }
    else { // outlet
        sprintf(s, "I am outlet %ld", a);
    }
}

void fm_load(t_fm* x, t_symbol* sfont)
{
    x->sfont_id = fluid_synth_sfload(x->synth, sfont->s_name, 1);
    if(x->sfont_id == FLUID_FAILED)
    {
        fm_error(x, "Loading SoundFont '%s' failed!", sfont->s_name);
    }
}

void fm_noteon(t_fm* x)
{
    fluid_synth_noteon(x->synth, 0, 60, 100);
}

void fm_noteoff(t_fm* x)
{
    fluid_synth_noteoff(x->synth, 0, 60);
}

void fm_mute(t_fm* x)
{
    if (x->mute == 0) {
        x->mute = 1;
        post("mute == 1");
    } else {
        x->mute = 0;
        post("mute == 0");
    }
}

void fm_bang(t_fm* x)
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

#if BUFFERED == 1

void fm_dsp64(t_fm* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags)
{
    post("buffered audio processing");
    fluid_settings_setnum(x->settings, "synth.sample-rate", samplerate);

    sysmem_freeptr(x->left_buffer);
    sysmem_freeptr(x->right_buffer);

    x->left_buffer = (double*)sysmem_newptr(sizeof(double) * maxvectorsize);
    x->right_buffer = (double*)sysmem_newptr(sizeof(double) * maxvectorsize);

    memset(x->left_buffer, 0.f, sizeof(double) * maxvectorsize);
    memset(x->right_buffer, 0.f, sizeof(double) * maxvectorsize);

    object_method(dsp64, gensym("dsp_add64"), x, fm_perform64, 0, NULL);
}


void fm_perform64(t_fm* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam)
{
    double* bufL = x->left_buffer;
    double* bufR = x->right_buffer;
    int n = (int)sampleframes;
    // double* dry[2];
    // dry[0] = bufL;
    // dry[1] = bufR;

    if (x->mute == 0) {
        
        fluid_synth_write_float(x->synth, n, bufL, 0, 1, bufR, 0, 1);
        // fluid_synth_process(x->synth, n, 0, NULL, 2, dry);
        for (int i = 0; i < n; i++) {
            outs[0][i] = *bufL++;
            outs[1][i] = *bufR++;
        }

    } else {
        for (int i = 0; i < n; i++) {
            outs[0][i] = outs[1][i]= 0.0;
        }
    }
}

#else

void fm_dsp64(t_fm* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags)
{
    fluid_settings_setnum(x->settings, "synth.sample-rate", samplerate);

    object_method(dsp64, gensym("dsp_add64"), x, fm_perform64, 0, NULL);
}


void fm_perform64(t_fm* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam)
{
    t_double* outL = outs[0]; // we get audio for each outlet of the object from the **outs argument
    t_double* outR = outs[1];
    int n = (int)sampleframes;


    if (x->mute == 0) {
        fluid_synth_write_float(x->synth, n, outL, 0, 1, outR, 0, 1);

        // fluid_synth_process(x->synth, n, 0, NULL, 2, outs);

    } else {
        for (int i = 0; i < n; i++) {
            outL[i] = outR[i] = 0.0;
        }
    }
}

#endif

