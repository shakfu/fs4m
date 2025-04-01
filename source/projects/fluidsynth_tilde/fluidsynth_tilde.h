#ifndef FSM_TILDE_H
#define FSM_TILDE_H

#include "ext.h"
#include "ext_obex.h"
#include "z_dsp.h"
#include "fluidsynth.h"

#define FSM_VERSION "03/2025 (15)"
#define M_TWOPI 6.283185308

typedef struct t_fsm t_fsm;

// Function declarations
void* fsm_new(t_symbol* s, long argc, t_atom* argv);
void fsm_free(t_fsm* x);
void fsm_dsp64(t_fsm* x, t_object* dsp64, short* count, double samplerate, long maxvectorsize, long flags);
void fsm_perform64(t_fsm* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam);

// User methods
void fsm_version(t_fsm* x);
void fsm_print(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_load(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_unload(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_reload(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_info(t_fsm* x, t_symbol* s, long argc, t_atom* argv);
void fsm_panic(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_reset(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_mute(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_unmute(t_fsm* x);
void fsm_tuning_octave(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_tuning_select(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_tuning_reset(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_reverb(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_chorus(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_set_gain(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_set_resampling_method(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_note(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_list(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_control_change(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_mod(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_pitch_bend(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_pitch_bend_wheel(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_program_change(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_bank_select(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_select(t_fsm* x, t_symbol* s, short argc, t_atom* argv);

// Info methods
void fsm_info_soundfonts(t_fsm* x, t_symbol* s, long argc, t_atom* argv);
void fsm_info_presets(t_fsm* x, t_symbol* s, long argc, t_atom* argv);
void fsm_info_channels(t_fsm* x, t_symbol* s, long argc, t_atom* argv);
void fsm_info_gain(t_fsm* x, t_symbol* s, long argc, t_atom* argv);
void fsm_info_reverb(t_fsm* x, t_symbol* s, long argc, t_atom* argv);
void fsm_info_chorus(t_fsm* x, t_symbol* s, long argc, t_atom* argv);
void fsm_info_polyphony(t_fsm* x, t_symbol* s, long argc, t_atom* argv);

// Print methods
void fsm_print_soundfonts(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_print_presets(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_print_channels(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_print_generators(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_print_gain(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_print_reverb(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_print_chorus(t_fsm* x, t_symbol* s, short argc, t_atom* argv);

// Utility functions
int is_long(t_atom* a);
int is_float(t_atom* a);
int is_number(t_atom* a);
int is_symbol(t_atom* a);
int get_number_as_long(t_atom* a);
double get_number_as_float(t_atom* a);
char* fsm_translate_fullpath(char* maxpath, char* fullpath);
t_symbol* fsm_get_stripped_name(const char* fullpath);
t_symbol* fsm_sfont_get_name(fluid_sfont_t* sfont);
fluid_sfont_t* fsm_sfont_get_by_name(t_fsm* x, t_symbol* name);
void fsm_do_load(t_fsm* x, t_symbol* s, short argc, t_atom* argv);
void fsm_load_with_dialog(t_fsm* x, t_symbol* s, short argc, t_atom* argv);

// global declaration of the class
t_class* fsm_class;

#endif // FSM_TILDE_H 