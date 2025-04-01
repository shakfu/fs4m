#ifndef FSM_GEN_H
#define FSM_GEN_H

// clang-format off
/*----------------------------------------------------------------------------*/
// generators

typedef struct {
    int index;
    const char* name;
    const char* unit;
} fsm_gen_descr_t;


static fsm_gen_descr_t fsm_gen_info[] = {
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



typedef struct _fluid_gen_info_t
{
    char num;       /* Generator number */
    char *name;
    char init;      /* Does the generator need to be initialized (not used) */
    char nrpn_scale;    /* The scale to convert from NRPN (cfr. fluid_gen_map_nrpn()) */
    float min;      /* The minimum value */
    float max;      /* The maximum value */
    float def;      /* The default value (cfr. fluid_gen_init()) */
} fluid_gen_info_t;

#define _GEN(_name) GEN_ ## _name, #_name

/* See SFSpec21 $8.1.3 */
static const fluid_gen_info_t fluid_gen_info[] =
{
    /* number/name             init  nrpn-scale    min        max         def */
    { _GEN(STARTADDROFS),           1,     1,       0.0f,     1e10f,       0.0f },
    { _GEN(ENDADDROFS),             1,     1,     -1e10f,      0.0f,       0.0f },
    { _GEN(STARTLOOPADDROFS),       1,     1,     -1e10f,     1e10f,       0.0f },
    { _GEN(ENDLOOPADDROFS),         1,     1,     -1e10f,     1e10f,       0.0f },
    { _GEN(STARTADDRCOARSEOFS),     0,     1,       0.0f,     1e10f,       0.0f },
    { _GEN(MODLFOTOPITCH),          1,     2,  -12000.0f,  12000.0f,       0.0f },
    { _GEN(VIBLFOTOPITCH),          1,     2,  -12000.0f,  12000.0f,       0.0f },
    { _GEN(MODENVTOPITCH),          1,     2,  -12000.0f,  12000.0f,       0.0f },
    { _GEN(FILTERFC),               1,     2,    1500.0f,  13500.0f,   13500.0f },
    { _GEN(FILTERQ),                1,     1,       0.0f,    960.0f,       0.0f },
    { _GEN(MODLFOTOFILTERFC),       1,     2,  -12000.0f,  12000.0f,       0.0f },
    { _GEN(MODENVTOFILTERFC),       1,     2,  -12000.0f,  12000.0f,       0.0f },
    { _GEN(ENDADDRCOARSEOFS),       0,     1,     -1e10f,      0.0f,       0.0f },
    { _GEN(MODLFOTOVOL),            1,     1,    -960.0f,    960.0f,       0.0f },
    { _GEN(UNUSED1),                0,     0,       0.0f,      0.0f,       0.0f },
    { _GEN(CHORUSSEND),             1,     1,       0.0f,   1000.0f,       0.0f },
    { _GEN(REVERBSEND),             1,     1,       0.0f,   1000.0f,       0.0f },
    { _GEN(PAN),                    1,     1,    -500.0f,    500.0f,       0.0f },
    { _GEN(UNUSED2),                0,     0,       0.0f,      0.0f,       0.0f },
    { _GEN(UNUSED3),                0,     0,       0.0f,      0.0f,       0.0f },
    { _GEN(UNUSED4),                0,     0,       0.0f,      0.0f,       0.0f },
    { _GEN(MODLFODELAY),            1,     2,  -12000.0f,   5000.0f,  -12000.0f },
    { _GEN(MODLFOFREQ),             1,     4,  -16000.0f,   4500.0f,       0.0f },
    { _GEN(VIBLFODELAY),            1,     2,  -12000.0f,   5000.0f,  -12000.0f },
    { _GEN(VIBLFOFREQ),             1,     4,  -16000.0f,   4500.0f,       0.0f },
    { _GEN(MODENVDELAY),            1,     2,  -12000.0f,   5000.0f,  -12000.0f },
    { _GEN(MODENVATTACK),           1,     2,  -12000.0f,   8000.0f,  -12000.0f },
    { _GEN(MODENVHOLD),             1,     2,  -12000.0f,   5000.0f,  -12000.0f },
    { _GEN(MODENVDECAY),            1,     2,  -12000.0f,   8000.0f,  -12000.0f },
    { _GEN(MODENVSUSTAIN),          0,     1,       0.0f,   1000.0f,       0.0f },
    { _GEN(MODENVRELEASE),          1,     2,  -12000.0f,   8000.0f,  -12000.0f },
    { _GEN(KEYTOMODENVHOLD),        0,     1,   -1200.0f,   1200.0f,       0.0f },
    { _GEN(KEYTOMODENVDECAY),       0,     1,   -1200.0f,   1200.0f,       0.0f },
    { _GEN(VOLENVDELAY),            1,     2,  -12000.0f,   5000.0f,  -12000.0f },
    { _GEN(VOLENVATTACK),           1,     2,  -12000.0f,   8000.0f,  -12000.0f },
    { _GEN(VOLENVHOLD),             1,     2,  -12000.0f,   5000.0f,  -12000.0f },
    { _GEN(VOLENVDECAY),            1,     2,  -12000.0f,   8000.0f,  -12000.0f },
    { _GEN(VOLENVSUSTAIN),          0,     1,       0.0f,   1440.0f,       0.0f },
    { _GEN(VOLENVRELEASE),          1,     2,  -12000.0f,   8000.0f,  -12000.0f },
    { _GEN(KEYTOVOLENVHOLD),        0,     1,   -1200.0f,   1200.0f,       0.0f },
    { _GEN(KEYTOVOLENVDECAY),       0,     1,   -1200.0f,   1200.0f,       0.0f },
    { _GEN(INSTRUMENT),             0,     0,       0.0f,      0.0f,       0.0f },
    { _GEN(RESERVED1),              0,     0,       0.0f,      0.0f,       0.0f },
    { _GEN(KEYRANGE),               0,     0,       0.0f,    127.0f,       0.0f },
    { _GEN(VELRANGE),               0,     0,       0.0f,    127.0f,       0.0f },
    { _GEN(STARTLOOPADDRCOARSEOFS), 0,     1,     -1e10f,     1e10f,       0.0f },
    { _GEN(KEYNUM),                 1,     0,       0.0f,    127.0f,      -1.0f },
    { _GEN(VELOCITY),               1,     1,       0.0f,    127.0f,      -1.0f },
    { _GEN(ATTENUATION),            1,     1,       0.0f,   1440.0f,       0.0f },
    { _GEN(RESERVED2),              0,     0,       0.0f,      0.0f,       0.0f },
    { _GEN(ENDLOOPADDRCOARSEOFS),   0,     1,     -1e10f,     1e10f,       0.0f },
    { _GEN(COARSETUNE),             0,     1,    -120.0f,    120.0f,       0.0f },
    { _GEN(FINETUNE),               0,     1,     -99.0f,     99.0f,       0.0f },
    { _GEN(SAMPLEID),               0,     0,       0.0f,      0.0f,       0.0f },
    { _GEN(SAMPLEMODE),             0,     0,       0.0f,      0.0f,       0.0f },
    { _GEN(RESERVED3),              0,     0,       0.0f,      0.0f,       0.0f },
    { _GEN(SCALETUNE),              0,     1,       0.0f,   1200.0f,     100.0f },
    { _GEN(EXCLUSIVECLASS),         0,     0,       0.0f,      0.0f,       0.0f },
    { _GEN(OVERRIDEROOTKEY),        1,     0,       0.0f,    127.0f,      -1.0f },
    { _GEN(PITCH),                  1,     0,       0.0f,    127.0f,       0.0f },
    { _GEN(CUSTOM_BALANCE),         1,     0,    -960.0f,    960.0f,       0.0f },
    { _GEN(CUSTOM_FILTERFC),        1,     2,       0.0f,  22050.0f,       0.0f },
    { _GEN(CUSTOM_FILTERQ),         1,     1,       0.0f,    960.0f,       0.0f }
};

// clang-format on


#endif // FSM_GEN_H 

