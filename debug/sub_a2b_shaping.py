#!/usr/bin/env python3
"""sub_a2b_shaping - an a2b cable actually shapes the traffic it carries, not
just accepts the setting. Two hosts on one wire made of two lans joined by an
a2b: a clean baseline ping, then delay added in both directions lifts the
round trip, then loss added in both directions drops a measurable share, and
clearing the cable brings the baseline back.

The thresholds are deliberately wide and every one is an env var, so a run
that comes out just outside a bound is tuned without touching the code:

    A2B_DELAY_MS     one-way delay set each direction        (default 100)
    A2B_DELAY_MIN    least round-trip rise accepted, ms      (default 120)
    A2B_DELAY_MAX    most round-trip rise accepted, ms       (default 2000)
    A2B_DELAY_PINGS  pings for the delay measure             (default 6)
    A2B_LOSS_VAL     loss set each direction, val=100 is 1%  (default 5000)
    A2B_LOSS_MIN     least loss percent accepted             (default 15)
    A2B_LOSS_MAX     most loss percent accepted              (default 99)
    A2B_LOSS_PINGS   pings for the loss measure              (default 100)
    A2B_BASE_PINGS   pings for the baseline and the recovery (default 10)
"""
import os
from sublib import *

A, B = "tisubAa", "tisubAb"
LAN_A, LAN_B = "tisubAla", "tisubAlb"
CABLE = "tisubAc"


def env_i(name, default):
    try:
        return int(os.environ.get(name, default))
    except ValueError:
        return default


DELAY_MS = env_i("A2B_DELAY_MS", 100)
DELAY_MIN = env_i("A2B_DELAY_MIN", 120)
DELAY_MAX = env_i("A2B_DELAY_MAX", 2000)
DELAY_PINGS = env_i("A2B_DELAY_PINGS", 6)
LOSS_VAL = env_i("A2B_LOSS_VAL", 5000)
LOSS_MIN = env_i("A2B_LOSS_MIN", 15)
LOSS_MAX = env_i("A2B_LOSS_MAX", 99)
LOSS_PINGS = env_i("A2B_LOSS_PINGS", 100)
BASE_PINGS = env_i("A2B_BASE_PINGS", 10)


def impair(cmd, val):
    for d in (0, 1):
        way = ("A2B", "B2A")[d]
        must(cli("cnf", CABLE, way, "%s=%d" % (cmd, val)),
             "cnf %s %s %s=%d" % (CABLE, way, cmd, val))


def clean_cable():
    for cmd in ("delay", "loss", "rate", "silentms"):
        for d in (0, 1):
            cli("cnf", CABLE, ("A2B", "B2A")[d], "%s=0" % cmd,
                quiet=True)


def sub_a2b_shaping():
    clean(A, B, CABLE)
    try:
        add_cnt(A, "s")
        add_cnt(B, "s")
        must(cli("add", "a2b", CABLE), "add a2b")
        must(cli("plug", A, "eth0", LAN_A), "plug a")
        must(cli("plug", CABLE, "A", LAN_A), "cable side A on lan a")
        must(cli("plug", B, "eth0", LAN_B), "plug b")
        must(cli("plug", CABLE, "B", LAN_B), "cable side B on lan b")
        wait_answer(A)
        wait_answer(B)
        address(A, 0, "10.60.0.1/24")
        address(B, 0, "10.60.0.2/24")
        clean_cable()

        base_loss, base_avg = ping_stats(A, "10.60.0.2", BASE_PINGS)
        if base_loss != 0 or base_avg is None:
            raise Fail("the clean cable does not carry a ping: %d%% loss, "
                       "rtt %s" % (base_loss, base_avg))
        print("      clean cable: %d%% loss, round trip %.1f ms"
              % (base_loss, base_avg))

        print("      delay: %d ms each direction" % DELAY_MS)
        impair("delay", DELAY_MS)
        d_loss, d_avg = ping_stats(A, "10.60.0.2", DELAY_PINGS)
        if d_avg is None:
            raise Fail("with the delay set the ping got no reply (%d%% loss)"
                       % d_loss)
        rise = d_avg - base_avg
        print("      delayed: round trip %.1f ms, a rise of %.1f ms (%d%% loss)"
              % (d_avg, rise, d_loss))
        if rise < DELAY_MIN or rise > DELAY_MAX:
            raise Fail("delay %d ms each way lifted the round trip by %.1f ms, "
                       "wanted %d..%d ms" % (DELAY_MS, rise, DELAY_MIN, DELAY_MAX))
        impair("delay", 0)

        print("      loss: val=%d each direction" % LOSS_VAL)
        impair("loss", LOSS_VAL)
        l_loss, _ = ping_stats(A, "10.60.0.2", LOSS_PINGS)
        print("      lossy: %d%% of %d pings lost" % (l_loss, LOSS_PINGS))
        if l_loss < LOSS_MIN or l_loss > LOSS_MAX:
            raise Fail("loss val=%d each way dropped %d%%, wanted %d..%d%%"
                       % (LOSS_VAL, l_loss, LOSS_MIN, LOSS_MAX))
        impair("loss", 0)

        clean_cable()
        r_loss, r_avg = ping_stats(A, "10.60.0.2", BASE_PINGS)
        print("      cleared cable: %d%% loss, round trip %s ms"
              % (r_loss, "%.1f" % r_avg if r_avg is not None else "none"))
        if r_loss != 0:
            raise Fail("the cable stayed lossy after clearing it: %d%%" % r_loss)

        delete(A)
        delete(B)
        delete(CABLE)
    finally:
        clean(A, B, CABLE)


if __name__ == "__main__":
    run("a2b shaping", sub_a2b_shaping)
