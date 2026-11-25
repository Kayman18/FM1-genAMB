"""Exercise the production 44.1 kHz USB capture ring with the 512-frame DMA timer."""
from pathlib import Path
from tempfile import TemporaryDirectory
import subprocess

source = (Path(__file__).resolve().parents[2] / "base/bsp/chip/usb.c").read_text()
# Compile the actual producer, consumer, state, and size/priming definitions
# independently of the board's USB registers and pi32 memory barrier.
state = source[source.index("#if HALF_FRAMES == 512u"):source.index("} uac;") + len("} uac;")]
handlers = source[source.index("static __attribute__((noinline)) void uac_render_start(void)"):
                  source.index("/* TIMER5, 2 kHz, also nested in the render")]
test = r'''
#include <assert.h>
#include <stdio.h>
static uint32_t generated, consumed, silence, rate;
static uint64_t packet_time;

/* Nonzero ramps on both channels expose a dropped, repeated, or swapped frame. */
static uint32_t word(uint32_t i) {
    uint32_t l = 1u + i % 30000u;
    return l | (uint32_t)(uint16_t)-(int32_t)l << 16;
}
static void packets_through(uint64_t t) {
    while (packet_time <= t) {
        uint32_t packet[UA_MAXF], count = uac_packet(packet);
        for (uint32_t i = 0; i < count; ++i) {
            if (silence) { assert(packet[i] == 0); --silence; }
            else { assert(packet[i] == word(consumed)); ++consumed; }
        }
        packet_time += rate;
    }
}
int main(int argc, char **argv) {
    assert(argc == 2);
    rate = (uint32_t)strtoul(argv[1], NULL, 10);
    usb.config = uac.alt = uac.flowing = 1;
    silence = UA_PRIME;
    for (unsigned block = 0; block < 16000; ++block) {
        uint64_t t = (uint64_t)block * 512000u;
        if (block) packets_through(t);
        uac_render_start();
        /* Two DSP quanta, with asymmetric loads and finishes up to 99.8% of budget. */
        unsigned first = 168u + (block % 15u) * 13u;
        unsigned second = 440u + block % 72u;
        for (unsigned half = 0; half < 2; ++half) {
            int32_t samples[512];
            packets_through(t + 1000u * (half ? second : first));
            for (unsigned i = 0; i < 256; ++i) {
                uint32_t stereo = word(generated++);
                samples[i*2] = (int16_t)stereo * 256;
                samples[i*2+1] = (int16_t)(stereo >> 16) * 256;
            }
            uac_tap(samples, 256);
        }
    }
    assert(consumed + ua_w - ua_r == generated);
    assert(uac.underruns == 0 && uac.overruns == 0);
    printf("PASS %u Hz simulated I2S, %u contiguous USB stereo frames, %u underruns, %u overruns\n",
           rate, consumed, uac.underruns, uac.overruns);
}
'''
preamble = """
#include <stdint.h>
#include <stdlib.h>
#define HALF_FRAMES 512u
#define FELUCCA_UAC_48K 0
#define RING_PUBLISH() ((void)0)
static struct { int config; } usb;
"""
with TemporaryDirectory() as out:
    c_file = Path(out) / 'test.c'
    binary = Path(out) / 'test'
    c_file.write_text(preamble + state + "\n" + handlers + test)
    subprocess.run(["cc", "-std=c11", "-O2", str(c_file), "-o", str(binary)], check=True)
    for clock in ("44090", "44100", "44118"):
        subprocess.run([str(binary), clock], check=True)
