#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"

#include <unistd.h>
#include <fcntl.h>

#include <fluidsynth.h>

#define fm_error(x, ...) object_error((t_object*)(x), __VA_ARGS__);
#define fm_post(x, ...) object_post((t_object*)(x), __VA_ARGS__);

#define MAX_COMMAND_LEN 2048
#define FM_MAX_ATOMS 1024

typedef struct _fm
{
    t_pxobject ob; // the object itself (t_pxobject in MSP instead of t_object)

    t_symbol* name;
    fluid_settings_t *settings;
    fluid_synth_t *synth;
    fluid_player_t *player;
    fluid_midi_router_t *router;
    fluid_midi_driver_t *mdriver;
    fluid_cmd_handler_t *cmd_handler;
    float * left_buffer;
    float * right_buffer;
    long out_maxsize;
    int sfont_id;
    int mute;
} t_fm;

// method prototypes
void* fm_new(t_symbol* s, long argc, t_atom* argv);
void fm_init(t_fm* x);
void fm_free(t_fm* x);
void fm_assist(t_fm* x, void* b, long m, long a, char* s);
void fm_bang(t_fm* x);
void fm_mute(t_fm* x);
t_symbol* fm_locate_path_from_symbol(t_fm* x, t_symbol* s);
void fm_load(t_fm* x, t_symbol* sfont);
void fm_play(t_fm* x, t_symbol* midifile);
void fm_stop(t_fm*);
void fm_note(t_fm* x, t_symbol* s, short argc, t_atom* argv);
// void fm_prog(t_fm* x, t_symbol* s, short argc, t_atom* argv);
t_max_err fm_anything(t_fm* x, t_symbol* s, short argc, t_atom* argv);
void fm_dsp64(t_fm* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags);
void fm_perform64(t_fm* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam);

// global class pointer variable
static t_class* fm_class = NULL;

//-----------------------------------------------------------------------------
// helpers

int is_long(t_atom* a) { return atom_gettype(a) == A_LONG; }
int is_float(t_atom* a) { return atom_gettype(a) == A_FLOAT; }
int is_number(t_atom* a) { return is_long(a) || is_float(a); }
int is_symbol(t_atom* a) { return atom_gettype(a) == A_SYM; }

int get_number_as_int(t_atom* a)
{
    if (!is_number(a)) {
        error("atom is not a number");
        return -1;
    }
    return is_long(a) ? (int)atom_getlong(a) : (int)atom_getfloat(a);
}

double get_number_as_float(t_atom* a)
{
    if (!is_number(a)) {
        error("atom is not a number");
        return -1.0;
    }
    return is_float(a) ? atom_getfloat(a) : (double)atom_getlong(a);
}

//-----------------------------------------------------------------------------
// main

void ext_main(void* r)
{
    t_class* c = class_new("fs4m~", (method)fm_new, (method)fm_free, (long)sizeof(t_fm), 0L, A_GIMME, 0);

    class_addmethod(c, (method)fm_bang,     "bang",     0);
    class_addmethod(c, (method)fm_mute,     "mute",     0);
    class_addmethod(c, (method)fm_stop,     "stop",     0);
    class_addmethod(c, (method)fm_play,     "play",     A_SYM,   0);
    class_addmethod(c, (method)fm_note,     "note",     A_GIMME, 0);
    // class_addmethod(c, (method)fm_prog,     "prog",     A_GIMME, 0);
    class_addmethod(c, (method)fm_load,     "load",     A_SYM,   0);
    class_addmethod(c, (method)fm_dsp64,    "dsp64",    A_CANT,  0);
    class_addmethod(c, (method)fm_assist,   "assist",   A_CANT,  0);
    class_addmethod(c, (method)fm_anything, "anything", A_GIMME, 0);

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    fm_class = c;
}

void* fm_new(t_symbol* s, long argc, t_atom* argv)
{
    t_fm* x = (t_fm*)object_alloc(fm_class);

    if (x) {
        dsp_setup((t_pxobject*)x, 0); // use 0 if you don't need signal inlets
        outlet_new((t_pxobject*)x, "signal");
        outlet_new((t_pxobject*)x, "signal");
        x->name = symbol_unique();
        x->settings = NULL;
        x->synth = NULL;
        x->player = NULL;
        x->router = NULL;
        x->mdriver = NULL;
        x->cmd_handler = NULL;
        x->left_buffer = NULL;
        x->right_buffer = NULL;
        x->out_maxsize = 0;
        x->mute = 0;
        fm_init(x);
    }
    return (x);
}

void fm_init(t_fm* x)
{
    x->settings = new_fluid_settings();
    if(x->settings == NULL) {
        fm_error(x, "Failed to create the settings!");
        return;
    }

    fluid_settings_setnum(x->settings, "synth.sample-rate", sys_getsr());
    fluid_settings_setint(x->settings, "synth.midi-channels", 16);
    fluid_settings_setint(x->settings, "synth.polyphony", 256);
    fluid_settings_setnum(x->settings, "synth.gain", 0.600000);

    x->synth = new_fluid_synth(x->settings);
    if(x->synth == NULL) {
        fm_error(x, "Failed to create the synth!");
        return;
    }

    x->router = new_fluid_midi_router(x->settings, fluid_synth_handle_midi_event, (void *)x->synth);
    if(x->router == NULL) {
        fm_error(x, "Creating midi router failed.");
        return;
    }

    x->mdriver = new_fluid_midi_driver(x->settings, fluid_midi_router_handle_midi_event, (void *)x->router);
    if(x->mdriver == NULL) {
        fm_error(x, "Creating midi driver failed.");
        return;
    }

    x->player  = new_fluid_player(x->synth);
    if(x->player == NULL) {
        fm_error(x, "Creating midi player failed.");
        return;
    }
    fluid_player_set_playback_callback(x->player, fluid_midi_router_handle_midi_event, x->router);

    x->cmd_handler = new_fluid_cmd_handler2(x->settings, x->synth, x->router, x->player);
    if(x->cmd_handler == NULL) {
        fm_error(x, "Creating cmd_handler failed.");
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
    if (x->cmd_handler != NULL)
        delete_fluid_cmd_handler(x->cmd_handler);

    if (x->player != NULL)
        delete_fluid_player(x->player);

    if (x->mdriver != NULL)
        delete_fluid_midi_driver(x->mdriver);

    if (x->router != NULL)
        delete_fluid_midi_router(x->router);

    if (x->synth != NULL)
        delete_fluid_synth(x->synth);

    if (x->settings != NULL)
        delete_fluid_settings(x->settings);

    if(x->left_buffer != NULL)
        sysmem_freeptr(x->left_buffer);
        x->left_buffer = NULL;
  
    if(x->right_buffer != NULL)
        sysmem_freeptr(x->right_buffer);
        x->right_buffer = NULL;
  
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

t_symbol* fm_locate_path_from_symbol(t_fm* x, t_symbol* s)
{
    char filename[MAX_PATH_CHARS];
    char pathname[MAX_PATH_CHARS];
    short path;
    t_fourcc type;
    t_max_err err;

    strncpy_zero(filename, s->s_name, MAX_PATH_CHARS);
    if (locatefile_extended(filename, &path, &type, NULL, 0)) {
        // nozero: not found
        error("can't find file %s", s->s_name);
        return gensym("");
    }

    pathname[0] = 0;
    err = path_toabsolutesystempath(path, filename, pathname);
    if (err != MAX_ERR_NONE) {
        error("can't convert %s to absolutepath", s->s_name);
        return gensym("");
    }
    // post("full path is: %s", pathname->s_name);
    return gensym(pathname);
}

void fm_load(t_fm* x, t_symbol* sfont)
{
    t_symbol* sfont_path = fm_locate_path_from_symbol(x, sfont);
    x->sfont_id = fluid_synth_sfload(x->synth, sfont_path->s_name, 1);
    if(x->sfont_id == FLUID_FAILED)
    {
        fm_error(x, "Loading SoundFont '%s' failed!", sfont->s_name);
    }
}

void fm_play(t_fm* x, t_symbol* midifile)
{
    t_symbol* midifile_path = fm_locate_path_from_symbol(x, midifile);
    fm_post(x, "playing %s", midifile->s_name);
    fluid_player_add(x->player, midifile_path->s_name);
    fluid_player_play(x->player);
}

void fm_stop(t_fm* x)
{
    fluid_player_stop(x->player);
}


// void fm_prog(t_fm* x, t_symbol* s, short argc, t_atom* argv)
// {
//     if (argc != 2 || !is_number(argv)) {
//         fm_error(x, "needs two args: prog <channel> <program>");
//         return;
//     }
//     int channel = get_number_as_int(argv);
//     int program = get_number_as_int(argv + 1);
//     int res = fluid_synth_program_change(x->synth, channel, program);
//     if (res != FLUID_OK) {
//         fm_error(x, "program change failed.");
//         return;
//     }
//     fm_post(x, "prog %d %d", channel, program);
// }


void fm_note(t_fm* x, t_symbol* s, short argc, t_atom* argv)
{
    if (argc > 0 && is_number(argv)) {
        int velocity = 64;
        int channel = 1;

        switch (argc) {
        default:
        case 3:
            if (is_number(argv + 2)) {
                channel = get_number_as_int(argv + 2);

                if (channel < 1)
                    channel = 1;
                else if (channel > fluid_synth_count_midi_channels(x->synth))
                    channel = fluid_synth_count_midi_channels(x->synth);
            }
        case 2:
            if (is_number(argv + 1))
                velocity = get_number_as_int(argv + 1);
        case 1:
            fluid_synth_noteon(x->synth, channel - 1, get_number_as_int(argv),
                               velocity);
        case 0:
            break;
        }
    }
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

void do_random_notes(t_fm* x, t_symbol* s, short argc, t_atom* argv)
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
        sleep(1);

        /* Stop the note */
        fluid_synth_noteoff(x->synth, 0, key);
    }
}

void fm_bang(t_fm* x)
{
    defer_low((t_object*)x, (method)do_random_notes, NULL, 0, NULL);
}

t_max_err fm_anything(t_fm* x, t_symbol* s, short argc, t_atom* argv)
{
    t_atom atoms[FM_MAX_ATOMS];
    long textsize = 0;
    char* text = NULL;

    if (s == gensym("")) {
        return MAX_ERR_GENERIC;
    }

    // set symbol as first atom in new atoms array
    atom_setsym(atoms, s);

    for (int i = 0; i < argc; i++) {
        switch ((argv + i)->a_type) {
        case A_FLOAT: {
            atom_setfloat((atoms + (i + 1)), atom_getfloat(argv + i));
            break;
        }
        case A_LONG: {
            atom_setlong((atoms + (i + 1)), atom_getlong(argv + i));
            break;
        }
        case A_SYM: {
            atom_setsym((atoms + (i + 1)), atom_getsym(argv + i));
            break;
        }
        default:
            fm_error(x, "cannot process unknown type");
            break;
        }
    }

    t_max_err err = atom_gettext(argc+1, atoms, &textsize, &text,
                                 // OBEX_UTIL_ATOM_GETTEXT_FORCE_ZEROS);
                                 OBEX_UTIL_ATOM_GETTEXT_DEFAULT);
    if (err != MAX_ERR_NONE && !textsize && !text) {
        goto error;
    }

    // post("cmd: '%s' size: %d", text, strlen(text));
    post("cmd: %s", text);

    char filename[MAX_FILENAME_CHARS];
    snprintf_zero(filename, MAX_FILENAME_CHARS, "/tmp/temp.%s", x->name->s_name);

    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    int res = fluid_command(x->cmd_handler, text, (fluid_ostream_t)fd);
    // int res = fluid_command(x->cmd_handler, text, (fluid_ostream_t)fd);
    if (res != FLUID_OK) {
        fm_error(x, "cmd failed: '%s'");
    }
    close(fd);

    fd = open(filename, O_RDONLY);
    if(fd == -1) {
        fm_error(x, "error opening file: %s", filename);
        goto cleanup;
    } 

    char buffer[MAX_COMMAND_LEN];
    memset(buffer, 0, MAX_COMMAND_LEN);
    size_t nbytes = sizeof(buffer);
    ssize_t bytes_read = read(fd, buffer, nbytes);
    close(fd);

    // post("bytes read: %zd", bytes_read);
    if(bytes_read)
        post("%s", buffer);

cleanup:
    sysmem_freeptr(text);
    return MAX_ERR_NONE;

error:
    return MAX_ERR_GENERIC;
}



void fm_dsp64(t_fm* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags)
{
    fluid_settings_setnum(x->settings, "synth.sample-rate", samplerate);

    if(x->out_maxsize < maxvectorsize) {
        fm_post(x, "buffers allocated");

        // sysmem_freeptr(x->left_buffer);
        // sysmem_freeptr(x->right_buffer);

        // x->left_buffer = (float*)sysmem_newptrclear(sizeof(float) * maxvectorsize);
        // x->right_buffer = (float*)sysmem_newptrclear(sizeof(float) * maxvectorsize);

        x->left_buffer = (float*)sysmem_resizeptrclear(x->left_buffer, sizeof(float) * maxvectorsize);
        x->right_buffer = (float*)sysmem_resizeptrclear(x->right_buffer, sizeof(float) * maxvectorsize);

        memset(x->left_buffer, 0.f, sizeof(float) * maxvectorsize);
        memset(x->right_buffer, 0.f, sizeof(float) * maxvectorsize);

        x->out_maxsize = maxvectorsize;
    }

    object_method(dsp64, gensym("dsp_add64"), x, fm_perform64, 0, NULL);
}

void fm_perform64(t_fm* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam)
{
    double* left_out = outs[0];
    double* right_out = outs[1];
    float* left_buf = x->left_buffer;
    float* right_buf = x->right_buffer;
    int n = (int)sampleframes;
    int err = 0;

    if (x->mute == 0) {
        err = fluid_synth_write_float(x->synth, n, left_buf, 0, 1, right_buf, 0, 1);
        if(err == FLUID_FAILED)
            error("Problem writing samples");

        for (int i = 0; i < n; i++) {
            left_out[i] = left_buf[i];
            right_out[i] = right_buf[i];
        }

    } else {
        for (int i = 0; i < n; i++) {
            left_out[i] = right_out[i] = 0.0;
        }
    }
}


// void fm_perform64(t_fm* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam)
// {
//     double* left_out = outs[0];
//     double* right_out = outs[1];
//     float* dry[2];
//     dry[0] = x->left_buffer;
//     dry[1] = x->right_buffer;
//     int n = (int)sampleframes;
//     int err = 0;

//     if (x->mute == 0) {
//         err = fluid_synth_process(x->synth, n, 0, NULL, 2, dry);
//         if(err == FLUID_FAILED)
//             error("Problem writing samples");

//         for (int i = 0; i < n; i++) {
//             left_out[i] = dry[0][i];
//             right_out[i] = dry[1][i];
//         }

//         memset(x->left_buffer, 0.f, sizeof(float) * x->out_maxsize);
//         memset(x->right_buffer, 0.f, sizeof(float) * x->out_maxsize);

//     } else {
//         for (int i = 0; i < n; i++) {
//             left_out[i] = right_out[i]= 0.0;
//         }
//     }
// }
