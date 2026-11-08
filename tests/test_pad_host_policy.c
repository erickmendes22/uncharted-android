/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "runtime_pad_host_policy.h"
#include <assert.h>

int main(void) {
    /* Native virtual pad + physical Xbox/DualSense must both remain live. */
    assert(bb_pad_scripted_only("touch-pad-state.txt","1","1")==0);
    assert(bb_pad_scripted_only("touch-pad-state.txt",0,"1")==0);
    assert(bb_pad_scripted_only("touch-pad-state.txt","0","1")==0);
    /* Original automated replay must stay deterministic, without host pad. */
    assert(bb_pad_scripted_only("recorded-pad.txt",0,0)==1);
    assert(bb_pad_scripted_only("recorded-pad.txt","0",0)==1);
    assert(bb_pad_scripted_only("recorded-pad.txt","1",0)==0);
    assert(bb_pad_scripted_only(0,0,0)==0);
    assert(bb_pad_scripted_only("",0,0)==0);
    return 0;
}
