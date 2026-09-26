#!/usr/bin/env python3
"""sub_position_readback - cnf x= y= and angle= are stored: a subscriber to the
topology (event_topo_sub, what the gui sends) is replayed item_pos x y and
item_poseth deg for the item; a deleted item is forgotten.
"""
from sublib import *

C = "tisubPos"
SUB = '{"fctident":"event_topo_sub","tid":0}'


def layout():
    pos, eth = {}, {}
    for m in raw_rpc(SUB):
        if m.get("fctident") == "item_pos":
            pos[m.get("name")] = (m.get("x"), m.get("y"))
        elif m.get("fctident") == "item_poseth":
            eth[(m.get("name"), m.get("num"))] = m.get("deg")
    return pos, eth


def sub_position_readback():
    clean(C)
    try:
        add_cnt(C, "ss")
        must(cli("cnf", C, "x=-100", "y=50"), "position")
        must(cli("cnf", C, "eth1", "angle=90"), "poseth")
        pos, eth = layout()
        if pos.get(C) != (-100, 50):
            raise Fail("position -100 50 read back as %s" % (pos.get(C),))
        if eth.get((C, 1)) != 90:
            raise Fail("cnf eth1 angle=90 read back as %s" % eth.get((C, 1)))
        must(cli("cnf", C, "x=7", "y=-8"), "position again")
        pos, eth = layout()
        if pos.get(C) != (7, -8):
            raise Fail("position 7 -8 read back as %s" % (pos.get(C),))
        delete(C)
        pos, eth = layout()
        if C in pos or (C, 1) in eth:
            raise Fail("the layout of a deleted item is still replayed")
    finally:
        clean(C)


if __name__ == "__main__":
    run("position read back", sub_position_readback)
