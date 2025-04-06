#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"

#include <unistd.h>
#include <fcntl.h>

#include <fluidsynth.h>

#define fsm_error(x, ...) object_error((t_object*)(x), __VA_ARGS__);
#define fsm_post(x, ...) object_post((t_object*)(x), __VA_ARGS__);

#define MAX_COMMAND_LEN 2048
#define FSM_MAX_ATOMS 1024

typedef struct _fsm
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
} t_fsm;

// method prototypes
void* fsm_new(t_symbol* s, long argc, t_atom* argv);
void fsm_init(t_fsm* x);
void fsm_free(t_fsm* x);
void fsm_assist(t_fsm* x, void* b, long m, long a, char* s);
void fsm_bang(t_fsm* x);
void fsm_mute(t_fsm* x);
t_symbol* fsm_locate_path_from_symbol(t_fsm* x, t_symbol* s);
void fsm_load(t_fsm* x, t_symbol* sfont);
void fsm_play(t_fsm* x, t_symbol* midifile);
void fsm_stop(t_fsm*);
// void fsm_note(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
t_max_err fsm_anything(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_dsp64(t_fsm* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags);
void fsm_perform64(t_fsm* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam);

// global class pointer variable
static t_class* fsm_class = NULL;

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
    t_class* c = class_new("fsm~", (method)fsm_new, (method)fsm_free, (long)sizeof(t_fsm), 0L, A_GIMME, 0);

    class_addmethod(c, (method)fsm_bang,     "bang",     0);
    class_addmethod(c, (method)fsm_mute,     "mute",     0);
    class_addmethod(c, (method)fsm_stop,     "stop",     0);
    class_addmethod(c, (method)fsm_play,     "play",     A_SYM,   0);
    class_addmethod(c, (method)fsm_load,     "load",     A_SYM,   0);
    class_addmethod(c, (method)fsm_dsp64,    "dsp64",    A_CANT,  0);
    class_addmethod(c, (method)fsm_assist,   "assist",   A_CANT,  0);
    class_addmethod(c, (method)fsm_anything, "anything", A_GIMME, 0);

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    fsm_class = c;
}

void* fsm_new(t_symbol* s, long argc, t_atom* argv)
{
    t_fsm* x = (t_fsm*)object_alloc(fsm_class);

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
        fsm_init(x);
    }
    return (x);
}

void fsm_init(t_fsm* x)
{
    x->settings = new_fluid_settings();
    if(x->settings == NULL) {
        fsm_error(x, "Failed to create the settings!");
        return;
    }

    fluid_settings_setnum(x->settings, "synth.sample-rate", sys_getsr());
    fluid_settings_setint(x->settings, "synth.midi-channels", 16);
    fluid_settings_setint(x->settings, "synth.polyphony", 256);
    fluid_settings_setnum(x->settings, "synth.gain", 0.600000);

    x->synth = new_fluid_synth(x->settings);
    if(x->synth == NULL) {
        fsm_error(x, "Failed to create the synth!");
        return;
    }

    x->router = new_fluid_midi_router(x->settings, fluid_synth_handle_midi_event, (void *)x->synth);
    if(x->router == NULL) {
        fsm_error(x, "Creating midi router failed.");
        return;
    }

    x->mdriver = new_fluid_midi_driver(x->settings, fluid_midi_router_handle_midi_event, (void *)x->router);
    if(x->mdriver == NULL) {
        fsm_error(x, "Creating midi driver failed.");
        return;
    }

    x->player  = new_fluid_player(x->synth);
    if(x->player == NULL) {
        fsm_error(x, "Creating midi player failed.");
        return;
    }
    fluid_player_set_playback_callback(x->player, fluid_midi_router_handle_midi_event, x->router);

    x->cmd_handler = new_fluid_cmd_handler2(x->settings, x->synth, x->router, x->player);
    if(x->cmd_handler == NULL) {
        fsm_error(x, "Creating cmd_handler failed.");
        return;
    }
    fsm_post(x, "fluidsynth initialized");
}

void fsm_free(t_fsm* x)
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

void fsm_assist(t_fsm* x, void* b, long m, long a, char* s)
{
    if (m == ASSIST_INLET) { // inlet
        sprintf(s, "I am inlet %ld", a);
    }
    else { // outlet
        sprintf(s, "I am outlet %ld", a);
    }
}

t_symbol* fsm_locate_path_from_symbol(t_fsm* x, t_symbol* s)
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

void fsm_load(t_fsm* x, t_symbol* sfont)
{
    t_symbol* sfont_path = fsm_locate_path_from_symbol(x, sfont);
    x->sfont_id = fluid_synth_sfload(x->synth, sfont_path->s_name, 1);
    if(x->sfont_id == FLUID_FAILED)
    {
        fsm_error(x, "Loading SoundFont '%s' failed!", sfont->s_name);
    }
}

void fsm_play(t_fsm* x, t_symbol* midifile)
{
    t_symbol* midifile_path = fsm_locate_path_from_symbol(x, midifile);
    fsm_post(x, "playing %s", midifile->s_name);
    fluid_player_add(x->player, midifile_path->s_name);
    fluid_player_play(x->player);
}

void fsm_stop(t_fsm* x)
{
    fluid_player_stop(x->player);
}

void fsm_mute(t_fsm* x)
{
    if (x->mute == 0) {
        x->mute = 1;
        post("mute == 1");
    } else {
        x->mute = 0;
        post("mute == 0");
    }
}


void fsm_bang(t_fsm* x)
{
    
}

t_max_err fsm_anything(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    t_atom atoms[FSM_MAX_ATOMS];
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
            fsm_error(x, "cannot process unknown type");
            break;
        }
    }

    t_max_err err = atom_gettext(argc+1, atoms, &textsize, &text,
                                 OBEX_UTIL_ATOM_GETTEXT_DEFAULT);
    if (err != MAX_ERR_NONE && !textsize && !text) {
        fsm_error(x, "conversion to text failed: '%s'");
        goto error;
    }

    // post("cmd: '%s' size: %d", text, strlen(text));
    post("cmd: %s", text);

    char filename[MAX_FILENAME_CHARS];
    snprintf_zero(filename, MAX_FILENAME_CHARS, "/tmp/temp.%s", x->name->s_name);

    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    int res = fluid_command(x->cmd_handler, text, (fluid_ostream_t)fd);
    close(fd);

    if (res == -1) {
        fsm_error(x, "command error: '%s'", text);
        goto error;
    } else if (res == 1) {
        fsm_error(x, "command is empty");
        goto error;
    } else if (res == -2) {
        fsm_error(x, "quit command");
        goto cleanup;
    }

    fd = open(filename, O_RDONLY);
    if(fd == -1) {
        fsm_error(x, "error opening file: %s", filename);
        goto error;
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
    sysmem_freeptr(text);
    return MAX_ERR_GENERIC;
}



void fsm_dsp64(t_fsm* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags)
{
    fluid_settings_setnum(x->settings, "synth.sample-rate", samplerate);

    if(x->out_maxsize < maxvectorsize) {
        fsm_post(x, "buffers allocated");

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

    object_method(dsp64, gensym("dsp_add64"), x, fsm_perform64, 0, NULL);
}

void fsm_perform64(t_fsm* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam)
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
