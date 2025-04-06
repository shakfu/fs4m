void fm_perform64(t_fm* x, t_object* dsp64, double** ins, long numins, double** outs, long numouts, long sampleframes, long flags, void* userparam)
{
    double* left_out = outs[0];
    double* right_out = outs[1];
    float* dry[2];
    dry[0] = x->left_buffer;
    dry[1] = x->right_buffer;
    int n = (int)sampleframes;
    int err = 0;

    if (x->mute == 0) {
        err = fluid_synth_process(x->synth, n, 0, NULL, 2, dry);
        if(err == FLUID_FAILED)
            error("Problem writing samples");

        for (int i = 0; i < n; i++) {
            left_out[i] = dry[0][i];
            right_out[i] = dry[1][i];
        }

        memset(x->left_buffer, 0.f, sizeof(float) * x->out_maxsize);
        memset(x->right_buffer, 0.f, sizeof(float) * x->out_maxsize);

    } else {
        for (int i = 0; i < n; i++) {
            left_out[i] = right_out[i]= 0.0;
        }
    }
}
