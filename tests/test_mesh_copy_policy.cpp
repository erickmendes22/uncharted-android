#include "bbport_mesh_copy_policy.h"
#include <cassert>
#include <cstdio>
int main(){
    using BbMeshCopy::NeedsGuestResult;
    assert(NeedsGuestResult(64,true,true));
    assert(!NeedsGuestResult(64,true,false));
    assert(!NeedsGuestResult(64,false,true));
    assert(!NeedsGuestResult(1,true,true));
    puts("PASS: CPU raycaster records get results; GPU-only and unrelated copies retain baseline policy");
}
