#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
static pthread_mutex_t m=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv=PTHREAD_COND_INITIALIZER;
static pthread_t thread;
static int started,busy;
static void (*job)(void*);
static void *arg;
static void *worker(void *unused) {
 (void)unused;
 pthread_mutex_lock(&m);
 for(;;) {
  while(!busy)pthread_cond_wait(&cv,&m);
  void (*f)(void*)=job;void *a=arg;
  pthread_mutex_unlock(&m);f(a);pthread_mutex_lock(&m);
  busy=0;pthread_cond_broadcast(&cv);
 }
 return 0;
}
unsigned dc_status(void){return getenv("DC_SINGLE")?1:2;}
unsigned dc_stage(void){return dc_status()==2?3:0;}
int dc_submit(void (*f)(void*),void*a){
 if(dc_status()==1)return 0;
 pthread_mutex_lock(&m);
 if(!started){if(pthread_create(&thread,0,worker,0))abort();started=1;}
 if(busy)abort();job=f;arg=a;busy=1;pthread_cond_broadcast(&cv);pthread_mutex_unlock(&m);return 1;
}
int dc_wait(void){pthread_mutex_lock(&m);while(busy)pthread_cond_wait(&cv,&m);pthread_mutex_unlock(&m);return 1;}
/* v20 UI displays a reason only on the hardware fault path. */
unsigned dc_fault(void){return 0;}
