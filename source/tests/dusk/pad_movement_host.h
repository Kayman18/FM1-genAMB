void pad_movement_tests(void)
{
    float low[2][4],high[2][4],prev[2][4];int different=0;
    pad_movement_init();en_playing=1;motion=1.0f;
    for(int l=0;l<2;l++)for(int i=0;i<4;i++){
        low[l][i]=high[l][i]=prev[l][i]=pad_motion_wave[l][i];
    }
    for(int t=0;t<180*44100/256;t++){
        pad_movement_step(256);
        assert(pad_motion_amount>=0.0f && pad_motion_amount<=1.0f);
        for(int l=0;l<2;l++)for(int i=0;i<4;i++){
            float w=pad_motion_wave[l][i];
            assert(w>=-1.00001f && w<=1.00001f);
            assert(w-prev[l][i]<0.005f && w-prev[l][i]>-0.005f);
            if(w<low[l][i])low[l][i]=w;if(w>high[l][i])high[l][i]=w;
            prev[l][i]=w;
        }
        if(pad_motion_wave[0][0]-pad_motion_wave[1][0]>0.5f || pad_motion_wave[0][0]-pad_motion_wave[1][0]<-0.5f)different++;
    }
    for(int l=0;l<2;l++)for(int i=0;i<4;i++)assert(low[l][i]<-0.99f && high[l][i]>0.99f);
    assert(different>100);
    motion=0.0f;for(int i=0;i<5*44100/256;i++)pad_movement_step(256);
    assert(pad_motion_amount==0.0f);
    puts("PASS pad movement: 8 smooth bounded cycles over 180s, independent layers, Motion ramps and settles to exact zero");
}
