#!/usr/bin/env python3
"""Exercise production UI turn/accel code without building the application."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "app/ui.c").read_text()


def function(signature):
    start = source.index(signature)
    body = source.index("{", start)
    depth = 0
    for pos in range(body, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[start:pos + 1]
    raise AssertionError("Unterminated production function")


pseudo = re.search(r"enum \{ K_LEDS = .*?\};", source).group(0)
views = re.search(r"enum \{ V_GARDEN, .*?\};", source).group(0)
knobs = re.search(r"static const uint8_t VIEW_KNOB\[NVIEWS\]\[4\] = \{.*?\n\};", source, re.S).group(0)
labels = re.search(r"static const char \*const KNOB_LABEL\[NVIEWS\]\[4\] = \{.*?\n\};", source, re.S).group(0)
harness = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app.h"
PSEUDO
@@VIEWS@@
@@KNOBS@@
@@LABELS@@
static struct { uint32_t enc_t[NE]; uint8_t enc_run[NE]; } ui;
static int values[256], writes, ranges, dirty;
static uint32_t clock_ms = 1000;
uint32_t plat_ms(void) { return clock_ms += 10; }
void en_evo_new(void) {}
void en_fmevo_new(void) {}
static void perf_reset(void) {}
static void mark_dirty(void) { ++dirty; }
static void say(const char *a, const char *b) { (void)a; (void)b; }
static int knob_get(int k) { return values[k]; }
static void knob_range(int k, int *lo, int *hi)
{ ++ranges; *lo = 0; *hi = k == P_RMODEL ? 6 : 100; }
static void knob_set(int k, int value) { values[k] = value; ++writes; }
ACCEL
TURN
int main(void)
{
    const int old_to_new[] = {0, 1, 2, 2, 1, 2, 2};
    unsigned cases = 0;
    assert(RINGS_MODEL_COUNT == 3);
    assert(APP_CANVAS_PX == 240u * 132u);
    assert(sizeof(uint16_t) * APP_CANVAS_PX == 63360u);
    for (int saved = 0; saved <= 6; ++saved)
        for (int delta = -8; delta <= 8; ++delta) {
            values[P_RMODEL] = saved; writes = ranges = 0;
            turn(0, P_RMODEL, delta);
            if (!delta) {
                assert(!writes && values[P_RMODEL] == saved);
            } else {
                int expected = old_to_new[saved] + delta;
                if (expected < 0) expected = 0;
                if (expected > 2) expected = 2;
                assert(writes == 1 && !ranges);
                assert(values[P_RMODEL] == expected);
            }
            ++cases;
        }
    values[P_RMODEL] = 0;
    for (int expected = 1; expected <= 2; ++expected) {
        turn(0, P_RMODEL, 1); assert(values[P_RMODEL] == expected);
    }
    for (int expected = 1; expected >= 0; --expected) {
        turn(0, P_RMODEL, -1); assert(values[P_RMODEL] == expected);
    }
    memset(&ui, 0, sizeof ui); values[P_PAD] = 50;
    turn(0, P_PAD, 1); assert(values[P_PAD] == 51);
    assert(VIEW_KNOB[V_RINGEXC][2] == K_NONE);
    assert(!KNOB_LABEL[V_RINGEXC][2][0]);
    values[P_RFX] = 4; writes = ranges = dirty = 0;
    for (int delta = -8; delta <= 8; ++delta)
        turn(2, VIEW_KNOB[V_RINGEXC][2], delta);
    assert(!writes && !ranges && !dirty && values[P_RFX] == 4);
    printf("PASS %u production Model turn cases: three choices, legacy fallbacks, limits, multi-detents, zero input\n", cases);
    puts("PASS forward/backward traversal and unrelated parameter turn");
    puts("PASS Rings Exciter K3 is blank/inactive; turns preserve legacy String FX value");
    puts("PASS app-specific canvas capacity 31,680 pixels / 63,360 bytes");
    return 0;
}
'''
harness = (harness.replace("PSEUDO", pseudo)
           .replace("@@VIEWS@@", views).replace("@@KNOBS@@", knobs).replace("@@LABELS@@", labels)
           .replace("ACCEL", function("static int32_t accel("))
           .replace("TURN", function("static void turn(")))
with tempfile.TemporaryDirectory(prefix="genamb-ui-model-") as tmp:
    tmp = Path(tmp)
    cfile = tmp / "ui-model.c"
    cfile.write_text(harness)
    binary = tmp / "ui-model"
    command = shlex.split(os.environ.get("CC", "cc")) + [
        "-std=c99", "-O1", "-g", "-Wall", "-Wextra", "-fsanitize=undefined",
        "-fno-sanitize-recover=all", "-I" + str(root / "app"),
        "-I" + str(root / "app/dsp"), "-I" + str(root / "base/include"),
        str(cfile), "-o", str(binary),
    ]
    subprocess.run(command, check=True)
    subprocess.run([str(binary)], check=True)
