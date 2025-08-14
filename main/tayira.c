#include "renderer/renderer.h"
#include <stdlib.h>

int update(renderer_ctx ctx) {
    renderer_fill(ctx, 1, 1, 1);
    return 0;
}

int main() {
    renderer_ctx ctx = renderer_init(720, 480, "Tayira - Echoes of the Crimson Sea");
    if (ctx == NULL) {
        return 1;
    }
    
    renderer_run(ctx, update);

    renderer_cleanup(ctx);
}
