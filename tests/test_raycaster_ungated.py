#!/usr/bin/env python3
"""U3 raycaster must not gate EOP labels; U2 guest-copy landing remains gated.

Source integration test. Frame-rate recovery requires the same-scene device A/B test.
"""
from pathlib import Path
base = Path(__file__).resolve().parents[1] / "gpu/shadps4/video_core"
b = (base/"buffer_cache/buffer_cache.cpp").read_text()
h = (base/"buffer_cache/buffer_cache.h").read_text()
g = (base/"landing_gate.h").read_text()
l = (base/"amdgpu/liverpool.cpp").read_text()
r = (base/"renderer_vulkan/vk_rasterizer.cpp").read_text()
start = b.index("bool BufferCache::DeferCopy64Readback(")
stop = b.index("bool BufferCache::QueueGuestSnapshot(", start)
assert "QueueGuestSnapshot(address, address, size, false, false)" in b[start:stop]
u2 = b[b.index("bool BufferCache::DeferGuestCopy("):b.index("bool BufferCache::DeferSmallReadback(")]
assert "QueueGuestSnapshot(src, dst, size)" in u2
assert "bool gate_guest_label = true" in h
assert "if (gate_guest_label)" in b
assert "landing_gated_registered.store(landing->seq" in b
assert "BufferCache::PendingGatedLanding() const" in b
assert "PendingGatedLanding() != 0" in b
assert "return cache ? cache->PendingGatedLanding() : 0;" in b
assert "return cache ? cache->PendingLanding() : 0;" not in b
assert "LandingGate::Pending()" in l
assert "must not stall every EOP/flip label" in g
assert "gated_events=%llu" in r
assert "event % 8192 == 0" in r
print("PASS: U3 snapshots ungated, U2 required landings gated, EOP/flip labels no longer inherit U3 snapshots")
