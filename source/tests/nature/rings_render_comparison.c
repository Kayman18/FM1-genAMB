#include "rings_port.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int main(int argc,char **argv) {
    int model=argc>1?atoi(argv[1]):0;
    uint64_t hash=1469598103934665603ull;
    float l[256],r[256],s[256];
    rings_port_init();rings_port_set_model(model);rings_port_set_level(85);
    for(unsigned b=0;b<2400;b++) {
        unsigned n=(b*137)%256+1;
        if(!(b%137))rings_port_set_polyphony(1+b/137%4);
        if(!(b%29))rings_port_trigger(36+b/29%48,65+b/29%62);
        if(!(b%19))rings_port_set_position(b/19*23%101);
        rings_port_set_modulation(.25f,RP_DEST_POSITION,b%251<125?11:0,-.5f,RP_DEST_STRUCTURE,0,.125f,RP_DEST_BRIGHTNESS,0);
        memset(l,0,sizeof l);memset(r,0,sizeof r);memset(s,0,sizeof s);
        rings_port_render_add(l,r,s,.6f,n);
        for(unsigned j=0;j<n;j++)for(unsigned ch=0;ch<3;ch++){
            uint32_t x;float val=ch==0?l[j]:ch==1?r[j]:s[j];
            memcpy(&x,&val,4);hash=(hash^x)*1099511628211ull;
        }
    }
    printf("%d %016llx\n",model,(unsigned long long)hash);
}
