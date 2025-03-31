#define FLUIDMAX_VERSION "03/2025 (15)"

#include <stdlib.h>

#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"

#include "fluidsynth.h"

#define M_TWOPI 6.283185308
// #define M_PI 3.141592654


int is_long(t_atom* a) { return atom_gettype(a) == A_LONG; }
int is_float(t_atom* a) { return atom_gettype(a) == A_FLOAT; }
int is_number(t_atom* a)
{
    return (atom_gettype(a) == A_LONG || atom_gettype(a) == A_FLOAT);
}
int is_symbol(t_atom* a) { return atom_gettype(a) == A_SYM; }

int get_number_as_long(t_atom* a)
{
    if (is_number(a)) {
        if (is_long(a)) {
            return (int)atom_getlong(a);
        } else {
            return (int)atom_getfloat(a);
        }
    } else {
        error("atom is not a number");
        return -1;
    }
}

double get_number_as_float(t_atom* a)
{
    if (is_number(a)) {
        if (is_long(a)) {
            return (double)atom_getlong(a);
        } else {
            return atom_getfloat(a);
        }
    } else {
        error("atom is not a number");
        return -1.0;
    }
}


typedef struct {
    t_pxobject obj;

    fluid_synth_t* synth;
    fluid_settings_t* settings;
    int reverb;
    int chorus;
    int mute;
    void* outlet;
} t_fluidmax;

static t_class* fluidmax_class;


void fluidmax_perform64(t_fluidmax* x, t_object* dsp64, double** ins,
                        long numins, double** outs, long numouts,
                        long sampleframes, long flags, void* userparam);


/***************************************************************
 *
 *  generators
 *
 */
typedef struct {
    int index;
    const char* name;
    const char* unit;
} fluidmax_gen_descr_t;

// clang-format off
static fluidmax_gen_descr_t fluidmax_gen_info[] = {
    { 0, "startAddrsOffset", "samples" },
    { 1, "endAddrsOffset", "samples" },
    { 2, "startloopAddrsOffset", "samples" },
    { 3, "endloopAddrsOffset", "samples" },
    { 4, "startAddrsCoarseOffset", "32k samples" },
    { 5, "modLfoToPitch", "cent fs" },
    { 6, "vibLfoToPitch", "cent fs" },
    { 7, "modEnvToPitch", "cent fs" },
    { 8, "initialFilterFc", "cent 8.176 Hz" },
    { 9, "initialFilterQ", "cB" },
    { 10, "modLfoToFilterFc", "cent fs" },
    { 11, "modEnvToFilterFc", "cent fs " },
    { 12, "endAddrsCoarseOffset", "32k samples" },
    { 13, "modLfoToVolume", "cB fs" },
    { 14, "unused1", "" },
    { 15, "chorusEffectsSend", "0.1%" },
    { 16, "reverbEffectsSend", "0.1% " },
    { 17, "pan", "0.1%" },
    { 18, "unused2", "" },
    { 19, "unused3", "" },
    { 20, "unused4", "" },
    { 21, "delayModLFO", "timecent" },
    { 22, "freqModLFO", "cent 8.176 " },
    { 23, "delayVibLFO", "timecent " },
    { 24, "freqVibLFO", "cent 8.176" },
    { 25, "delayModEnv", "timecent" },
    { 26, "attackModEnv", "timecent " },
    { 27, "holdModEnv", "timecent" },
    { 28, "decayModEnv", "timecent" },
    { 29, "sustainModEnv", "-0.1%" },
    { 30, "releaseModEnv", "timecent" },
    { 31, "keynumToModEnvHold", "tcent/key" },
    { 32, "keynumToModEnvDecay", "tcent/key" },
    { 33, "delayVolEnv", "timecent" },
    { 34, "attackVolEnv", "timecent" },
    { 35, "holdVolEnv", "timecent" },
    { 36, "decayVolEnv", "timecent" },
    { 37, "sustainVolEnv", "cB" },
    { 38, "releaseVolEnv", "timecent " },
    { 39, "keynumToVolEnvHold", "tcent/key" },
    { 40, "keynumToVolEnvDecay", "tcent/key " },
    { 41, "instrument", "" },
    { 42, "reserved1", "" },
    { 43, "keyRange MIDI", "" },
    { 44, "velRange MIDI", "" },
    { 45, "startloopAddrsCoarseOffset", "samples" },
    { 46, "keynum MIDI", "" },
    { 47, "velocity MIDI", "" },
    { 48, "initialAttenuation", "cB" },
    { 49, "reserved2", "" },
    { 50, "endloopAddrsCoarseOffset", "samples" },
    { 51, "coarseTune", "semitone" },
    { 52, "fineTune", "cent" },
    { 53, "sampleId", "" },
    { 54, "sampleModes", "Bit Flags" },
    { 55, "reserved3", "" },
    { 56, "scaleTuning", "cent/key" },
    { 57, "exclusiveClass", "arbitrary#" },
    { 58, "unused5", "" },
    { 59, "unused6", "" },
    { 60, "unused7", "" },
    { 61, "unused8", "" },
    { 62, "unused9", "" },
    { 63, "unused10", "" }
};
// clang-format on

/***************************************************************
 *
 *  dsp
 *
 */

void fluidmax_dsp64(t_fluidmax* x, t_object* dsp64, short* count,
                    double samplerate, long maxvectorsize, long flags)
{
    post("my sample rate is: %f", samplerate);

    // instead of calling dsp_add(), we send the "dsp_add64" message to the
    // object representing the dsp chain the arguments passed are: 1: the dsp64
    // object passed-in by the calling function 2: the symbol of the
    // "dsp_add64" message we are sending 3: a pointer to your object 4: a
    // pointer to your 64-bit perform method 5: flags to alter how the signal
    // chain handles your object -- just pass 0 6: a generic pointer that you
    // can use to pass any additional data to your perform method

    object_method(dsp64, gensym("dsp_add64"), x, fluidmax_perform64, 0, NULL);
}


// this is the 64-bit perform method audio vectors
void fluidmax_perform64(t_fluidmax* x, t_object* dsp64, double** ins,
                        long numins, double** outs, long numouts,
                        long sampleframes, long flags, void* userparam)
{
    t_double* outL = outs[0]; // we get audio for each outlet of the object
                              // from the **outs argument
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

/***************************************************************
 *
 *  load utlilities
 *
 */
char* fluidmax_translate_fullpath(char* maxpath, char* fullpath)
{
    int i;

    strcpy(fullpath, "/Volumes/");

    for (i = 0; maxpath[i] != ':'; i++)
        fullpath[i + 9] = maxpath[i];

    /* skip ':' */
    i++;

    strcpy(fullpath + i + 8, maxpath + i);

    return fullpath;
}

t_symbol* fluidmax_get_stripped_name(const char* fullpath)
{
    char stripped[1024];
    int i;

    for (i = strlen(fullpath) - 1; i >= 0; i--) {
        if (fullpath[i] == '/')
            break;
    }

    if (i != 0)
        i++;

    strcpy(stripped, fullpath + i);

    for (i = 0; stripped[i] != '\0'; i++) {
        if ((stripped[i] == '.')
            && (stripped[i + 1] == 's' || stripped[i + 1] == 'S')
            && (stripped[i + 2] == 'f' || stripped[i + 2] == 'F')
            && (stripped[i + 3] == '2')) {
            stripped[i] = '\0';
            break;
        }
    }

    return gensym(stripped);
}

t_symbol* fluidmax_sfont_get_name(fluid_sfont_t* sfont)
{
    return fluidmax_get_stripped_name(fluid_sfont_get_name(sfont));
}

fluid_sfont_t* fluidmax_sfont_get_by_name(t_fluidmax* x, t_symbol* name)
{
    int n = fluid_synth_sfcount(x->synth);

    for (int i = 0; i < n; i++) {
        fluid_sfont_t* sfont = fluid_synth_get_sfont(x->synth, i);

        if (fluidmax_sfont_get_name(sfont) == name)
            return sfont;
    }

    return NULL;
}

void fluidmax_do_load(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv)
{
    if (argc > 0 && is_symbol(argv)) {
        const char* filename = atom_getsym(argv)->s_name;
        t_symbol* name = fluidmax_get_stripped_name(filename);
        fluid_sfont_t* sf = fluidmax_sfont_get_by_name(x, name);

        if (sf == NULL) {
            int id = fluid_synth_sfload(x->synth, filename, 0);

            if (id >= 0) {
                post("fluidsynth~: loaded soundfont '%s' (id %d)",
                     name->s_name, id);

                fluid_synth_program_reset(x->synth);

                outlet_bang(x->outlet);
            } else
                error("fluidsynth~: cannot load soundfont from file '%s'",
                      filename);
        } else {
            error("fluidsynth~: soundfont named '%s' is already loaded",
                  name->s_name);
            return;
        }
    }
}

void fluidmax_load_with_dialog(t_fluidmax* x, t_symbol* s, short argc,
                               t_atom* argv)
{
    char filename[256];
    char maxpath[1024];
    char fullpath[1024];
    long type;
    short path;

    open_promptset("open SoundFont 2 file");

    if (open_dialog(filename, &path, &type, 0, 0))
        return;

    if (path_topotentialname(path, filename, maxpath, 0) == 0) {
        t_atom a;

        atom_setsym(&a, gensym(fluidmax_translate_fullpath(maxpath, fullpath)));
        fluidmax_do_load(x, NULL, 1, &a);
    }
}

/***************************************************************
 *
 *  user methods
 *
 */
void fluidmax_load(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv)
{
    if (argc == 0)
        defer(x, (method)fluidmax_load_with_dialog, NULL, 0, 0);
    else {
        if (is_symbol(argv)) {
            t_symbol* name = atom_getsym(argv);
            char* string = (char*)name->s_name;

            if (string[0] == '/')
                defer(x, (method)fluidmax_do_load, NULL, argc, argv);
            else {
                char maxpath[1024];
                char fullpath[1024];
                short path;
                long type;
                t_atom a;

                if (locatefile_extended(string, &path, &type, 0, 0)
                    || path_topotentialname(path, string, maxpath, 0) != 0) {
                    error("fluidsynth~: cannot find file '%s'", string);
                    return;
                }

                atom_setsym(&a, gensym(fluidmax_translate_fullpath(maxpath, fullpath)));
                defer(x, (method)fluidmax_do_load, NULL, 1, &a);
            }
        }
    }
}

void fluidmax_unload(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv)
{
    if (argc > 0) {
        if (is_number(argv)) {
            int id = get_number_as_long(argv);
            fluid_sfont_t* sf = fluid_synth_get_sfont_by_id(x->synth, id);

            if (sf != NULL) {
                t_symbol* name = fluidmax_sfont_get_name(sf);

                if (fluid_synth_sfunload(x->synth, id, 0) >= 0) {
                    post("fluidsynth~: unloaded soundfont '%s' (id %d)",
                         name->s_name, id);
                    return;
                }
            }

            error("fluidsynth~: cannot unload soundfont %d", id);
        } else if (is_symbol(argv)) {
            t_symbol* sym = atom_getsym(argv);

            if (sym == gensym("all")) {
                fluid_sfont_t* sf = fluid_synth_get_sfont(x->synth, 0);

                fluid_synth_system_reset(x->synth);

                while (sf != NULL) {
                    t_symbol* name = fluidmax_sfont_get_name(sf);
                    unsigned int id = fluid_sfont_get_id(sf);

                    if (fluid_synth_sfunload(x->synth, id, 0) >= 0)
                        post("fluidsynth~: unloaded soundfont '%s' (id %d)",
                             name->s_name, id);
                    else
                        error("fluidsynth~: cannot unload soundfont '%s' (id "
                              "%d)",
                              name->s_name, id);

                    sf = fluid_synth_get_sfont(x->synth, 0);
                }
            } else {
                fluid_sfont_t* sf = fluidmax_sfont_get_by_name(x, sym);

                if (sf != NULL) {
                    unsigned int id = fluid_sfont_get_id(sf);

                    if (fluid_synth_sfunload(x->synth, id, 0) >= 0) {
                        post("fluidsynth~: unloaded soundfont '%s' (id %d)",
                             sym->s_name, id);
                        return;
                    }
                }

                error("fluidsynth~: cannot unload soundfont '%s'", sym->s_name);
            }
        }
    }
}

void fluidmax_reload(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv)
{
    if (argc > 0) {
        if (is_number(argv)) {
            int id = get_number_as_long(argv);
            fluid_sfont_t* sf = fluid_synth_get_sfont_by_id(x->synth, id);

            if (sf != NULL) {
                if (fluid_synth_sfreload(x->synth, id) >= 0) {
                    post("fluidsynth~: reloaded soundfont '%s' (id %d)", id);
                    return;
                }

                error("fluidsynth~: cannot reload soundfont %d", id);
            }
        } else if (is_symbol(argv)) {
            t_symbol* sym = atom_getsym(argv);

            if (sym == gensym("all")) {
                int n = fluid_synth_sfcount(x->synth);
                int i;

                fluid_synth_system_reset(x->synth);

                for (i = 0; i < n; i++) {
                    fluid_sfont_t* sf = fluid_synth_get_sfont(x->synth, i);
                    t_symbol* name = fluidmax_sfont_get_name(sf);
                    unsigned int id = fluid_sfont_get_id(sf);


                    if (fluid_synth_sfreload(x->synth, id) >= 0)
                        post("fluidsynth~: reloaded soundfont '%s' (id %d)",
                             name->s_name, id);
                    else
                        error("fluidsynth~: cannot reload soundfont '%s' (id %d)",
                              name->s_name, id);
                }
            } else {
                fluid_sfont_t* sf = fluidmax_sfont_get_by_name(x, sym);

                if (sf != NULL) {
                    unsigned int id = fluid_sfont_get_id(sf);

                    if (fluid_synth_sfreload(x->synth, id) >= 0) {
                        post("fluidsynth~: reloaded soundfont '%s' (id %d)",
                             sym->s_name, id);
                        return;
                    }
                }

                error("fluidsynth~: cannot reload soundfont '%s'",
                      sym->s_name);
            }
        }
    }
}

void fluidmax_note(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv)
{
    if (argc > 0 && is_number(argv)) {
        int velocity = 64;
        int channel = 1;

        switch (argc) {
        default:
        case 3:
            if (is_number(argv + 2)) {
                channel = get_number_as_long(argv + 2);

                if (channel < 1)
                    channel = 1;
                else if (channel > fluid_synth_count_midi_channels(x->synth))
                    channel = fluid_synth_count_midi_channels(x->synth);
            }
        case 2:
            if (is_number(argv + 1))
                velocity = get_number_as_long(argv + 1);
        case 1:
            fluid_synth_noteon(x->synth, channel - 1, get_number_as_long(argv),
                               velocity);
        case 0:
            break;
        }
    }
}

void fluidmax_list(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv)
{
    fluidmax_note(x, NULL, argc, argv);
}


void fluidmax_control_change(t_fluidmax* x, t_symbol* s, short argc,
                             t_atom* argv)
{

    if (argc > 0 && is_number(argv)) {
        int value = 64;
        int channel = 1;

        switch (argc) {
        default:
        case 3:
            if (is_number(argv + 2)) {
                channel = get_number_as_long(argv + 2);

                if (channel < 1)
                    channel = 1;
                else if (channel > fluid_synth_count_midi_channels(x->synth))
                    channel = fluid_synth_count_midi_channels(x->synth);
            }
        case 2:
            if (is_number(argv + 1))
                value = get_number_as_long(argv + 1);
        case 1:
            fluid_synth_cc(x->synth, channel - 1, get_number_as_long(argv),
                           value);
        case 0:
            break;
        }
    }
}

void fluidmax_mod(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv)
{

    if (argc > 1 && is_number(argv) && is_number(argv + 1)) {
        int param = get_number_as_long(argv);
        float value = get_number_as_float(argv + 1);
        int channel = 1;

        if (argc > 2 && is_number(argv + 2)) {
            channel = get_number_as_long(argv + 2);

            if (channel < 1)
                channel = 1;
            else if (channel > fluid_synth_count_midi_channels(x->synth))
                channel = fluid_synth_count_midi_channels(x->synth);
        }

        fluid_synth_set_gen(x->synth, channel - 1, param, value);
    }
}

void fluidmax_pitch_bend(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv)
{
    if (argc > 0 && is_number(argv)) {
        int channel = 1;
        double bend = 0.0;

        if (argc > 1 && is_number(argv + 1)) {
            channel = get_number_as_long(argv + 1);

            if (channel < 1)
                channel = 1;
            else if (channel > fluid_synth_count_midi_channels(x->synth))
                channel = fluid_synth_count_midi_channels(x->synth);
        }

        bend = get_number_as_float(argv);

        if (bend < 0.0)
            bend = 0.0;
        else if (bend > 127.0)
            bend = 127.0;

        fluid_synth_pitch_bend(x->synth, channel - 1, (int)(bend * 128.0));
    }
}

void fluidmax_pitch_bend_wheel(t_fluidmax* x, t_symbol* s, short argc,
                               t_atom* argv)
{
    if (argc > 0 && is_number(argv)) {
        int channel = 1;

        if (argc > 1 && is_number(argv + 1))
            channel = get_number_as_long(argv + 1);

        fluid_synth_pitch_wheel_sens(x->synth, channel - 1,
                                     get_number_as_long(argv));
    }
}

void fluidmax_program_change(t_fluidmax* x, t_symbol* s, short argc,
                             t_atom* argv)
{
    if (argc > 0 && is_number(argv)) {
        int channel = 1;

        if (argc > 1 && is_number(argv + 1)) {
            channel = get_number_as_long(argv + 1);

            if (channel < 1)
                channel = 1;
            else if (channel > fluid_synth_count_midi_channels(x->synth))
                channel = fluid_synth_count_midi_channels(x->synth);
        }

        fluid_synth_program_change(x->synth, channel - 1,
                                   get_number_as_long(argv));
    }
}

void fluidmax_bank_select(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv)
{
    if (argc > 0 && is_number(argv)) {
        int channel = 1;
        unsigned int sf_id;
        unsigned int bank_num;
        unsigned int prog_num;

        if (argc > 1 && is_number(argv + 1)) {
            channel = get_number_as_long(argv + 1);

            if (channel < 1)
                channel = 1;
            else if (channel > fluid_synth_count_midi_channels(x->synth))
                channel = fluid_synth_count_midi_channels(x->synth);
        }

        fluid_synth_bank_select(x->synth, channel - 1,
                                get_number_as_long(argv));
        fluid_synth_get_program(x->synth, channel - 1, &sf_id, &bank_num,
                                &prog_num);
        fluid_synth_program_change(x->synth, channel - 1, prog_num);
    }
}

void fluidmax_select(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv)
{
    unsigned int bank = 0;
    unsigned int preset = 0;
    int channel = 1;

    switch (argc) {
    default:
    case 4:
        if (is_number(argv + 3))
            channel = get_number_as_long(argv + 3);

        if (channel < 1)
            channel = 1;
        else if (channel > fluid_synth_count_midi_channels(x->synth))
            channel = fluid_synth_count_midi_channels(x->synth);

    case 3:
        if (is_number(argv + 2))
            preset = get_number_as_long(argv + 2);

    case 2:
        if (is_number(argv + 1))
            bank = get_number_as_long(argv + 1);

    case 1:
        if (is_number(argv))
            fluid_synth_program_select(x->synth, channel - 1,
                                       get_number_as_long(argv), bank, preset);
        else if (is_symbol(argv)) {
            t_symbol* name = atom_getsym(argv);
            fluid_sfont_t* sf = fluidmax_sfont_get_by_name(x, name);

            if (sf != NULL)
                fluid_synth_program_select(x->synth, channel - 1,
                                           fluid_sfont_get_id(sf), bank,
                                           preset);
            else
                error("fluidsynth~ select: cannot find soundfont named '%s'",
                      name->s_name);
        }
    case 0:
        break;
    }
}

void fluidmax_reverb(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv)
{
    int res;
    double roomsize, damping, width, level;

    if (argc == 0) {
        // fluid_synth_set_reverb_on(x->synth, 1);
        fluid_synth_reverb_on(x->synth, -1, 1);
        // fluid_synth_reset_reverb(x->synth);
        x->reverb = 1;
    } else if (is_number(argv)) {
        res = fluid_synth_get_reverb_group_roomsize(x->synth, -1, &roomsize);
        if (res != FLUID_OK) {
            object_error((t_object*)x, "could not get reverb group roomsize");
            goto exception;
        }
        res = fluid_synth_get_reverb_group_damp(x->synth, -1, &damping);
        if (res != FLUID_OK) {
            object_error((t_object*)x, "could not get reverb group damping value");
            goto exception;
        }
        res = fluid_synth_get_reverb_group_width(x->synth, -1, &width);
        if (res != FLUID_OK) {
            object_error((t_object*)x, "could not get reverb group width value");
            goto exception;
        }
        res = fluid_synth_get_reverb_group_level(x->synth, -1, &level);
        if (res != FLUID_OK) {
            object_error((t_object*)x, "could not get reverb group level");
            goto exception;
        }

        // fluid_synth_set_reverb_on(x->synth, 1);
        fluid_synth_reverb_on(x->synth, -1, 1);
        x->reverb = 1;

        switch (argc) {
        default:
        case 4:
            if (is_number(argv + 3))
                width = get_number_as_float(argv + 3);
        case 3:
            if (is_number(argv + 2))
                damping = get_number_as_float(argv + 2);
        case 2:
            if (is_number(argv + 1))
                roomsize = get_number_as_float(argv + 1);
        case 1: 
            {
            level = get_number_as_float(argv);
            res = fluid_synth_set_reverb_group_roomsize(x->synth, -1, roomsize);
            if (res != FLUID_OK) {
                object_error((t_object*)x, "could not set reverb group roomsize");
                goto exception;
            }
            res = fluid_synth_set_reverb_group_damp(x->synth, -1, damping);
            if (res != FLUID_OK) {
                object_error((t_object*)x, "could not set reverb group damping value");
                goto exception;
            }
            res = fluid_synth_set_reverb_group_width(x->synth, -1, width);
            if (res != FLUID_OK) {
                object_error((t_object*)x, "could not set reverb group width value");
                goto exception;
            }
            res = fluid_synth_set_reverb_group_level(x->synth, -1, level);
            if (res != FLUID_OK) {
                object_error((t_object*)x, "could not set reverb group level");
                goto exception;
            }
            }
        //     fluid_synth_set_reverb(x->synth, room, damping, width,
        //                            get_number_as_float(argv));
        // case 0:
            break;
        }
    } else if (is_symbol(argv)) {
        t_symbol* sym = atom_getsym(argv);

        if (sym == gensym("on")) {
            // fluid_synth_set_reverb_on(x->synth, 1);
            fluid_synth_reverb_on(x->synth, -1, 1);
            x->reverb = 1;
        } else if (sym == gensym("off")) {
            // fluid_synth_set_reverb_on(x->synth, 0);
            fluid_synth_reverb_on(x->synth, -1, 0);
            x->reverb = 0;
        }
    }
    return;

exception:
    object_error((t_object*)x, "could not get/set reverb value");
}

void fluidmax_chorus(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv)
{
    int res, nr, type;
    double level, speed, depth_ms;

    if (argc == 0) {
        // fluid_synth_set_chorus_on(x->synth, 1);
        fluid_synth_chorus_on(x->synth, -1, 1);
        // fluid_synth_reset_chorus(x->synth);
        x->chorus = 1;
    } else if (is_number(argv)) {

        res = fluid_synth_get_chorus_group_speed(x->synth, -1, &speed);
        if (res != FLUID_OK) {
            object_error((t_object*)x, "could not get chorus group speed");
            goto exception;
        }
        res = fluid_synth_get_chorus_group_depth(x->synth, -1, &depth_ms);
        if (res != FLUID_OK) {
            object_error((t_object*)x, "could not get chorus group depth_ms value");
            goto exception;
        }
        res = fluid_synth_get_chorus_group_type(x->synth, -1, &type);
        if (res != FLUID_OK) {
            object_error((t_object*)x, "could not get chorus group type value");
            goto exception;
        }
        res = fluid_synth_get_chorus_group_nr(x->synth, -1, &nr);
        if (res != FLUID_OK) {
            object_error((t_object*)x, "could not get chorus group nr value");
            goto exception;
        }
        res = fluid_synth_get_chorus_group_level(x->synth, -1, &level);
        if (res != FLUID_OK) {
            object_error((t_object*)x, "could not get chorus group level");
            goto exception;
        }

        fluid_synth_chorus_on(x->synth, -1, 1);
        x->chorus = 1;

        switch (argc) {
        default:
        case 5:
            if (is_number(argv + 4))
                nr = get_number_as_long(argv + 4);
        case 4:
            if (is_number(argv + 3))
                type = get_number_as_long(argv + 3);
        case 3:
            if (is_number(argv + 2))
                depth_ms = get_number_as_float(argv + 2);
        case 2:
            if (is_number(argv + 1))
                speed = get_number_as_float(argv + 1);
        case 1:
            {
            // fluid_synth_set_chorus(x->synth, nr, get_number_as_float(argv),
            //                        speed, depth, type);

            level = get_number_as_float(argv);

            res = fluid_synth_set_chorus_group_nr(x->synth, -1, nr);
            if (res != FLUID_OK) {
                object_error((t_object*)x, "could not set chorus group nr");
                goto exception;
            }
            res = fluid_synth_set_chorus_group_level(x->synth, -1, level);
            if (res != FLUID_OK) {
                object_error((t_object*)x, "could not set chorus group level");
                goto exception;
            }
            res = fluid_synth_set_chorus_group_speed(x->synth, -1, speed);
            if (res != FLUID_OK) {
                object_error((t_object*)x, "could not set chorus group speed");
                goto exception;
            }
            res = fluid_synth_set_chorus_group_depth(x->synth, -1, depth_ms);
            if (res != FLUID_OK) {
                object_error((t_object*)x, "could not set chorus group depth_ms");
                goto exception;
            }
            res = fluid_synth_set_chorus_group_type(x->synth, -1, type);
            if (res != FLUID_OK) {
                object_error((t_object*)x, "could not set chorus group type");
                goto exception;
            }
            }
        case 0:
            break;
        }
    } else if (is_symbol(argv)) {
        t_symbol* sym = atom_getsym(argv);

        if (sym == gensym("on")) {
            fluid_synth_chorus_on(x->synth, -1, 1);
            x->chorus = 1;
        } else if (sym == gensym("off")) {
            fluid_synth_chorus_on(x->synth, -1, 0);
            x->chorus = 0;
        }
    }
    return;

exception:
    object_error((t_object*)x, "could not get/set chorus value");
}

void fluidmax_set_gain(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv)
{

    if (argc > 0 && is_number(argv)) {
        float gain = get_number_as_float(argv);

        fluid_synth_set_gain(x->synth, gain);
    }
}

void fluidmax_set_resampling_method(t_fluidmax* x, t_symbol* s, short argc,
                                    t_atom* argv)
{

    if (argc > 0) {
        if (is_number(argv)) {
            int ip = atom_getlong(argv);

            if (ip == 0)
                fluid_synth_set_interp_method(x->synth, -1, FLUID_INTERP_NONE);
            else if (ip < 3)
                fluid_synth_set_interp_method(x->synth, -1,
                                              FLUID_INTERP_LINEAR);
            else if (ip < 6)
                fluid_synth_set_interp_method(x->synth, -1,
                                              FLUID_INTERP_4THORDER);
            else
                fluid_synth_set_interp_method(x->synth, -1,
                                              FLUID_INTERP_7THORDER);
        } else if (is_symbol(argv)) {
            t_symbol* sym = atom_getsym(argv);

            if (sym == gensym("nearest"))
                fluid_synth_set_interp_method(x->synth, -1, FLUID_INTERP_NONE);
            else if (sym == gensym("linear"))
                fluid_synth_set_interp_method(x->synth, -1,
                                              FLUID_INTERP_LINEAR);
            else if (sym == gensym("cubic"))
                fluid_synth_set_interp_method(x->synth, -1,
                                              FLUID_INTERP_4THORDER);
            else if (sym == gensym("sinc"))
                fluid_synth_set_interp_method(x->synth, -1,
                                              FLUID_INTERP_7THORDER);
            else
                error("fluidsynth~: undefined resampling method: %s",
                      sym->s_name);
        }
    }
}

void fluidmax_panic(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv)
{

    fluid_synth_system_reset(x->synth);
}

void fluidmax_reset(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv)
{
    int n = fluid_synth_count_midi_channels(x->synth);
    int i;

    // for (i = 0; i < n; i++)
    //     fluid_channel_reset(x->synth->channel[i]);
}

void fluidmax_mute(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv)
{
    int mute = 1;

    if (argc > 0 && is_number(argv))
        mute = (get_number_as_long(argv) != 0);

    fluid_synth_system_reset(x->synth);

    x->mute = mute;
}

void fluidmax_unmute(t_fluidmax* x)
{
    t_atom a;

    atom_setlong(&a, 0);
    fluidmax_mute(x, NULL, 1, &a);
}

/*
int fluid_synth_count_audio_channels (fluid_synth_t *synth)
int fluid_synth_count_audio_groups (fluid_synth_t *synth)
int fluid_synth_count_effects_channels (fluid_synth_t *synth)
*/

void fluidmax_tuning_octave(t_fluidmax* x, t_symbol* s, short argc,
                            t_atom* argv)
{
    t_symbol* name;
    int tuning_bank = 0;
    int tuning_prog = 0;
    double pitch[12];
    int i, n;

    if (argc > 0 && is_symbol(argv)) {
        name = atom_getsym(argv);
        argv++;
        argc--;
    }

    n = argc - 2;
    if (n > 12)
        n = 12;

    if (argc > 0 && is_number(argv))
        tuning_bank = get_number_as_long(argv) % 128;

    if (argc > 1 && is_number(argv + 1))
        tuning_prog = get_number_as_long(argv) % 128;

    for (i = 0; i < n; i++) {
        if (is_number(argv + i + 2))
            pitch[i] = get_number_as_float(argv + i + 2);
        else
            pitch[i] = 0.0;
    }

    for (; i < 12; n++)
        pitch[i] = 0.0;

    // fluid_synth_create_octave_tuning(x->synth, tuning_bank, tuning_prog,
    //                                  name->s_name, pitch);
}

void fluidmax_tuning_select(t_fluidmax* x, t_symbol* s, short argc,
                            t_atom* argv)
{
    int tuning_bank = 0;
    int tuning_prog = 0;
    int channel = 1;

    if (argc > 0 && is_number(argv))
        tuning_bank = get_number_as_long(argv) % 128;

    if (argc > 1 && is_number(argv + 1))
        tuning_prog = get_number_as_long(argv + 1) % 128;

    if (argc > 2 && is_number(argv + 2))
        channel = get_number_as_long(argv + 2);

    if (channel < 1)
        channel = 1;
    else if (channel > fluid_synth_count_midi_channels(x->synth))
        channel = fluid_synth_count_midi_channels(x->synth);

    // fluid_synth_select_tuning(x->synth, channel - 1, tuning_bank, tuning_prog);
}

void fluidmax_tuning_reset(t_fluidmax* x, t_symbol* s, short argc,
                           t_atom* argv)
{
    int channel = 0;

    if (argc > 0 && is_number(argv))
        channel = get_number_as_long(argv);

    if (channel < 1)
        channel = 1;
    else if (channel > fluid_synth_count_midi_channels(x->synth))
        channel = fluid_synth_count_midi_channels(x->synth);

    // fluid_synth_reset_tuning(x->synth, channel - 1);
}

/* more tuning ??
fluid_synth_create_key_tuning (fluid_synth_t *synth, int tuning_bank, int
tuning_prog, char *name, double *pitch) fluid_synth_tune_notes (fluid_synth_t
*synth, int tuning_bank, int tuning_prog, int len, int *keys, double *pitch,
int apply) fluid_synth_tuning_iteration_start (fluid_synth_t *synth)
fluid_synth_tuning_iteration_next (fluid_synth_t *synth, int *bank, int *prog)
fluid_synth_tuning_dump (fluid_synth_t *synth, int bank, int prog, char *name,
int len, double *pitch)
*/

void fluidmax_version(t_object* o)
{
    post("fluidsynth~, version %s (based on FluidSynth %s)", FLUIDMAX_VERSION,
         FLUIDSYNTH_VERSION);
    post("  FluidSynth is written by Peter Hanappe et al. (see "
         "fluidsynth.org)");
    post("  Max/MSP integration by Norbert Schnell IMTR IRCAM - Centre "
         "Pompidou");
}

// extern fluid_gen_info_t fluid_gen_info[];

void fluidmax_print(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv)
{

    if (argc > 0) {
        if (is_symbol(argv)) {
            t_symbol* sym = atom_getsym(argv);

            if (sym == gensym("soundfonts")) {
                int n = fluid_synth_sfcount(x->synth);
                int i;

                if (n > 0)
                    post("fluidsynth~ soundfonts:");
                else
                    post("fluidsynth~: no soundfonts loaded");

                for (i = 0; i < n; i++) {
                    fluid_sfont_t* sf = fluid_synth_get_sfont(x->synth, i);
                    t_symbol* name = fluidmax_sfont_get_name(sf);
                    unsigned int id = fluid_sfont_get_id(sf);

                    post("  %d: '%s' (id %d)", i, name->s_name, id);
                }
            } else if (sym == gensym("presets")) {
                int n = fluid_synth_sfcount(x->synth);

                if (n > 0) {
                    if (argc > 1) {
                        fluid_sfont_t* sf = NULL;
                        t_symbol* name;

                        if (is_symbol(argv + 1)) {
                            name = atom_getsym(argv + 1);
                            sf = fluidmax_sfont_get_by_name(x, name);
                        } else if (is_long(argv + 1)) {
                            int id = atom_getlong(argv + 1);

                            sf = fluid_synth_get_sfont_by_id(x->synth, id);
                            name = fluidmax_sfont_get_name(sf);
                        }

                        if (sf != NULL) {
                            // fluid_preset_t preset;

                            // fluid_sfont_iteration_start(sf);

                            // post("fluidsynth~ presets of soundfont '%s':",
                            //      name->s_name);

                            // while (fluid_sfont_iteration_next(sf, &preset)
                            //        > 0) {
                            //     char* preset_str = fluid_preset_get_name(
                            //         &preset);
                            //     t_symbol* preset_name = gensym(preset_str);
                            //     int bank_num = fluid_preset_get_banknum(
                            //         &preset);
                            //     int prog_num = fluid_preset_get_num(&preset);

                            //     post("  '%s': bank %d, program %d",
                            //          preset_name->s_name, bank_num, prog_num);
                            // }
                        }
                    } else {
                        int i;

                        post("fluidsynth~ presets:");

                        for (i = 0; i < 128; i++) {
                            int j;

                            for (j = 0; j < 128; j++) {
                                fluid_preset_t* preset = NULL;
                                fluid_sfont_t* sf = NULL;
                                int k;

                                for (k = 0; k < n; k++) {
                                    sf = fluid_synth_get_sfont(x->synth, k);
                                    preset = fluid_sfont_get_preset(sf, i, j);

                                    if (preset != NULL)
                                        break;
                                }

                                if (preset != NULL) {
                                    t_symbol* sf_name
                                        = fluidmax_sfont_get_name(sf);
                                    char* preset_str = fluid_preset_get_name(
                                        preset);
                                    t_symbol* preset_name = gensym(preset_str);

                                    post("  '%s': soundfont '%s', bank %d, "
                                         "program %d",
                                         preset_name->s_name, sf_name->s_name,
                                         i, j);
                                }
                            }
                        }
                    }
                } else
                    error("fluidsynth~: no soundfonts loaded");
            } else if (sym == gensym("channels")) {
                int n = fluid_synth_count_midi_channels(x->synth);
                int i;

                post("fluidsynth~ channels:");

                for (i = 0; i < n; i++) {
                    fluid_preset_t* preset = fluid_synth_get_channel_preset(
                        x->synth, i);

                    if (preset != NULL) {
                        char* preset_str = fluid_preset_get_name(preset);
                        t_symbol* preset_name = gensym(preset_str);
                        unsigned int sf_id;
                        unsigned int bank_num;
                        unsigned int prog_num;
                        fluid_sfont_t* sf;

                        fluid_synth_get_program(x->synth, i, &sf_id, &bank_num,
                                                &prog_num);
                        sf = fluid_synth_get_sfont_by_id(x->synth, sf_id);

                        post("  %d: soundfont '%s', bank %d, program %d: '%s'",
                             i + 1, fluidmax_sfont_get_name(sf)->s_name,
                             bank_num, prog_num, preset_name->s_name);
                    } else
                        post("  channel %d: no preset", i + 1);
                }
            } else if (sym == gensym("generators")) {
                // int channel = 1;
                // int n = GEN_LAST;
                // int i;

                // if (argc > 1 && is_number(argv + 1))
                //     channel = get_number_as_long(argv + 1);

                // if (channel < 1)
                //     channel = 1;
                // else if (channel > fluid_synth_count_midi_channels(x->synth))
                //     channel = fluid_synth_count_midi_channels(x->synth);

                // post("fluidsynth~ generators of channel %d:", channel);

                // for (i = 0; i < n; i++) {
                //     const char* name = fluidmax_gen_info[i].name;
                //     const char* unit = fluidmax_gen_info[i].unit;
                //     double incr = fluid_synth_get_gen(x->synth, channel - 1,
                //                                       i);
                //     double min = fluid_gen_info[i].min;
                //     double max = fluid_gen_info[i].max;

                //     post("  %d '%s': %s %g [%g ... %g] (%s)", i, name,
                //          (incr >= 0) ? "" : "-", fabs(incr), min, max, unit);
                // }
            } else if (sym == gensym("gain")) {
                double gain = fluid_synth_get_gain(x->synth);

                post("gain: %g", gain);
            } else if (sym == gensym("reverb")) {
                double level, roomsize, damping, width;
                fluid_synth_get_reverb_group_roomsize(x->synth, -1, &roomsize);
                fluid_synth_get_reverb_group_damp(x->synth, -1, &damping);
                fluid_synth_get_reverb_group_level(x->synth, -1, &level);
                fluid_synth_get_reverb_group_width(x->synth, -1, &width);

                if (x->reverb != 0) {
                    post("fluidsynth~ current reverb parameters:");
                    post("  level: %f", level);
                    post("  room size: %f", roomsize);
                    post("  damping: %f", damping);
                    post("  width: %f", width);
                } else
                    post("fluidsynth~: reverb off");
            } else if (sym == gensym("chorus")) {
                if (x->chorus != 0) {
                    double level, speed, depth;
                    int type, nr;
                    fluid_synth_get_chorus_group_speed(x->synth, -1, &speed);
                    fluid_synth_get_chorus_group_level(x->synth, -1, &level);
                    fluid_synth_get_chorus_group_depth(x->synth, -1, &depth);
                    fluid_synth_get_chorus_group_nr(x->synth, -1, &nr);
                    fluid_synth_get_chorus_group_type(x->synth, -1, &type);

                    post("fluidsynth~ current chorus parameters:");
                    post("  level: %f", level);
                    post("  speed: %f Hz", speed);
                    post("  depth: %f msec", depth);
                    post("  type: %d (%s)", type, type ? "triangle" : "sine");
                    post("  %d units", nr);
                } else
                    post("fluidsynth~: chorus off");
            }
        }
    }
}

void fluidmax_info(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv)
{

    if (argc > 0) {
        if (is_symbol(argv)) {
            t_symbol* sym = atom_getsym(argv);

            if (sym == gensym("soundfonts")) {
                int n = fluid_synth_sfcount(x->synth);
                int i;

                for (i = 0; i < n; i++) {
                    fluid_sfont_t* sf = fluid_synth_get_sfont(x->synth, i);
                    unsigned int id = fluid_sfont_get_id(sf);
                    t_atom a[2];

                    atom_setlong(a, i);
                    atom_setsym(a + 1, fluidmax_sfont_get_name(sf));
                    atom_setlong(a + 2, id);
                    outlet_anything(x->outlet, gensym("soundfont"), 3, a);
                }
            } else if (sym == gensym("presets")) {
                int n = fluid_synth_sfcount(x->synth);

                if (n > 0) {
                    if (argc > 1) {
                        fluid_sfont_t* sf = NULL;
                        t_symbol* sf_name;

                        if (is_symbol(argv + 1)) {
                            sf_name = atom_getsym(argv + 1);
                            sf = fluidmax_sfont_get_by_name(x, sf_name);
                        } else if (is_long(argv + 1)) {
                            int id = atom_getlong(argv + 1);

                            sf = fluid_synth_get_sfont_by_id(x->synth, id);
                            sf_name = fluidmax_sfont_get_name(sf);
                        }

                        if (sf != NULL) {
                            // fluid_preset_t preset;

                            // fluid_sfont_iteration_start(sf);

                            // while (fluid_sfont_iteration_next(sf, &preset)
                            //        > 0) {
                            //     char* preset_str = fluid_preset_get_name(
                            //         &preset);
                            //     t_symbol* preset_name = gensym(preset_str);
                            //     int bank_num = fluid_preset_get_banknum(
                            //         &preset);
                            //     int prog_num = fluid_preset_get_num(&preset);
                            //     t_atom a[4];

                            //     atom_setsym(a, preset_name);
                            //     atom_setsym(a + 1, sf_name);
                            //     atom_setlong(a + 2, bank_num);
                            //     atom_setlong(a + 3, prog_num);
                            //     outlet_anything(x->outlet, gensym("preset"), 4,
                            //                     a);
                            // }
                        }
                    } else {
                        int i;

                        for (i = 0; i < 128; i++) {
                            int j;

                            for (j = 0; j < 128; j++) {
                                fluid_preset_t* preset = NULL;
                                fluid_sfont_t* sf = NULL;
                                int k;

                                for (k = 0; k < n; k++) {
                                    sf = fluid_synth_get_sfont(x->synth, k);
                                    preset = fluid_sfont_get_preset(sf, i, j);

                                    if (preset != NULL)
                                        break;
                                }

                                if (preset != NULL) {
                                    t_symbol* sf_name
                                        = fluidmax_sfont_get_name(sf);
                                    char* preset_str = fluid_preset_get_name(
                                        preset);
                                    t_symbol* preset_name = gensym(preset_str);
                                    t_atom a[4];

                                    atom_setsym(a, preset_name);
                                    atom_setsym(a + 1, sf_name);
                                    atom_setlong(a + 2, i);
                                    atom_setlong(a + 3, j);
                                    outlet_anything(x->outlet,
                                                    gensym("preset"), 4, a);
                                }
                            }
                        }
                    }
                } else
                    error("fluidsynth~ info: no soundfonts loaded");
            } else if (sym == gensym("channels")) {
                int n = fluid_synth_count_midi_channels(x->synth);
                int i;

                for (i = 0; i < n; i++) {
                    fluid_preset_t* preset = fluid_synth_get_channel_preset(
                        x->synth, i);

                    if (preset != NULL) {
                        char* preset_str = fluid_preset_get_name(preset);
                        t_symbol* preset_name = gensym(preset_str);
                        unsigned int sf_id, bank_num, prog_num;
                        fluid_sfont_t* sf;
                        t_atom a[5];

                        fluid_synth_get_program(x->synth, i, &sf_id, &bank_num,
                                                &prog_num);
                        sf = fluid_synth_get_sfont_by_id(x->synth, sf_id);

                        atom_setlong(a, i + 1);
                        atom_setsym(a + 1, fluidmax_sfont_get_name(sf));
                        atom_setlong(a + 2, bank_num);
                        atom_setlong(a + 3, prog_num);
                        atom_setsym(a + 4, preset_name);
                        outlet_anything(x->outlet, gensym("channel"), 5, a);
                    } else {
                        t_atom a[2];

                        atom_setlong(a, i + 1);
                        atom_setsym(a + 1, gensym("undefined"));
                        outlet_anything(x->outlet, gensym("channel"), 2, a);
                    }
                }
            } else if (sym == gensym("gain")) {
                t_atom a;
                double gain = fluid_synth_get_gain(x->synth);

                atom_setfloat(&a, gain);
                outlet_anything(x->outlet, gensym("channel"), 1, &a);
            } else if (sym == gensym("reverb")) {
                if (x->reverb != 0) {
                    double level, roomsize, damping, width;
                    fluid_synth_get_reverb_group_roomsize(x->synth, -1, &roomsize);
                    fluid_synth_get_reverb_group_damp(x->synth, -1, &damping);
                    fluid_synth_get_reverb_group_level(x->synth, -1, &level);
                    fluid_synth_get_reverb_group_width(x->synth, -1, &width);

                    // double level = fluid_synth_get_reverb_level(x->synth);
                    // double room = fluid_synth_get_reverb_roomsize(x->synth);
                    // double damping = fluid_synth_get_reverb_damp(x->synth);
                    // double width = fluid_synth_get_reverb_width(x->synth);
                    t_atom a[4];

                    atom_setfloat(a, level);
                    atom_setfloat(a + 1, roomsize);
                    atom_setfloat(a + 2, damping);
                    atom_setfloat(a + 3, width);
                    outlet_anything(x->outlet, gensym("reverb"), 4, a);
                } else {
                    t_atom a;

                    atom_setsym(&a, gensym("off"));
                    outlet_anything(x->outlet, gensym("reverb"), 1, &a);
                }
            } else if (sym == gensym("chorus")) {
                if (x->chorus != 0) {
                    double level, speed, depth;
                    int type, nr;
                    fluid_synth_get_chorus_group_speed(x->synth, -1, &speed);
                    fluid_synth_get_chorus_group_level(x->synth, -1, &level);
                    fluid_synth_get_chorus_group_depth(x->synth, -1, &depth);
                    fluid_synth_get_chorus_group_nr(x->synth, -1, &nr);
                    fluid_synth_get_chorus_group_type(x->synth, -1, &type);

                    t_atom a[5];

                    atom_setfloat(a, level);
                    atom_setfloat(a + 1, speed);
                    atom_setfloat(a + 2, depth);
                    atom_setlong(a + 3, type);
                    atom_setlong(a + 4, nr);
                    outlet_anything(x->outlet, gensym("chorus"), 5, a);
                } else {
                    t_atom a;

                    atom_setsym(&a, gensym("off"));
                    outlet_anything(x->outlet, gensym("chorus"), 1, &a);
                }
            } else if (sym == gensym("polyphony")) {
                int polyphony = fluid_synth_get_polyphony(x->synth);
                t_atom a;

                atom_setlong(&a, polyphony);
                outlet_anything(x->outlet, gensym("polyphony"), 1, &a);
            }
        }
    }
}

/***************************************************************
 *
 *  class
 *
 */
void* fluidmax_new(t_symbol* s, long argc, t_atom* argv)
{
    t_fluidmax* x = (t_fluidmax*)object_alloc(fluidmax_class);
    int polyphony = 256;
    int midi_channels = 16;

    x->outlet = outlet_new(x, "anything");

    dsp_setup((t_pxobject*)x, 0);
    outlet_new(x, "signal");
    outlet_new(x, "signal");

    x->synth = NULL;
    x->settings = new_fluid_settings();
    x->reverb = 0;
    x->chorus = 0;
    x->mute = 0;

    if (argc > 0 && is_number(argv)) {
        polyphony = get_number_as_long(argv);
        argc--;
        argv++;
    }

    if (argc > 0 && is_number(argv)) {
        midi_channels = get_number_as_long(argv);
        argc--;
        argv++;
    }

    if (argc > 0 && is_symbol(argv)) {
        fluidmax_load(x, NULL, 1, argv);
    }

    if (x->settings != NULL) {
        fluid_settings_setint(x->settings, "synth.midi-channels",
                              midi_channels);
        fluid_settings_setint(x->settings, "synth.polyphony", polyphony);
        fluid_settings_setnum(x->settings, "synth.gain", 0.600000);
        fluid_settings_setnum(x->settings, "synth.sample-rate", sys_getsr());

        x->synth = new_fluid_synth(x->settings);

        if (x->synth != NULL) {
            // fluid_synth_set_reverb_on(x->synth, 0);
            // fluid_synth_set_chorus_on(x->synth, 0);

            if (argc > 0 && is_symbol(argv))
                fluidmax_load(x, NULL, argc, argv);

            return x;
        }

        delete_fluid_settings(x->settings);
    }

    error("fluidsynth~: cannot create FluidSynth core");

    return NULL;
}

void fluidmax_free(t_fluidmax* x)
{
    if (x->settings != NULL)
        delete_fluid_settings(x->settings);

    if (x->synth != NULL)
        delete_fluid_synth(x->synth);

    dsp_free((t_pxobject*)x);
}

// clang-format off
void ext_main(void* r)
{
    t_class* c = class_new("fluidmax~", 
        (method)fluidmax_new, 
        (method)fluidmax_free, 
        (long)sizeof(t_fluidmax), 0L, A_GIMME, 0);

    class_addmethod(c, (method)fluidmax_dsp64, "dsp64", A_CANT, 0);
    // class_addmethod(c, (method)fluidmax_assist, "assist", A_CANT, 0);

    class_addmethod(c, (method)fluidmax_version, "version", 0);
    class_addmethod(c, (method)fluidmax_print, "print", A_GIMME, 0);

    class_addmethod(c, (method)fluidmax_load, "load", A_GIMME, 0);
    class_addmethod(c, (method)fluidmax_unload, "unload", A_GIMME, 0);
    class_addmethod(c, (method)fluidmax_reload, "reload", A_GIMME, 0);
    class_addmethod(c, (method)fluidmax_info, "info", A_GIMME, 0);

    class_addmethod(c, (method)fluidmax_panic, "panic", A_GIMME, 0);
    class_addmethod(c, (method)fluidmax_reset, "reset", A_GIMME, 0);
    class_addmethod(c, (method)fluidmax_mute, "mute", A_GIMME, 0);
    class_addmethod(c, (method)fluidmax_unmute, "unmute", 0);

    /*class_addmethod(c, (method)fluidmax_tuning_keys, "tuning-keys", A_GIMME, 0);*/
    class_addmethod(c, (method)fluidmax_tuning_octave, "tuning-octave", A_GIMME, 0);
    class_addmethod(c, (method)fluidmax_tuning_select, "tuning-select", A_GIMME, 0);
    class_addmethod(c, (method)fluidmax_tuning_reset, "tuning-reset", A_GIMME, 0);

    class_addmethod(c, (method)fluidmax_reverb, "reverb", A_GIMME, 0);
    class_addmethod(c, (method)fluidmax_chorus, "chorus", A_GIMME, 0);
    class_addmethod(c, (method)fluidmax_set_gain, "gain", A_GIMME, 0);
    class_addmethod(c, (method)fluidmax_set_resampling_method, "resample", A_GIMME, 0);

    class_addmethod(c, (method)fluidmax_note, "note", A_GIMME, 0);
    class_addmethod(c, (method)fluidmax_list, "list", A_GIMME, 0);

    class_addmethod(c, (method)fluidmax_control_change, "control", A_GIMME, 0);
    class_addmethod(c, (method)fluidmax_mod, "mod", A_GIMME, 0);

    class_addmethod(c, (method)fluidmax_pitch_bend, "bend", A_GIMME, 0);
    class_addmethod(c, (method)fluidmax_pitch_bend_wheel, "wheel", A_GIMME, 0);

    class_addmethod(c, (method)fluidmax_program_change, "program", A_GIMME, 0);
    class_addmethod(c, (method)fluidmax_bank_select, "bank", A_GIMME, 0);
    class_addmethod(c, (method)fluidmax_select, "select", A_GIMME, 0);

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    fluidmax_class = c;
}
// clang-format on
