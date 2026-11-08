#!/usr/bin/env python3
"""Static + integration regression for worker scheduling backported from
revvv991/uncharted-android 0.3.0, based on measured G2 A/B benchmarks.
Actual FPS still requires comparable on-device logs; never infer device FPS.
"""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
def source(path): return (root / path).read_text()

probe = source("src/probe.c")
activity = source("android/scripts/g2/game_app/GameActivity.java")
compat = source("android/scripts/g2/game_app/ArmCompatibility.java")
spin = source("src/runtime_jobspin.c")
pad = source("src/runtime_pad.c")
touch = source("android/scripts/g2/game_app/TouchControllerOverlay.java")
upscaler = source("gpu/shadps4/video_core/renderer_vulkan/vk_temporal_upscaler.cpp")

def function(name):
    start = probe.index("static void " + name + "(")
    left = probe.index("{", start)
    depth = 0
    for i in range(left, len(probe)):
        if probe[i] == "{": depth += 1
        if probe[i] == "}":
            depth -= 1
            if depth == 0: return probe[start:i+1]
    raise AssertionError("unclosed " + name)

sleep = function("patch_job_sleep")
wake = function("patch_job_resume_wake")
u1guard = function("install_u1_set_job_guard")
u3guard = function("install_u3_particle_job_guard")

# No default patch on unknown game executable: exact 3-site signatures for
# each supported build MUST pass first, then all 3 are patched as a group.
assert sleep.index("if (i<3)") < sleep.index("for (i=0;i<3;++i) {\n            memcpy(")
assert sleep.count("find_unique_pattern(image_size,builds[b][i].pattern)") == 1
assert '"worker loop"' in sleep and '"re-check before waiting"' in sleep
assert '"job submission wakes them"' in sleep and "static const struct Patch builds[][3]" in sleep
assert "498b4500 80785900" in sleep  # retail 1.02
assert "498b07 80785900" in sleep    # 1.00
assert 'BB_JOB_SLEEP' in sleep and "if (env && env[0]=='0')" in sleep
assert "find_unique_pattern(image_size,\"4d8d7001 807f5900 9090 4981f802000000" in sleep  # existing U2 recipe

# Guard against partial installation: a matched sleep loop alone is insufficient.
assert "Job workers: incomplete 2-spin/wake signatures; keeping original workers" in sleep
assert sleep.index("const int64_t wait_site=") < sleep.index("for (i=0;i<3;++i) {\n            memcpy(")
assert sleep.index("if (!have_broadcast || resume_site1<0 || resume_site2<0)") < sleep.index("for (i=0;i<3;++i) {\n            memcpy(")

# Validated 2-spin/1 ms timer replacement and U3/U1 fast fiber wake.
assert 'const long spins=spins_env?strtol(spins_env,NULL,10):2;' in sleep
assert "spins>0 && spins<1000" in sleep
assert "const uint32_t n=(uint32_t)spins" in sleep
assert "memcpy(image+at[0]+13,&n,4)" in sleep
assert '"BB_JOB_WAIT_US"' in sleep and '"4883c060 ba10270000' in sleep
assert "patch_job_resume_wake(image_size,at[2])" in sleep
assert "find_unique_pattern(image_size,\"48ff04fe" in wake
assert "find_unique_pattern(image_size,\"48ff04f2" in wake
assert "if (s1<0 || s2<0)" in wake
assert "near_image_memory(512)" in wake
assert "BB_JOB_RESUME_WAKE" in wake
assert "t[skip]=(unsigned char)(k-(skip+1))" in wake
assert "k+=jump_abs(t+k" in wake
assert "site[0]=0xe9" in wake

# Safety for late frame jobs: each patch verifies its exact guest bytes and
# provides an individual kill switch.
assert "BB_U1_SET_GUARD" in u1guard
assert "BB_U3_PARTICLE_GUARD" in u3guard
assert "memcmp(image + job - base, job_old, sizeof(job_old))" in u1guard
assert "memcmp(image + job - base, job_old, sizeof(job_old))" in u3guard
assert "memcmp(image + load_at - base, load_old, sizeof(load_old))" in u3guard
assert "near_image_memory(64)" in u3guard
assert "install_u1_set_job_guard(image_end);" in probe
assert "install_u3_particle_job_guard(image_end);" in probe
assert probe.index("patch_job_sleep(image_end)") < probe.index("patch_job_lock_wait(image_end)")
assert probe.index("patch_job_sleep(image_end)") < probe.index("protect(image, round_page(size), 0)")

# Existing launcher option now governs BOTH new upstream workers and the
# existing FEX instruction hook fallback.
assert '"BB_JOB_SPIN","BB_JOB_SLEEP","BB_JOB_SPINS","BB_JOB_WAIT_US","BB_JOB_RESUME_WAKE"' in compat
for entry in ["BB_JOB_SLEEP=", "BB_JOB_SPINS=", "BB_JOB_WAIT_US=1000", "BB_JOB_RESUME_WAKE="]:
    assert entry in compat
assert 'boolean upstreamWorkers=idleSleep && !"u2".equals(game);' in compat
assert 'upstreamWorkers?"1":"0"' in compat and 'upstreamWorkers?"2":"0"' in compat
assert 'BB_JOB_SPIN="+(idleSleep?"256,20":"0")' in compat
assert "runtime_job_spin_install" in spin

# No control, movie, or FSR code changed in this dedicated performance PR.
assert 'bb_pad_scripted_only(file,host,java_touch)' in pad
assert 'pad.virtual(pressed,lx,ly,rx,ry,left,right,l2,r2)' in touch
assert '"BB_PAD_HOST", "1"' in activity
assert 'BB_U2_MOVIE_FSR_BYPASS' in upscaler
print("PASS: upstream worker 2-spin/wake, 1.00/1.02 signatures, U1/U3 guards, runtime toggle, old hook fallback, input/FSR preserved")
