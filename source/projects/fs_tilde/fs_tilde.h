#ifndef FS_TILDE_H
#define FS_TILDE_H

#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"
#include "fluidsynth.h"

#define FS_VERSION "03/2025 (15)"
#define M_TWOPI 6.283185308

#define fs_error(x, ...) object_error((t_object*)(x), __VA_ARGS__);
#define fs_post(x, ...) object_post((t_object*)(x), __VA_ARGS__);

typedef struct t_fs t_fs;

// Function declarations
void* fs_new(t_symbol* s, long argc, t_atom* argv);
void fs_free(t_fs* x);
void fs_dsp64(t_fs* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags);
void fs_perform64(t_fs* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam);

// User methods
void fs_version(t_fs* x);
void fs_print(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_load(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_unload(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_reload(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_info(t_fs* x, t_symbol* s, long argc, t_atom* argv);
void fs_panic(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_reset(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_mute(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_unmute(t_fs* x);
void fs_tuning_octave(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_tuning_select(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_tuning_reset(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_reverb(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_chorus(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_set_gain(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_set_resampling_method(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_note(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_list(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_control_change(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_mod(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_pitch_bend(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_pitch_bend_wheel(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_program_change(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_bank_select(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_select(t_fs* x, t_symbol* s, short argc, t_atom* argv);

// Info methods
void fs_info_soundfonts(t_fs* x, t_symbol* s, long argc, t_atom* argv);
void fs_info_presets(t_fs* x, t_symbol* s, long argc, t_atom* argv);
void fs_info_channels(t_fs* x, t_symbol* s, long argc, t_atom* argv);
void fs_info_gain(t_fs* x, t_symbol* s, long argc, t_atom* argv);
void fs_info_reverb(t_fs* x, t_symbol* s, long argc, t_atom* argv);
void fs_info_chorus(t_fs* x, t_symbol* s, long argc, t_atom* argv);
void fs_info_polyphony(t_fs* x, t_symbol* s, long argc, t_atom* argv);

// Print methods
void fs_print_soundfonts(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_print_presets(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_print_channels(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_print_generators(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_print_gain(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_print_reverb(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_print_chorus(t_fs* x, t_symbol* s, short argc, t_atom* argv);

// Utility functions
int is_long(t_atom* a);
int is_float(t_atom* a);
int is_number(t_atom* a);
int is_symbol(t_atom* a);
int get_number_as_int(t_atom* a);
double get_number_as_float(t_atom* a);

char* fs_translate_fullpath(char* maxpath, char* fullpath);
t_symbol* fs_get_stripped_name(const char* fullpath);
t_symbol* fs_sfont_get_name(fluid_sfont_t* sfont);
fluid_sfont_t* fs_sfont_get_by_name(t_fs* x, t_symbol* name);
void fs_do_load(t_fs* x, t_symbol* s, short argc, t_atom* argv);
void fs_load_with_dialog(t_fs* x, t_symbol* s, short argc, t_atom* argv);

// global declaration of the class
t_class* fs_class;

#endif // FS_TILDE_H 