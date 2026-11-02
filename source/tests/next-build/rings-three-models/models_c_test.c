#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include "rings_models.h"
int main(void) {
    const int saved[] = {0,1,2,2,1,2,2};
    int i;
    assert(RINGS_MODEL_COUNT == 3);
    for(i=0;i<7;i++) {
        assert(rings_model_index(i)==saved[i]);
        assert(rings_model_normalize(i)==saved[i]);
    }
    for(i=0;i<3;i++) assert(rings_model_from_index(i)==i);
    assert(rings_model_normalize(3)==2);
    assert(rings_model_index(3)==2);
    assert(rings_model_normalize(INT_MIN)==0);
    assert(rings_model_normalize(INT_MAX)==2);
    assert(rings_model_from_index(INT_MIN)==0);
    assert(rings_model_from_index(INT_MAX)==2);
    puts("PASS C99 model mapping and clamp endpoints");
    return 0;
}
