void fsm_dsp64(t_fsm* x, t_object* dsp64, short* count, double samplerate,
               long maxvectorsize, long flags)
{

    sysmem_freeptr(x->left_buffer);
    sysmem_freeptr(x->right_buffer);

    x->left_buffer = sysmem_newptr(sizeof(double) * maxvectorsize);
    x->right_buffer = sysmem_newptr(sizeof(double) * maxvectorsize);

    memset(x->left_buffer, 0.f, sizeof(double) * maxvectorsize);
    memset(x->right_buffer, 0.f, sizeof(double) * maxvectorsize);

    object_method(dsp64, gensym("dsp_add64"), x, fsm_perform64, 0, NULL);
}

void fsm_perform64(t_fsm* x, t_object* dsp64, double** ins, long numins,
                   double** outs, long numouts, long sampleframes, long flags,
                   void* userparam)

{
    t_double* bufL = x->left_buffer;
    t_double* bufR = x->right_buffer;
    t_double* outL = outs[0];
    t_double* outR = outs[1];
    int n = (int)sampleframes;

    fluid_synth_write_float(x->synth, n, bufL, 0, 1, bufR, 0, 1);

    while (n--) {
        *outL++ = *bufL++;
        *outR++ = *bufR++;
    }
}

void fsm_dsp64(t_fsm* x, t_object* dsp64, short* count, double samplerate,
               long maxvectorsize, long flags)
{
        sysmem_freeptr(x->left_buffer);
        sysmem_freeptr(x->right_buffer);

        x->left_buffer = sysmem_newptr(sizeof(double) * maxvectorsize);
        x->right_buffer = sysmem_newptr(sizeof(double) * maxvectorsize);

        memset(x->left_buffer, 0.f, sizeof(double) * maxvectorsize);
        memset(x->right_buffer, 0.f, sizeof(double) * maxvectorsize);

    object_method(dsp64, gensym("dsp_add64"), x, fsm_perform64, 0, NULL);
}

void fsm_perform64(t_fsm* x, t_object* dsp64, double** ins, long numins,
                   double** outs, long numouts, long sampleframes, long flags,
                   void* userparam)

{
    double *dry[1 * 2], *fx[1 * 2];
    dry[0] = x->left_buffer;
    dry[1] = x->right_buffer;
    fx[0] = x->left_buffer;
    fx[1] = x->right_buffer;
    int n = (int)sampleframes;

    err = fluid_synth_process(x->synth, n, 2, fx, 2, dry);

}





