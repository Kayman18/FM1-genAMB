/* Control-rate production checks; excluded from FM1 firmware. */
void pad_roles_tests(void)
{
    par[P_MIDI]=0;par[P_PAD2OCT]=0;par[P_PAD2RANGE]=0;par[P_PAD2CHANCE]=0;
    par[P_ROOT]=2;pool=EN_MODE_POOL[MODE_AEOLIAN];scale_set();
    deg=0;pad_retarget();
    assert(pad2_deg==deg && pad2_chord_nt==chord_nt);
    for(int i=0;i<chord_nt;i++)assert(pad2_chord_pcs[i]==chord_pcs[i]);
    swell=60.0f;pad_breathe(&pad[0],0);
    float seconds=32.0f/(pad[0].rate*EN_SR);
    assert(seconds>=17.0f && seconds<=23.1f);
    pad2_swell=60.0f;en_playing=1;
    for(int i=0;i<4;i++){
        pad2[i].t=0.0f;pad2[i].peak=0.0f;pad2[i].midi_note=0;
        pad2[i].hand=0;pad2[i].hf=1.0f;
        pad2_rest[i]=(2.0f+1.75f*(float)i)*EN_SR;
    }
    int sum_active=0,silent=0,starts=0,max_active=0,prev[4]={0};
    const int ticks=180*44100/128;
    for(int t=0;t<ticks;t++){
        int active=0;
        for(int i=0;i<4;i++){
            pad2_coefs_lite(&pad2[i],i);
            int note=pad2[i].midi_note?pad2[i].emitted_note:0;
            if(note){
                int in_chord=0;active++;
                assert(note>=60 && note<=71);
                for(int j=0;j<chord_nt;j++)in_chord|=note%12==chord_pcs[j];
                assert(in_chord);
                for(int j=0;j<i;j++)assert(!pad2[j].midi_note || pad2[j].emitted_note!=note);
                if(!prev[i])starts++;
            }
            prev[i]=note;
        }
        assert(active<=2);if(active>max_active)max_active=active;
        sum_active+=active;silent+=active==0;
    }
    assert(starts>10 && max_active==2 && silent>ticks/100);
    assert(sum_active<ticks*17/10);
    deg=3;pad_retarget();
    assert(pad2_deg==deg && pad2_chord_nt==chord_nt);
    for(int i=0;i<chord_nt;i++)assert(pad2_chord_pcs[i]==chord_pcs[i]);
    printf("PASS pad roles: Pad1 cycle %.2fs; Pad2 180s: %d starts, %.3f mean active, %.1f%% full silence, max %d, upper chord notes unique, harmony follows\n",seconds,starts,(double)sum_active/ticks,100.0*(double)silent/ticks,max_active);
}
