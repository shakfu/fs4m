#ifndef FLUIDMAX_TILDE_H
#define FLUIDMAX_TILDE_H

#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"
#include "fluidsynth.h"

#define FLUIDMAX_VERSION "03/2025 (15)"
#define M_TWOPI 6.283185308

typedef struct t_fluidmax t_fluidmax;

// Function declarations
void* fluidmax_new(t_symbol* s, long argc, t_atom* argv);
void fluidmax_free(t_fluidmax* x);
void fluidmax_dsp64(t_fluidmax* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags);
void fluidmax_perform64(t_fluidmax* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam);

// User methods
void fluidmax_version(t_fluidmax* x);
void fluidmax_print(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_load(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_unload(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_reload(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_info(t_fluidmax* x, t_symbol* s, long argc, t_atom* argv);
void fluidmax_panic(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_reset(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_mute(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_unmute(t_fluidmax* x);
void fluidmax_tuning_octave(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_tuning_select(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_tuning_reset(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_reverb(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_chorus(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_set_gain(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_set_resampling_method(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_note(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_list(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_control_change(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_mod(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_pitch_bend(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_pitch_bend_wheel(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_program_change(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_bank_select(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_select(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);

// Info methods
void fluidmax_info_soundfonts(t_fluidmax* x, t_symbol* s, long argc, t_atom* argv);
void fluidmax_info_presets(t_fluidmax* x, t_symbol* s, long argc, t_atom* argv);
void fluidmax_info_channels(t_fluidmax* x, t_symbol* s, long argc, t_atom* argv);
void fluidmax_info_gain(t_fluidmax* x, t_symbol* s, long argc, t_atom* argv);
void fluidmax_info_reverb(t_fluidmax* x, t_symbol* s, long argc, t_atom* argv);
void fluidmax_info_chorus(t_fluidmax* x, t_symbol* s, long argc, t_atom* argv);
void fluidmax_info_polyphony(t_fluidmax* x, t_symbol* s, long argc, t_atom* argv);

// Print methods
void fluidmax_print_soundfonts(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_print_presets(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_print_channels(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_print_generators(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_print_gain(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_print_reverb(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_print_chorus(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);

// Utility functions
int is_long(t_atom* a);
int is_float(t_atom* a);
int is_number(t_atom* a);
int is_symbol(t_atom* a);
int get_number_as_long(t_atom* a);
double get_number_as_float(t_atom* a);
char* fluidmax_translate_fullpath(char* maxpath, char* fullpath);
t_symbol* fluidmax_get_stripped_name(const char* fullpath);
t_symbol* fluidmax_sfont_get_name(fluid_sfont_t* sfont);
fluid_sfont_t* fluidmax_sfont_get_by_name(t_fluidmax* x, t_symbol* name);
void fluidmax_do_load(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);
void fluidmax_load_with_dialog(t_fluidmax* x, t_symbol* s, short argc, t_atom* argv);

// global declaration of the class
t_class* fluidmax_class;

#endif // FLUIDMAX_TILDE_H 