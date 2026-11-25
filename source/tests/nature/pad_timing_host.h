/* Called only by the host test command. Every slice must consume the
 * correct number of frames even when sequencer strikes are one frame apart. */
void pad_timing_tests(void)
{
    const unsigned sizes[] = {1, 7, 23, 1, 47, 127, 5, 255};
    en_init();
    pad_gain = pad_gate = pad2_gain = pad2_gate = 1.0f;
    for (int i=0; i<EN_NPAD; ++i) {
        pad[i].t = pad2[i].t = 0.125f;
        pad[i].rate = pad2[i].rate = 1.0f/4096.0f;
        pad[i].peak = pad2[i].peak = 0.5f;
        pad2_rest[i] = 0;
    }
    unsigned frames=0;
    for (unsigned slice=0; slice<sizeof sizes/sizeof *sizes; ++slice) {
        unsigned n=sizes[slice];
        render_pad(n);
        render_pad2(n);
        frames+=n;
        for (int voice=0; voice<EN_NPAD; ++voice) {
            assert(pad[voice].control_left==(32-frames%32)%32);
            assert(pad2[voice].control_left==(128-frames%128)%128);
            assert(pad[voice].t==0.125f+(float)((frames+31)/32)/4096.0f);
            assert(pad2[voice].t==0.125f+(float)((frames+127)/128)/4096.0f);
        }
    }
    quiet();
    for (int i=0;i<EN_NPAD;++i) assert(!pad[i].control_left && !pad2[i].control_left);
    /* Compare against the old integer tap at every active sample, across
     * phase wraps and rate changes, including the maximum safe step. */
    float phase=0.49f;
    pad_ch_hold=0;
    for (int i=0; i<2205000; ++i) {
        int wanted=(int)(115.0f*fm_sinf(phase*FM_TWO_PI));
        assert(pad_chorus_offset(phase)==wanted);
        float rate=(i/733)%2 ? 0.3f : 0.17f;
        phase+=rate*(1.0f/EN_SR);
        if (phase>=1.0f) phase-=1.0f;
    }
    puts("PASS pad control cadence across irregular 1..255 frame slices, panic reset and 2,205,000 exact chorus taps");
}
