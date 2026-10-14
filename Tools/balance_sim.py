#!/usr/bin/env python3
"""Balance simulator for Evolution Idle.

Mirrors the formulas in Source/EvolutionIdle/Private/EvoEconomy.cpp and plays the
campaign with a simple payback-greedy strategy, printing when each stage is reached.

Usage:  python3 Tools/balance_sim.py [path/to/GameData.json] [--cps 5] [--hours 48]
"""
import argparse
import json
import math
import os


def load(path):
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    data["stages"].sort(key=lambda s: s["id"])
    return data


class Economy:
    def __init__(self, data):
        self.d = data
        self.eco = data["economy"]
        self.era_mult = {e["id"]: e.get("costMultiplier", 1) for e in data["eras"]}
        self.current = 0.0
        self.lifetime = 0.0
        self.stage = 0  # index into stages
        self.levels = {u["id"]: 0 for u in data["upgrades"]}

    # --- queries (keep in sync with EvoEconomy.cpp) ---
    def cost(self, u, level=None):
        lvl = self.levels[u["id"]] if level is None else level
        scale = u.get("costScaling", 0) or self.eco["defaultCostScaling"]
        return math.floor(u["baseCost"] * scale ** lvl * self.era_mult.get(u["era"], 1))

    def unlocked(self, u):
        return u.get("unlockStage", 1) <= self.d["stages"][self.stage]["id"]

    def upgrade_mult(self, extra=None):
        total = 1.0
        for u in self.d["upgrades"]:
            if u["effect"] == "multiplier":
                total += (self.levels[u["id"]] + (1 if extra == u["id"] else 0)) * u["amount"]
        return total

    def stage_mult(self):
        return 1.0 + sum(s.get("productionBonus", 0) for s in self.d["stages"][: self.stage + 1])

    def mult(self, extra=None):
        return self.upgrade_mult(extra) * self.stage_mult()

    def base_eps(self, extra=None):
        return sum(
            (self.levels[u["id"]] + (1 if extra == u["id"] else 0)) * u["amount"]
            for u in self.d["upgrades"] if u["effect"] == "production")

    def eps(self, extra=None):
        return self.base_eps(extra) * self.mult(extra)

    def click(self, extra=None):
        base = self.eco["baseClickPower"] + sum(
            (self.levels[u["id"]] + (1 if extra == u["id"] else 0)) * u["amount"]
            for u in self.d["upgrades"] if u["effect"] == "click")
        return base * self.mult(extra)


def simulate(data, cps, max_hours, verbose=False):
    e = Economy(data)
    t = 0.0
    dt = 1.0
    reached = [(1, data["stages"][0]["name"], 0.0)]
    buys = 0
    while t < max_hours * 3600 and e.stage < len(data["stages"]) - 1:
        rate = e.eps() + e.click() * cps
        nxt = data["stages"][e.stage + 1]
        # Evolve as soon as affordable.
        if e.current >= nxt["cost"]:
            e.current -= nxt["cost"]
            e.stage += 1
            reached.append((nxt["id"], nxt["name"], t))
            continue
        # Otherwise pick the upgrade with the best payback.
        best = None
        for u in data["upgrades"]:
            if not e.unlocked(u):
                continue
            gain = (e.eps(u["id"]) + e.click(u["id"]) * cps) - rate
            if gain <= 0:
                continue
            payback = e.cost(u) / gain
            if best is None or payback < best[0]:
                best = (payback, u)
        time_to_stage = (nxt["cost"] - e.current) / rate if rate > 0 else float("inf")
        if best and best[0] < max(120.0, time_to_stage) and e.current >= e.cost(best[1]):
            e.current -= e.cost(best[1])
            e.levels[best[1]["id"]] += 1
            buys += 1
            continue
        e.current += rate * dt
        e.lifetime += rate * dt
        t += dt
    return reached, e, buys, t


def fmt_time(s):
    h, rem = divmod(int(s), 3600)
    m, sec = divmod(rem, 60)
    return f"{h:d}h {m:02d}m {sec:02d}s"


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser()
    ap.add_argument("path", nargs="?", default=os.path.join(here, "..", "Content", "Data", "GameData.json"))
    ap.add_argument("--cps", type=float, nargs="*", default=[0.5, 3.0], help="manual clicks per second profiles")
    ap.add_argument("--hours", type=float, default=72)
    args = ap.parse_args()
    data = load(args.path)
    for cps in args.cps:
        reached, e, buys, t = simulate(data, cps, args.hours)
        print(f"\n=== Profile: {cps} clicks/sec ===")
        prev = 0.0
        for sid, name, at in reached:
            print(f"  Stage {sid:2d} {name:<26} at {fmt_time(at)}  (+{fmt_time(at - prev)})")
            prev = at
        print(f"  Upgrades bought: {buys}; final EP/sec {e.eps():,.0f}; EP/click {e.click():,.0f}")
        if e.stage < len(data["stages"]) - 1:
            print(f"  STALLED: did not finish within {args.hours}h")


if __name__ == "__main__":
    main()
