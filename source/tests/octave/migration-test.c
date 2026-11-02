#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../app/app.h"
#include "../../app/project.c"
static unsigned char storage[4096];static int size;
int plat_store_load(uint32_t obj,void *dst,uint32_t max){assert(size<=(int)max);memcpy(dst,storage,size);return size;}
int plat_store_save(uint32_t obj,const void *src,uint32_t len){assert(len<4096);memcpy(storage,src,len);size=len;return 0;}
int main(void){
 project_defaults(); project_t reference=proj; project_v14_t old={0};
 old.magic=PROJ_MAGIC;old.format=14;old.pool=proj.pool;old.leds=proj.leds;old.piece=proj.piece;
 for(int p=0;p<V14_NPARAMS;p++)old.par[p]=proj.par[p];
 for(int i=0;i<NPIECES;i++){old.slot[i].pool=proj.slot[i].pool;for(int p=0;p<V14_NPARAMS;p++)old.slot[i].par[p]=proj.slot[i].par[p];}
 plat_store_save(0,&old,sizeof old);assert(project_load()==0);assert(proj.format==15);
 for(int p=0;p<V14_NPARAMS;p++)assert(proj.par[p]==reference.par[p]);
 for(int i=0;i<NPIECES;i++)for(int p=0;p<V14_NPARAMS;p++)assert(proj.slot[i].par[p]==reference.slot[i].par[p]);
 for(int p=P_PADRANGE;p<P_NPARAMS;p++){assert(proj.par[p]==0);for(int i=0;i<NPIECES;i++)assert(proj.slot[i].par[p]==0);}
 proj.par[P_PADRANGE]=2;proj.par[P_PADCHANCE]=31;proj.par[P_PAD2RANGE]=1;proj.par[P_PAD2CHANCE]=42;proj.par[P_RINGRANGE]=2;proj.par[P_RINGCHANCE]=73;
 assert(project_save()==0);memset(&proj,0,sizeof proj);assert(project_load()==0);
 assert(proj.par[P_PADCHANCE]==31&&proj.par[P_PAD2CHANCE]==42&&proj.par[P_RINGCHANCE]==73);
 assert(proj.par[P_PADRANGE]==2&&proj.par[P_PAD2RANGE]==1&&proj.par[P_RINGRANGE]==2);
 puts("PASS: format 14 migration preserves every old parameter and slot; new controls default to zero and persist independently.");
}
