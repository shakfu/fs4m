#include "fsm_tilde.h"
#include "fsm_tilde_gen.h"

/*----------------------------------------------------------------------------*/
// core struct

struct t_fsm {
    t_pxobject obj;

    fluid_synth_t* synth;
    fluid_settings_t* settings;
    int reverb;
    int chorus;
    int mute;
    double* left_buffer;
    double* right_buffer;
    long out_maxsize;
    void* outlet;
};

/*----------------------------------------------------------------------------*/
// secondary fx structs

typedef struct {
    int fx_group;
    double level;
    double roomsize;
    double damping;
    double width;
} t_fsm_fx_reverb;

typedef struct {
    int fx_group;
    double level;
    double speed;
    double depth_ms;
    int nr;
    int type;
} t_fsm_fx_chorus;

/*----------------------------------------------------------------------------*/
// dsp

void fsm_dsp64(t_fsm* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags)
{
    fluid_settings_setnum(x->settings, "synth.sample-rate", samplerate);

    if(x->out_maxsize < maxvectorsize) {
        sysmem_freeptr(x->left_buffer);
        sysmem_freeptr(x->right_buffer);

        x->left_buffer = (double*)sysmem_newptr(sizeof(double) * maxvectorsize);
        x->right_buffer = (double*)sysmem_newptr(sizeof(double) * maxvectorsize);

        memset(x->left_buffer, 0.f, sizeof(double) * maxvectorsize);
        memset(x->right_buffer, 0.f, sizeof(double) * maxvectorsize);

        x->out_maxsize = maxvectorsize;
    }

    object_method(dsp64, gensym("dsp_add64"), x, fsm_perform64, 0, NULL);
}


void fsm_perform64(t_fsm* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam)
{
    double* left_out = outs[0];
    double* right_out = outs[1];
    int n = (int)sampleframes;

    if (x->mute == 0) {
        
        fluid_synth_write_float(x->synth, n, x->left_buffer, 0, 1, x->right_buffer, 0, 1);

        for (int i = 0; i < n; i++) {
            left_out[i] = x->left_buffer[i];
            right_out[i] = x->right_buffer[i];
        }

    } else {
        for (int i = 0; i < n; i++) {
            left_out[i] = right_out[i]= 0.0;
        }
    }
}

/*----------------------------------------------------------------------------*/
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

/*----------------------------------------------------------------------------*/
// load utilities

char* fsm_translate_fullpath(char* maxpath, char* fullpath)
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

t_symbol* fsm_get_stripped_name(const char* fullpath)
{
    char stripped[MAX_PATH_CHARS];
    int i;

    for (i = (int)strlen(fullpath) - 1; i >= 0; i--) {
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

t_symbol* fsm_sfont_get_name(fluid_sfont_t* sfont)
{
    return fsm_get_stripped_name(fluid_sfont_get_name(sfont));
}

fluid_sfont_t* fsm_sfont_get_by_name(t_fsm* x, t_symbol* name)
{
    int n = fluid_synth_sfcount(x->synth);

    for (int i = 0; i < n; i++) {
        fluid_sfont_t* sfont = fluid_synth_get_sfont(x->synth, i);

        if (fsm_sfont_get_name(sfont) == name)
            return sfont;
    }

    return NULL;
}

void fsm_do_load(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    if (argc > 0 && is_symbol(argv)) {
        const char* filename = atom_getsym(argv)->s_name;
        t_symbol* name = fsm_get_stripped_name(filename);
        fluid_sfont_t* sf = fsm_sfont_get_by_name(x, name);

        if (sf == NULL) {
            int id = fluid_synth_sfload(x->synth, filename, 0);

            if (id >= 0) {
                fsm_post(x, "loaded soundfont '%s' (id %d)", name->s_name, id);

                fluid_synth_program_reset(x->synth);

                outlet_bang(x->outlet);
            } else
                fsm_error(x, "cannot load soundfont from file '%s'", filename);
        } else {
            fsm_error(x, "soundfont named '%s' is already loaded",
                      name->s_name);
        }
    }
}

void fsm_load_with_dialog(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    char filename[MAX_FILENAME_CHARS];
    char maxpath[MAX_PATH_CHARS];
    char fullpath[MAX_PATH_CHARS];
    t_fourcc type;
    short path;

    open_promptset("open SoundFont 2 file");

    if (open_dialog(filename, &path, &type, 0, 0))
        return;

    if (path_topotentialname(path, filename, maxpath, 0) == 0) {
        t_atom a;

        atom_setsym(&a, gensym(fsm_translate_fullpath(maxpath, fullpath)));
        fsm_do_load(x, NULL, 1, &a);
    }
}

/*----------------------------------------------------------------------------*/
// user methods

void fsm_load(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    if (argc == 0)
        defer(x, (method)fsm_load_with_dialog, NULL, 0, 0);
    else {
        if (is_symbol(argv)) {
            t_symbol* name = atom_getsym(argv);
            char* string = (char*)name->s_name;

            if (string[0] == '/')
                defer(x, (method)fsm_do_load, NULL, argc, argv);
            else {
                char maxpath[MAX_PATH_CHARS];
                char fullpath[MAX_PATH_CHARS];
                short path;
                t_fourcc type;
                t_atom a;

                if (locatefile_extended(string, &path, &type, 0, 0)
                    || path_topotentialname(path, string, maxpath, 0) != 0) {
                    fsm_error(x, "cannot find file '%s'", string);
                    return;
                }

                atom_setsym(&a,
                            gensym(fsm_translate_fullpath(maxpath, fullpath)));
                defer(x, (method)fsm_do_load, NULL, 1, &a);
            }
        }
    }
}

void fsm_unload(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    if (argc == 0) {
        fsm_error(x, "no soundfont specified");
        return;
    }

    if (is_number(argv)) {
        int id = get_number_as_int(argv);
        fluid_sfont_t* sf = fluid_synth_get_sfont_by_id(x->synth, id);

        if (sf != NULL) {
            t_symbol* name = fsm_sfont_get_name(sf);

            if (fluid_synth_sfunload(x->synth, id, 0) >= 0) {
                fsm_post(x, "unloaded soundfont '%s' (id %d)", name->s_name,
                         id);
                return;
            }
        }
        fsm_error(x, "cannot unload soundfont %d", id);

    } else if (is_symbol(argv)) {
        t_symbol* sym = atom_getsym(argv);

        if (sym == gensym("all")) {
            fluid_sfont_t* sf = fluid_synth_get_sfont(x->synth, 0);

            fluid_synth_system_reset(x->synth);

            while (sf != NULL) {
                t_symbol* name = fsm_sfont_get_name(sf);
                unsigned int id = fluid_sfont_get_id(sf);

                if (fluid_synth_sfunload(x->synth, id, 0) >= 0) {
                    fsm_post(x, "unloaded soundfont '%s' (id %d)",
                             name->s_name, id);
                } else {
                    fsm_error(x, "cannot unload soundfont '%s' (id %d)",
                              name->s_name, id);
                }

                sf = fluid_synth_get_sfont(x->synth, 0);
            }
        } else {
            fluid_sfont_t* sf = fsm_sfont_get_by_name(x, sym);

            if (sf != NULL) {
                unsigned int id = fluid_sfont_get_id(sf);

                if (fluid_synth_sfunload(x->synth, id, 0) >= 0) {
                    fsm_post(x, "unloaded soundfont '%s' (id %d)", sym->s_name,
                             id);
                    return;
                }
            }

            fsm_error(x, "cannot unload soundfont '%s'", sym->s_name);
        }
    }
}

void fsm_reload(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    if (argc > 0) {
        if (is_number(argv)) {
            int id = get_number_as_int(argv);
            fluid_sfont_t* sf = fluid_synth_get_sfont_by_id(x->synth, id);

            if (sf != NULL) {
                if (fluid_synth_sfreload(x->synth, id) >= 0) {
                    fsm_post(x, "reloaded soundfont '%s' (id %d)", id);
                    return;
                }

                fsm_error(x, "cannot reload soundfont %d", id);
            }
        } else if (is_symbol(argv)) {
            t_symbol* sym = atom_getsym(argv);

            if (sym == gensym("all")) {
                int n = fluid_synth_sfcount(x->synth);
                int i;

                fluid_synth_system_reset(x->synth);

                for (i = 0; i < n; i++) {
                    fluid_sfont_t* sf = fluid_synth_get_sfont(x->synth, i);
                    t_symbol* name = fsm_sfont_get_name(sf);
                    unsigned int id = fluid_sfont_get_id(sf);


                    if (fluid_synth_sfreload(x->synth, id) >= 0) {
                        fsm_post(x, "reloaded soundfont '%s' (id %d)",
                                 name->s_name, id);
                    } else {
                        fsm_error(x, "cannot reload soundfont '%s' (id %d)",
                                  name->s_name, id);
                    }
                }
            } else {
                fluid_sfont_t* sf = fsm_sfont_get_by_name(x, sym);

                if (sf != NULL) {
                    unsigned int id = fluid_sfont_get_id(sf);

                    if (fluid_synth_sfreload(x->synth, id) >= 0) {
                        fsm_post(x, "reloaded soundfont '%s' (id %d)",
                                 sym->s_name, id);
                        return;
                    }
                }

                fsm_error(x, "cannot reload soundfont '%s'", sym->s_name);
            }
        }
    }
}

void fsm_note(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
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

void fsm_list(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    fsm_note(x, NULL, argc, argv);
}


void fsm_control_change(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    if (argc > 0 && is_number(argv)) {
        int value = 64;
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
                value = get_number_as_int(argv + 1);
        case 1:
            fluid_synth_cc(x->synth, channel - 1, get_number_as_int(argv),
                           value);
        case 0:
            break;
        }
    }
}

void fsm_mod(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    if (argc > 1 && is_number(argv) && is_number(argv + 1)) {
        int param = get_number_as_int(argv);
        float value = get_number_as_float(argv + 1);
        int channel = 1;

        if (argc > 2 && is_number(argv + 2)) {
            channel = get_number_as_int(argv + 2);

            if (channel < 1)
                channel = 1;
            else if (channel > fluid_synth_count_midi_channels(x->synth))
                channel = fluid_synth_count_midi_channels(x->synth);
        }

        fluid_synth_set_gen(x->synth, channel - 1, param, value);
    }
}

void fsm_pitch_bend(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    if (argc > 0 && is_number(argv)) {
        int channel = 1;
        double bend = 0.0;

        if (argc > 1 && is_number(argv + 1)) {
            channel = get_number_as_int(argv + 1);

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

void fsm_pitch_bend_wheel(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    if (argc > 0 && is_number(argv)) {
        int channel = 1;

        if (argc > 1 && is_number(argv + 1))
            channel = get_number_as_int(argv + 1);

        fluid_synth_pitch_wheel_sens(x->synth, channel - 1,
                                     get_number_as_int(argv));
    }
}

void fsm_program_change(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    if (argc > 0 && is_number(argv)) {
        int channel = 1;

        if (argc > 1 && is_number(argv + 1)) {
            channel = get_number_as_int(argv + 1);

            if (channel < 1)
                channel = 1;
            else if (channel > fluid_synth_count_midi_channels(x->synth))
                channel = fluid_synth_count_midi_channels(x->synth);
        }

        fluid_synth_program_change(x->synth, channel - 1,
                                   get_number_as_int(argv));
    }
}

void fsm_bank_select(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    if (argc > 0 && is_number(argv)) {
        int channel = 1;
        int sf_id;
        int bank_num;
        int prog_num;

        if (argc > 1 && is_number(argv + 1)) {
            channel = get_number_as_int(argv + 1);

            if (channel < 1)
                channel = 1;
            else if (channel > fluid_synth_count_midi_channels(x->synth))
                channel = fluid_synth_count_midi_channels(x->synth);
        }

        fluid_synth_bank_select(x->synth, channel - 1,
                                get_number_as_int(argv));
        fluid_synth_get_program(x->synth, channel - 1, &sf_id, &bank_num,
                                &prog_num);
        fluid_synth_program_change(x->synth, channel - 1, prog_num);
    }
}

void fsm_select(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    unsigned int bank = 0;
    unsigned int preset = 0;
    int channel = 1;

    switch (argc) {
    default:
    case 4:
        if (is_number(argv + 3))
            channel = get_number_as_int(argv + 3);

        if (channel < 1)
            channel = 1;
        else if (channel > fluid_synth_count_midi_channels(x->synth))
            channel = fluid_synth_count_midi_channels(x->synth);

    case 3:
        if (is_number(argv + 2))
            preset = get_number_as_int(argv + 2);

    case 2:
        if (is_number(argv + 1))
            bank = get_number_as_int(argv + 1);

    case 1:
        if (is_number(argv))
            fluid_synth_program_select(x->synth, channel - 1,
                                       get_number_as_int(argv), bank, preset);
        else if (is_symbol(argv)) {
            t_symbol* name = atom_getsym(argv);
            fluid_sfont_t* sf = fsm_sfont_get_by_name(x, name);

            if (sf != NULL)
                fluid_synth_program_select(x->synth, channel - 1,
                                           fluid_sfont_get_id(sf), bank,
                                           preset);
            else
                fsm_error(x, "cannot find soundfont named '%s'", name->s_name);
        }
    case 0:
        break;
    }
}

void fsm_reverb(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    int res;
    t_fsm_fx_reverb r;
    r.fx_group = -1;

    if (argc == 0) {
        fluid_synth_reverb_on(x->synth, r.fx_group, 1);
        x->reverb = 1;
    } else if (is_number(argv)) {
        res = fluid_synth_get_reverb_group_roomsize(x->synth, r.fx_group,
                                                    &r.roomsize);
        if (res != FLUID_OK) {
            fsm_error(x, "could not get reverb group roomsize");
            goto exception;
        }
        res = fluid_synth_get_reverb_group_damp(x->synth, r.fx_group,
                                                &r.damping);
        if (res != FLUID_OK) {
            fsm_error(x, "could not get reverb group damping value");
            goto exception;
        }
        res = fluid_synth_get_reverb_group_width(x->synth, r.fx_group,
                                                 &r.width);
        if (res != FLUID_OK) {
            fsm_error(x, "could not get reverb group width value");
            goto exception;
        }
        res = fluid_synth_get_reverb_group_level(x->synth, r.fx_group,
                                                 &r.level);
        if (res != FLUID_OK) {
            fsm_error(x, "could not get reverb group level");
            goto exception;
        }

        fluid_synth_reverb_on(x->synth, -1, 1);
        x->reverb = 1;

        switch (argc) {
        default:
        case 4:
            if (is_number(argv + 3))
                r.width = get_number_as_float(argv + 3);
        case 3:
            if (is_number(argv + 2))
                r.damping = get_number_as_float(argv + 2);
        case 2:
            if (is_number(argv + 1))
                r.roomsize = get_number_as_float(argv + 1);
        case 1: {
            r.level = get_number_as_float(argv);
            res = fluid_synth_set_reverb_group_roomsize(x->synth, r.fx_group,
                                                        r.roomsize);
            if (res != FLUID_OK) {
                fsm_error(x, "could not set reverb group roomsize");
                goto exception;
            }
            res = fluid_synth_set_reverb_group_damp(x->synth, r.fx_group,
                                                    r.damping);
            if (res != FLUID_OK) {
                fsm_error(x, "could not set reverb group damping value");
                goto exception;
            }
            res = fluid_synth_set_reverb_group_width(x->synth, r.fx_group,
                                                     r.width);
            if (res != FLUID_OK) {
                fsm_error(x, "could not set reverb group width value");
                goto exception;
            }
            res = fluid_synth_set_reverb_group_level(x->synth, r.fx_group,
                                                     r.level);
            if (res != FLUID_OK) {
                fsm_error(x, "could not set reverb group level");
                goto exception;
            }
        } break;
        }
    } else if (is_symbol(argv)) {
        t_symbol* sym = atom_getsym(argv);

        if (sym == gensym("on")) {
            fluid_synth_reverb_on(x->synth, r.fx_group, 1);
            x->reverb = 1;
        } else if (sym == gensym("off")) {
            fluid_synth_reverb_on(x->synth, r.fx_group, 0);
            x->reverb = 0;
        }
    }
    return;

exception:
    fsm_error(x, "could not get/set reverb value");
}


void fsm_chorus(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    int res;
    t_fsm_fx_chorus c;

    if (argc == 0) {
        fluid_synth_chorus_on(x->synth, c.fx_group, 1);
        x->chorus = 1;
    } else if (is_number(argv)) {

        res = fluid_synth_get_chorus_group_speed(x->synth, c.fx_group,
                                                 &c.speed);
        if (res != FLUID_OK) {
            fsm_error(x, "could not get chorus group speed");
            goto exception;
        }
        res = fluid_synth_get_chorus_group_depth(x->synth, c.fx_group,
                                                 &c.depth_ms);
        if (res != FLUID_OK) {
            fsm_error(x, "could not get chorus group depth_ms value");
            goto exception;
        }
        res = fluid_synth_get_chorus_group_type(x->synth, c.fx_group, &c.type);
        if (res != FLUID_OK) {
            fsm_error(x, "could not get chorus group type value");
            goto exception;
        }
        res = fluid_synth_get_chorus_group_nr(x->synth, c.fx_group, &c.nr);
        if (res != FLUID_OK) {
            fsm_error(x, "could not get chorus group nr value");
            goto exception;
        }
        res = fluid_synth_get_chorus_group_level(x->synth, c.fx_group,
                                                 &c.level);
        if (res != FLUID_OK) {
            fsm_error(x, "could not get chorus group level");
            goto exception;
        }

        fluid_synth_chorus_on(x->synth, -1, 1);
        x->chorus = 1;

        switch (argc) {
        default:
        case 5:
            if (is_number(argv + 4))
                c.nr = get_number_as_int(argv + 4);
        case 4:
            if (is_number(argv + 3))
                c.type = get_number_as_int(argv + 3);
        case 3:
            if (is_number(argv + 2))
                c.depth_ms = get_number_as_float(argv + 2);
        case 2:
            if (is_number(argv + 1))
                c.speed = get_number_as_float(argv + 1);
        case 1: {
            fluid_synth_chorus_on(x->synth, c.fx_group, TRUE);

            c.level = get_number_as_float(argv);

            res = fluid_synth_set_chorus_group_nr(x->synth, c.fx_group, c.nr);
            if (res != FLUID_OK) {
                fsm_error(x, "could not set chorus group nr");
                goto exception;
            }
            res = fluid_synth_set_chorus_group_level(x->synth, c.fx_group,
                                                     c.level);
            if (res != FLUID_OK) {
                fsm_error(x, "could not set chorus group level");
                goto exception;
            }
            res = fluid_synth_set_chorus_group_speed(x->synth, c.fx_group,
                                                     c.speed);
            if (res != FLUID_OK) {
                fsm_error(x, "could not set chorus group speed");
                goto exception;
            }
            res = fluid_synth_set_chorus_group_depth(x->synth, c.fx_group,
                                                     c.depth_ms);
            if (res != FLUID_OK) {
                fsm_error(x, "could not set chorus group depth_ms");
                goto exception;
            }
            res = fluid_synth_set_chorus_group_type(x->synth, c.fx_group,
                                                    c.type);
            if (res != FLUID_OK) {
                fsm_error(x, "could not set chorus group type");
                goto exception;
            }
        }
        case 0:
            break;
        }
    } else if (is_symbol(argv)) {
        t_symbol* sym = atom_getsym(argv);

        if (sym == gensym("on")) {
            fluid_synth_chorus_on(x->synth, c.fx_group, 1);
            x->chorus = 1;
        } else if (sym == gensym("off")) {
            fluid_synth_chorus_on(x->synth, c.fx_group, 0);
            x->chorus = 0;
        }
    }
    return;

exception:
    fsm_error(x, "could not get/set chorus value");
}

void fsm_set_gain(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    if (argc > 0 && is_number(argv)) {
        float gain = get_number_as_float(argv);

        fluid_synth_set_gain(x->synth, gain);
    }
}

void fsm_set_resampling_method(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    int ip = 0;

    if (argc > 0) {
        if (is_number(argv)) {
            ip = (int)atom_getlong(argv);

            if (ip == 0) {
                fluid_synth_set_interp_method(x->synth, -1, FLUID_INTERP_NONE);
            }
            else if (ip < 3) {
                fluid_synth_set_interp_method(x->synth, -1, FLUID_INTERP_LINEAR);
            }
            else if (ip < 6) {
                fluid_synth_set_interp_method(x->synth, -1, FLUID_INTERP_4THORDER);
            }
            else {
                fluid_synth_set_interp_method(x->synth, -1, FLUID_INTERP_7THORDER);
            }
        } else if (is_symbol(argv)) {
            t_symbol* sym = atom_getsym(argv);

            if (sym == gensym("nearest")) {
                fluid_synth_set_interp_method(x->synth, -1, FLUID_INTERP_NONE);
            }
            else if (sym == gensym("linear")) {
                fluid_synth_set_interp_method(x->synth, -1, FLUID_INTERP_LINEAR);
            }
            else if (sym == gensym("cubic")) {
                fluid_synth_set_interp_method(x->synth, -1, FLUID_INTERP_4THORDER);
            }
            else if (sym == gensym("sinc")) {
                fluid_synth_set_interp_method(x->synth, -1, FLUID_INTERP_7THORDER);
            }
            else {
                fsm_error(x, "undefined resampling method: %s", sym->s_name);
            }
        }
    }
}

void fsm_panic(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    fluid_synth_system_reset(x->synth);
}

void fsm_reset(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    // fluid_synth_system_reset(x->synth);

    // fluid_synth_reset_basic_channel(x->synth, -1); // for all channels;
    int n = fluid_synth_count_midi_channels(x->synth);
    for (int i = 0; i < n; i++) {
        fluid_synth_reset_basic_channel(x->synth, i);
        // fluid_channel_reset(x->synth->channel[i]);
    }
}

void fsm_mute(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    int mute = 1;

    if (argc > 0 && is_number(argv))
        mute = (get_number_as_int(argv) != 0);

    fluid_synth_system_reset(x->synth);

    x->mute = mute;
}

void fsm_unmute(t_fsm* x)
{
    t_atom a;

    atom_setlong(&a, 0);
    fsm_mute(x, NULL, 1, &a);
}


void fsm_tuning_octave(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
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
        tuning_bank = get_number_as_int(argv) % 128;

    if (argc > 1 && is_number(argv + 1))
        tuning_prog = get_number_as_int(argv) % 128;

    for (i = 0; i < n; i++) {
        if (is_number(argv + i + 2))
            pitch[i] = get_number_as_float(argv + i + 2);
        else
            pitch[i] = 0.0;
    }

    for (; i < 12; n++)
        pitch[i] = 0.0;

    fluid_synth_activate_octave_tuning(x->synth, tuning_bank, tuning_prog,
                                       name->s_name, pitch, TRUE);
}

void fsm_tuning_select(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    int tuning_bank = 0;
    int tuning_prog = 0;
    int channel = 1;

    if (argc > 0 && is_number(argv))
        tuning_bank = get_number_as_int(argv) % 128;

    if (argc > 1 && is_number(argv + 1))
        tuning_prog = get_number_as_int(argv + 1) % 128;

    if (argc > 2 && is_number(argv + 2))
        channel = get_number_as_int(argv + 2);

    if (channel < 1)
        channel = 1;
    else if (channel > fluid_synth_count_midi_channels(x->synth))
        channel = fluid_synth_count_midi_channels(x->synth);

    fluid_synth_activate_tuning(x->synth, channel - 1, tuning_bank,
                                tuning_prog, TRUE);
}

void fsm_tuning_reset(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    int channel = 0;

    if (argc > 0 && is_number(argv))
        channel = get_number_as_int(argv);

    if (channel < 1)
        channel = 1;
    else if (channel > fluid_synth_count_midi_channels(x->synth))
        channel = fluid_synth_count_midi_channels(x->synth);

    fluid_synth_deactivate_tuning(x->synth, channel - 1, FALSE);
}


void fsm_version(t_fsm* x)
{
    post("fluidsynth~, version %s (based on FluidSynth %s)", FSM_VERSION,
        FLUIDSYNTH_VERSION);
    post("FluidSynth is written by Peter Hanappe et al. (see fluidsynth.org)");
    post("Max/MSP integration by Norbert Schnell IMTR IRCAM - Centre Pompidou");
}


void fsm_print_soundfonts(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    int n = fluid_synth_sfcount(x->synth);
    int i;

    if (n > 0) {
        fsm_post(x, "soundfonts:");
    } else {
        fsm_error(x, "no soundfonts loaded");
        return;
    }

    for (i = 0; i < n; i++) {
        fluid_sfont_t* sf = fluid_synth_get_sfont(x->synth, i);
        t_symbol* name = fsm_sfont_get_name(sf);
        unsigned int id = fluid_sfont_get_id(sf);

        fsm_post(x, "  %d: '%s' (id %d)", i, name->s_name, id);
    }
}

void fsm_print_presets(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    int n = fluid_synth_sfcount(x->synth);

    if (n > 0) {
        if (argc > 1) {
            fluid_sfont_t* sf = NULL;
            t_symbol* name;

            if (is_symbol(argv + 1)) {
                name = atom_getsym(argv + 1);
                sf = fsm_sfont_get_by_name(x, name);
            } else if (is_long(argv + 1)) {
                int id = (int)atom_getlong(argv + 1);

                sf = fluid_synth_get_sfont_by_id(x->synth, id);
                name = fsm_sfont_get_name(sf);
            }

            if (sf != NULL) {
                fluid_preset_t* preset;

                fluid_sfont_iteration_start(sf);

                fsm_post(x, "presets of soundfont '%s':", name->s_name);

                while ((preset = fluid_sfont_iteration_next(sf)) != NULL) {
                    const char* preset_str = fluid_preset_get_name(preset);
                    t_symbol* preset_name = gensym(preset_str);
                    int bank_num = fluid_preset_get_banknum(preset);
                    int prog_num = fluid_preset_get_num(preset);

                    fsm_post(x, "  '%s': bank %d, program %d",
                             preset_name->s_name, bank_num, prog_num);
                }
            }
        } else {
            int i;

            fsm_post(x, "presets:");

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
                        t_symbol* sf_name = fsm_sfont_get_name(sf);
                        const char* preset_str = fluid_preset_get_name(preset);
                        t_symbol* preset_name = gensym(preset_str);

                        fsm_post(x,
                                 "  '%s': soundfont '%s', bank %d, "
                                 "program %d",
                                 preset_name->s_name, sf_name->s_name, i, j);
                    }
                }
            }
        }
    } else
        fsm_error(x, "no soundfonts loaded");
}

void fsm_print_channels(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    int n = fluid_synth_count_midi_channels(x->synth);
    int i;

    fsm_post(x, "channels:");

    for (i = 0; i < n; i++) {
        fluid_preset_t* preset = fluid_synth_get_channel_preset(x->synth, i);

        if (preset != NULL) {
            const char* preset_str = fluid_preset_get_name(preset);
            t_symbol* preset_name = gensym(preset_str);
            int sf_id;
            int bank_num;
            int prog_num;
            fluid_sfont_t* sf;

            fluid_synth_get_program(x->synth, i, &sf_id, &bank_num, &prog_num);
            sf = fluid_synth_get_sfont_by_id(x->synth, sf_id);

            fsm_post(x, "  %d: soundfont '%s', bank %d, program %d: '%s'",
                     i + 1, fsm_sfont_get_name(sf)->s_name, bank_num, prog_num,
                     preset_name->s_name);
        } else
            fsm_post(x, "  channel %d: no preset", i + 1);
    }
}

void fsm_print_generators(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    int channel = 1;
    int n = GEN_LAST;
    int i;

    if (argc > 1 && is_number(argv + 1))
        channel = get_number_as_int(argv + 1);

    if (channel < 1)
        channel = 1;
    else if (channel > fluid_synth_count_midi_channels(x->synth))
        channel = fluid_synth_count_midi_channels(x->synth);

    fsm_post(x, "generators of channel %d:", channel);

    for (i = 0; i < n; i++) {
        const char* name = fsm_gen_info[i].name;
        const char* unit = fsm_gen_info[i].unit;
        double incr = fluid_synth_get_gen(x->synth, channel - 1, i);
        double min = fluid_gen_info[i].min;
        double max = fluid_gen_info[i].max;

        fsm_post(x, "  %d '%s': %s %g [%g ... %g] (%s)", i, name,
                 (incr >= 0) ? "" : "-", fabs(incr), min, max, unit);
    }
}

void fsm_print_gain(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    double gain = fluid_synth_get_gain(x->synth);
    fsm_post(x, "gain: %g", gain);
}

void fsm_print_reverb(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    t_fsm_fx_reverb r;
    r.fx_group = -1;

    fluid_synth_get_reverb_group_roomsize(x->synth, r.fx_group, &r.roomsize);
    fluid_synth_get_reverb_group_damp(x->synth, r.fx_group, &r.damping);
    fluid_synth_get_reverb_group_level(x->synth, r.fx_group, &r.level);
    fluid_synth_get_reverb_group_width(x->synth, r.fx_group, &r.width);

    if (x->reverb != 0) {
        fsm_post(x, "current reverb parameters:");
        fsm_post(x, "  level: %f", r.level);
        fsm_post(x, "  room size: %f", r.roomsize);
        fsm_post(x, "  damping: %f", r.damping);
        fsm_post(x, "  width: %f", r.width);
    } else {
        fsm_post(x, "reverb off");
    }
}

void fsm_print_chorus(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    t_fsm_fx_chorus c;
    c.fx_group = -1;

    if (x->chorus != 0) {
        fluid_synth_get_chorus_group_speed(x->synth, c.fx_group, &c.speed);
        fluid_synth_get_chorus_group_level(x->synth, c.fx_group, &c.level);
        fluid_synth_get_chorus_group_depth(x->synth, c.fx_group, &c.depth_ms);
        fluid_synth_get_chorus_group_nr(x->synth, c.fx_group, &c.nr);
        fluid_synth_get_chorus_group_type(x->synth, c.fx_group, &c.type);

        fsm_post(x, "current chorus parameters:");
        fsm_post(x, "  level: %f", c.level);
        fsm_post(x, "  speed: %f Hz", c.speed);
        fsm_post(x, "  depth: %f msec", c.depth_ms);
        fsm_post(x, "  type: %d (%s)", c.type, c.type ? "triangle" : "sine");
        fsm_post(x, "  %d units", c.nr);
    } else {
        fsm_post(x, "chorus off");
    }
}

void fsm_print(t_fsm* x, t_symbol* s, short argc, t_atom* argv)
{
    if (argc > 0) {
        if (is_symbol(argv)) {
            t_symbol* sym = atom_getsym(argv);
            if (sym == gensym("soundfonts")) {
                fsm_print_soundfonts(x, s, argc, argv);
            } else if (sym == gensym("presets")) {
                fsm_print_presets(x, s, argc, argv);
            } else if (sym == gensym("channels")) {
                fsm_print_channels(x, s, argc, argv);
            } else if (sym == gensym("generators")) {
                fsm_print_generators(x, s, argc, argv);
            } else if (sym == gensym("gain")) {
                fsm_print_gain(x, s, argc, argv);
            } else if (sym == gensym("reverb")) {
                fsm_print_reverb(x, s, argc, argv);
            } else if (sym == gensym("chorus")) {
                fsm_print_chorus(x, s, argc, argv);
            }
        }
    }
}

void fsm_info_soundfonts(t_fsm* x, t_symbol* s, long argc, t_atom* argv)
{
    int n = fluid_synth_sfcount(x->synth);
    int i;

    for (int i = 0; i < n; i++) {
        fluid_sfont_t* sf = fluid_synth_get_sfont(x->synth, i);
        unsigned int id = fluid_sfont_get_id(sf);
        t_atom a[2];

        atom_setlong(a, i);
        atom_setsym(a + 1, fsm_sfont_get_name(sf));
        atom_setlong(a + 2, id);
        outlet_anything(x->outlet, gensym("soundfont"), 3, a);
    }
}

void fsm_info_presets(t_fsm* x, t_symbol* s, long argc, t_atom* argv)
{
    int n = fluid_synth_sfcount(x->synth);
    if (n <= 0) {
        fsm_error(x, "info: no soundfonts loaded");
        return;
    }

    if (argc > 1) {
        fluid_sfont_t* sf = NULL;
        t_symbol* sf_name;

        if (is_symbol(argv + 1)) {
            sf_name = atom_getsym(argv + 1);
            sf = fsm_sfont_get_by_name(x, sf_name);
        } else if (is_long(argv + 1)) {
            int id = (int)atom_getlong(argv + 1);
            sf = fluid_synth_get_sfont_by_id(x->synth, id);
            sf_name = fsm_sfont_get_name(sf);
        }

        if (sf != NULL) {
            fluid_preset_t* preset;

            fluid_sfont_iteration_start(sf);

            while ((preset = fluid_sfont_iteration_next(sf)) != NULL) {
                const char* preset_str = fluid_preset_get_name(preset);
                t_symbol* preset_name = gensym(preset_str);
                int bank_num = fluid_preset_get_banknum(preset);
                int prog_num = fluid_preset_get_num(preset);
                t_atom a[4];

                atom_setsym(a, preset_name);
                atom_setsym(a + 1, sf_name);
                atom_setlong(a + 2, bank_num);
                atom_setlong(a + 3, prog_num);
                outlet_anything(x->outlet, gensym("preset"), 4, a);
            }
        }
    } else {

        for (int i = 0; i < 128; i++) {

            for (int j = 0; j < 128; j++) {
                fluid_preset_t* preset = NULL;
                fluid_sfont_t* sf = NULL;

                for (int k = 0; k < n; k++) {
                    sf = fluid_synth_get_sfont(x->synth, k);
                    preset = fluid_sfont_get_preset(sf, i, j);

                    if (preset != NULL)
                        break;
                }

                if (preset != NULL) {
                    t_symbol* sf_name = fsm_sfont_get_name(sf);
                    const char* preset_str = fluid_preset_get_name(preset);
                    t_symbol* preset_name = gensym(preset_str);
                    t_atom a[4];

                    atom_setsym(a, preset_name);
                    atom_setsym(a + 1, sf_name);
                    atom_setlong(a + 2, i);
                    atom_setlong(a + 3, j);
                    outlet_anything(x->outlet, gensym("preset"), 4, a);
                }
            }
        }
    }
}

void fsm_info_channels(t_fsm* x, t_symbol* s, long argc, t_atom* argv)
{
    int n = fluid_synth_count_midi_channels(x->synth);

    for (int i = 0; i < n; i++) {
        fluid_preset_t* preset = fluid_synth_get_channel_preset(x->synth, i);

        if (preset != NULL) {
            const char* preset_str = fluid_preset_get_name(preset);
            t_symbol* preset_name = gensym(preset_str);
            // unsigned int sf_id, bank_num, prog_num;
            int sf_id, bank_num, prog_num;
            fluid_sfont_t* sf;
            t_atom a[5];

            fluid_synth_get_program(x->synth, i, &sf_id, &bank_num, &prog_num);
            sf = fluid_synth_get_sfont_by_id(x->synth, sf_id);

            atom_setlong(a, i + 1);
            atom_setsym(a + 1, fsm_sfont_get_name(sf));
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
}

void fsm_info_gain(t_fsm* x, t_symbol* s, long argc, t_atom* argv)
{
    t_atom a;
    double gain = fluid_synth_get_gain(x->synth);

    atom_setfloat(&a, gain);
    outlet_anything(x->outlet, gensym("channel"), 1, &a);
}

void fsm_info_reverb(t_fsm* x, t_symbol* s, long argc, t_atom* argv)
{
    t_fsm_fx_reverb r;
    r.fx_group = -1;

    if (x->reverb != 0) {
        fluid_synth_get_reverb_group_roomsize(x->synth, r.fx_group,
                                              &r.roomsize);
        fluid_synth_get_reverb_group_damp(x->synth, r.fx_group, &r.damping);
        fluid_synth_get_reverb_group_level(x->synth, r.fx_group, &r.level);
        fluid_synth_get_reverb_group_width(x->synth, r.fx_group, &r.width);

        t_atom a[4];
        atom_setfloat(a, r.level);
        atom_setfloat(a + 1, r.roomsize);
        atom_setfloat(a + 2, r.damping);
        atom_setfloat(a + 3, r.width);
        outlet_anything(x->outlet, gensym("reverb"), 4, a);
    } else {
        t_atom a;
        atom_setsym(&a, gensym("off"));
        outlet_anything(x->outlet, gensym("reverb"), 1, &a);
    }
}

void fsm_info_chorus(t_fsm* x, t_symbol* s, long argc, t_atom* argv)
{
    t_fsm_fx_chorus c;
    c.fx_group = -1;

    if (x->chorus != 0) {
        fluid_synth_get_chorus_group_speed(x->synth, c.fx_group, &c.speed);
        fluid_synth_get_chorus_group_level(x->synth, c.fx_group, &c.level);
        fluid_synth_get_chorus_group_depth(x->synth, c.fx_group, &c.depth_ms);
        fluid_synth_get_chorus_group_nr(x->synth, c.fx_group, &c.nr);
        fluid_synth_get_chorus_group_type(x->synth, c.fx_group, &c.type);

        t_atom a[5];

        atom_setfloat(a, c.level);
        atom_setfloat(a + 1, c.speed);
        atom_setfloat(a + 2, c.depth_ms);
        atom_setlong(a + 3, c.type);
        atom_setlong(a + 4, c.nr);
        outlet_anything(x->outlet, gensym("chorus"), 5, a);
    } else {
        t_atom a;
        atom_setsym(&a, gensym("off"));
        outlet_anything(x->outlet, gensym("chorus"), 1, &a);
    }
}

void fsm_info_polyphony(t_fsm* x, t_symbol* s, long argc, t_atom* argv)
{
    int polyphony = fluid_synth_get_polyphony(x->synth);
    t_atom a;

    atom_setlong(&a, polyphony);
    outlet_anything(x->outlet, gensym("polyphony"), 1, &a);
}

void fsm_info(t_fsm* x, t_symbol* s, long argc, t_atom* argv)
{
    if (argc > 0) {
        if (is_symbol(argv)) {
            t_symbol* sym = atom_getsym(argv);
            if (sym == gensym("soundfonts")) {
                fsm_info_soundfonts(x, s, argc, argv);
            } else if (sym == gensym("presets")) {
                fsm_info_presets(x, s, argc, argv);
            } else if (sym == gensym("channels")) {
                fsm_info_channels(x, s, argc, argv);
            } else if (sym == gensym("gain")) {
                fsm_info_gain(x, s, argc, argv);
            } else if (sym == gensym("reverb")) {
                fsm_info_reverb(x, s, argc, argv);
            } else if (sym == gensym("chorus")) {
                fsm_info_chorus(x, s, argc, argv);
            } else if (sym == gensym("polyphony")) {
                fsm_info_polyphony(x, s, argc, argv);
            }
        }
    }
}

/*----------------------------------------------------------------------------*/
// class

void* fsm_new(t_symbol* s, long argc, t_atom* argv)
{
    t_fsm* x = (t_fsm*)object_alloc(fsm_class);
    int polyphony = 256;
    int midi_channels = 16;

    x->outlet = outlet_new(x, "anything");

    dsp_setup((t_pxobject*)x, 0);
    outlet_new(x, "signal");
    outlet_new(x, "signal");

    x->synth = NULL;
    x->settings = new_fluid_settings();
    x->left_buffer = NULL;
    x->right_buffer = NULL;
    x->out_maxsize = 0;
    x->reverb = 0;
    x->chorus = 0;
    x->mute = 0;

    if (argc > 0 && is_number(argv)) {
        polyphony = get_number_as_int(argv);
        argc--;
        argv++;
    }

    if (argc > 0 && is_number(argv)) {
        midi_channels = get_number_as_int(argv);
        argc--;
        argv++;
    }

    if (argc > 0 && is_symbol(argv)) {
        fsm_load(x, NULL, 1, argv);
    }

    if (x->settings != NULL) {
        fluid_settings_setint(x->settings, "synth.midi-channels",
                              midi_channels);
        fluid_settings_setint(x->settings, "synth.polyphony", polyphony);
        fluid_settings_setnum(x->settings, "synth.gain", 0.600000);
        fluid_settings_setnum(x->settings, "synth.sample-rate", sys_getsr());
        fluid_settings_setstr(x->settings, "synth.default-soundfont",
                              "/usr/local/share/sounds/sf2/FluidR3_GM.sf2");

        x->synth = new_fluid_synth(x->settings);

        if (x->synth != NULL) {
            fluid_synth_reverb_on(x->synth, -1, 0);
            fluid_synth_chorus_on(x->synth, -1, 0);

            if (argc > 0 && is_symbol(argv))
                fsm_load(x, NULL, argc, argv);

            return x;
        }

        delete_fluid_settings(x->settings);
    }

    fsm_error(x, "cannot create FluidSynth core");

    return NULL;
}

void fsm_free(t_fsm* x)
{
    if (x->synth != NULL)
        delete_fluid_synth(x->synth);

    if (x->settings != NULL)
        delete_fluid_settings(x->settings);

    if(x->left_buffer != NULL)
        sysmem_freeptr(x->left_buffer);
  
    if(x->right_buffer != NULL)
        sysmem_freeptr(x->right_buffer);
  
    dsp_free((t_pxobject*)x);
}

// clang-format off
void ext_main(void* r)
{
    t_class* c = class_new("fsm~", 
        (method)fsm_new, 
        (method)fsm_free, 
        (long)sizeof(t_fsm), 0L, A_GIMME, 0);

    class_addmethod(c, (method)fsm_dsp64, "dsp64", A_CANT, 0);
    // class_addmethod(c, (method)fsm_assist, "assist", A_CANT, 0);

    class_addmethod(c, (method)fsm_version, "version", 0);
    class_addmethod(c, (method)fsm_print, "print", A_GIMME, 0);

    class_addmethod(c, (method)fsm_load, "load", A_GIMME, 0);
    class_addmethod(c, (method)fsm_unload, "unload", A_GIMME, 0);
    class_addmethod(c, (method)fsm_reload, "reload", A_GIMME, 0);
    class_addmethod(c, (method)fsm_info, "info", A_GIMME, 0);

    class_addmethod(c, (method)fsm_panic, "panic", A_GIMME, 0);
    class_addmethod(c, (method)fsm_reset, "reset", A_GIMME, 0);
    class_addmethod(c, (method)fsm_mute, "mute", A_GIMME, 0);
    class_addmethod(c, (method)fsm_unmute, "unmute", 0);

    /*class_addmethod(c, (method)fsm_tuning_keys, "tuning-keys", A_GIMME, 0);*/
    class_addmethod(c, (method)fsm_tuning_octave, "tuning-octave", A_GIMME, 0);
    class_addmethod(c, (method)fsm_tuning_select, "tuning-select", A_GIMME, 0);
    class_addmethod(c, (method)fsm_tuning_reset, "tuning-reset", A_GIMME, 0);

    class_addmethod(c, (method)fsm_reverb, "reverb", A_GIMME, 0);
    class_addmethod(c, (method)fsm_chorus, "chorus", A_GIMME, 0);
    class_addmethod(c, (method)fsm_set_gain, "gain", A_GIMME, 0);
    class_addmethod(c, (method)fsm_set_resampling_method, "resample", A_GIMME, 0);

    class_addmethod(c, (method)fsm_note, "note", A_GIMME, 0);
    class_addmethod(c, (method)fsm_list, "list", A_GIMME, 0);

    class_addmethod(c, (method)fsm_control_change, "control", A_GIMME, 0);
    class_addmethod(c, (method)fsm_mod, "mod", A_GIMME, 0);

    class_addmethod(c, (method)fsm_pitch_bend, "bend", A_GIMME, 0);
    class_addmethod(c, (method)fsm_pitch_bend_wheel, "wheel", A_GIMME, 0);

    class_addmethod(c, (method)fsm_program_change, "program", A_GIMME, 0);
    class_addmethod(c, (method)fsm_bank_select, "bank", A_GIMME, 0);
    class_addmethod(c, (method)fsm_select, "select", A_GIMME, 0);

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    fsm_class = c;
}
// clang-format on
