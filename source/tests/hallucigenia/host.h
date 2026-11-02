/* Production engine integration checks; runs only in the host simulator. */
void hallucigenia_tests(void)
{
    en_evo_bank_t oldring,oldfm,got;
    int16_t savedpar[P_NPARAMS];memcpy(savedpar,par,sizeof par);
    en_evo_snapshot(&oldring);en_fmevo_snapshot(&oldfm);
    par[P_EPATTERN]=par[P_FMEPATTERN]=1;
    par[P_RSEQMODE]=par[P_FMSEQMODE]=1;
    evo_select();fm_seq_select();
    for(int i=0;i<16;i++){evo_bank.step[0][i]=0;fm_evo_bank.step[0][i]=0;}
    hall_load(&evo_core,evo_bank.step[0]);hall_load(&fm_evo_core,fm_evo_bank.step[0]);
    par[P_ELENGTH]=par[P_FMELENGTH]=2;
    par[P_EMUTATE]=par[P_FMEMUTATE]=100;
    par[P_EDENSITY]=par[P_FMEDENSITY]=100;
    evo_next=fm_evo_next=0;
    evo_tick();fm_evo_tick();assert(en_evo_pos==1 && en_fm_evo_pos==1);
    assert(!evo_core.on[15] && !fm_evo_core.on[15]);
    evo_tick();fm_evo_tick();assert(en_evo_pos==0 && en_fm_evo_pos==0);
    for(int i=0;i<16;i++)assert(evo_core.on[i] && fm_evo_core.on[i]);
    en_evo_snapshot(&got);assert(got.step[0][15]&128u);
    /* Both banks restore byte-for-byte, including the original 7-bit pitches. */
    en_evo_restore_boot(&oldring);en_evo_snapshot(&got);assert(!memcmp(&got,&oldring,sizeof got));
    en_fmevo_restore_boot(&oldfm);en_fmevo_snapshot(&got);assert(!memcmp(&got,&oldfm,sizeof got));
    for(int i=0;i<16;i++){
        assert(evo_core.note[i]==(float)(oldring.step[0][i]&127u)/128.f);
        assert(fm_evo_core.note[i]==(float)(oldfm.step[0][i]&127u)/128.f);
    }
    for(int root=0;root<12;root++)for(int follow=0;follow<2;follow++){
        par[P_ROOT]=root;par[P_EFOLLOW]=par[P_FMEFOLLOW]=follow;
        par[P_ERANGE]=par[P_FMERANGE]=3;
        for(int k=0;k<1024;k++){
            float value=(float)k/1024.f;
            int notes[2]={evo_pitch(value),fm_evo_pitch(value)};
            for(int layer=0;layer<2;layer++){
                int pc=(notes[layer]%12+12)%12,ok=0;
                if(follow){for(int j=0;j<chord_nt;j++)ok|=pc==chord_pcs[j];}
                else {for(int j=0;j<nsteps;j++)ok|=pc==(root+steps[j])%12;}
                assert(ok);
            }
        }
    }
    memcpy(par,savedpar,sizeof par);rings_wait_clear();fm_seq_stop();
    evo_next=evo_wrapped=fm_evo_next=fm_evo_wrapped=0;
    evo_select();fm_seq_select();
    puts("PASS production Rings + FM Hallucigenia: upstream first step, wrap mutation includes hidden steps, RC1 bank restore, 49152 scale/chord mappings");
}
