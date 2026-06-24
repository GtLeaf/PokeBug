#!/usr/bin/env python3
"""Monte Carlo checks for the current PokeBug ATB battle balance.

This script mirrors the battle formulas currently used by:
  - src/game/BattleCalc.h
  - src/scenes/BattleScene.cpp

Notable assumptions:
  - Battle is ATB-style: whichever gauge fills first attacks.
  - Gauge speed is 45 + SPD * 3. MOT does not affect gauge speed.
  - A battle ends after 40 authoritative actions if nobody is knocked out.
  - Rhythm inputs are not simulated here; this isolates stat/temperament balance.

Examples:
  python3 spareAsset/scripts/simulate_battle_balance.py
  python3 spareAsset/scripts/simulate_battle_balance.py --trials 10000 --mot 100 50
  python3 spareAsset/scripts/simulate_battle_balance.py --matrix --seed 7
"""

from __future__ import annotations

import argparse
import itertools
import math
import random
from dataclasses import dataclass


ATTRS = ("SIZ", "STR", "END", "SPD", "SPI")
TEMPERAMENTS = ("SWIFT", "RESILIENT", "GIANT", "BRUTE", "BALANCED", "SPIRIT")

TEMPERAMENT_MULTIPLIERS = {
    #             SIZ   STR   END   SPD   SPI
    "SWIFT":     (0.90, 1.00, 1.00, 1.10, 1.00),
    "RESILIENT": (1.00, 1.00, 1.10, 1.00, 0.90),
    "GIANT":     (1.10, 1.00, 1.00, 0.90, 1.00),
    "BRUTE":     (1.00, 1.10, 0.90, 1.00, 1.00),
    "BALANCED":  (1.00, 1.00, 1.00, 1.00, 1.00),
    "SPIRIT":    (1.00, 0.90, 1.00, 1.00, 1.10),
}


@dataclass(frozen=True)
class BattleStats:
    siz: int
    str: int
    end: int
    spd: int
    spi: int
    mot: int = 100

    def values(self) -> tuple[int, int, int, int, int]:
        return self.siz, self.str, self.end, self.spd, self.spi


@dataclass
class FightResult:
    a_won: bool
    actions: int
    a_attacks: int
    b_attacks: int
    a_crits: int
    b_crits: int
    a_dodges: int
    b_dodges: int


@dataclass
class Aggregate:
    wins: int = 0
    fights: int = 0
    actions: int = 0
    attacks: int = 0
    crits: int = 0
    dodges: int = 0

    def add(self, won: bool, actions: int, attacks: int, crits: int, dodges: int) -> None:
        self.wins += int(won)
        self.fights += 1
        self.actions += actions
        self.attacks += attacks
        self.crits += crits
        self.dodges += dodges

    @property
    def win_rate(self) -> float:
        return self.wins / self.fights if self.fights else 0.0

    @property
    def avg_actions(self) -> float:
        return self.actions / self.fights if self.fights else 0.0

    @property
    def avg_attacks(self) -> float:
        return self.attacks / self.fights if self.fights else 0.0

    @property
    def crit_rate(self) -> float:
        return self.crits / self.attacks if self.attacks else 0.0

    @property
    def dodge_rate(self) -> float:
        return self.dodges / self.attacks if self.attacks else 0.0


def cpp_round(value: float) -> int:
    """Match C++ roundf for the positive values used by battle formulas."""
    if value >= 0.0:
        return int(math.floor(value + 0.5))
    return int(math.ceil(value - 0.5))


def clamp(value: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, value))


def compute_hp(stats: BattleStats) -> int:
    return 20 + stats.siz * 3 + stats.end * 2


def compute_mot_loss(spi: int, mot: int) -> int:
    loss = cpp_round(9.0 - spi / 3.0)
    loss = int(clamp(loss, 2, 9))
    if mot <= 50:
        loss = cpp_round(loss * 0.5)
    elif mot <= 70:
        loss = cpp_round(loss * 0.8)
    return max(1, loss)


def compute_attack(attacker: BattleStats,
                   defender: BattleStats,
                   rng: random.Random) -> tuple[int, bool, bool]:
    dodge_rate = 0.0
    spd_diff = defender.spd - attacker.spd
    if spd_diff > 0:
        dodge_rate += spd_diff * 2.0
    dodge_rate += defender.spi / 8.0
    dodge_rate = clamp(dodge_rate, 0.0, 22.0)
    if rng.random() * 100.0 < dodge_rate:
        return 0, False, True

    str_power = 1.5 + attacker.str * 0.80
    size_bonus = 1.0 + 0.45 * attacker.siz / (attacker.siz + 8.0)
    mot_mult = 0.75 + attacker.mot / 200.0
    raw = str_power * size_bonus * mot_mult

    crit_rate = 5.0 + attacker.spi * 2.2 + attacker.spd * 0.5 - defender.spi * 1.5
    crit_rate = clamp(crit_rate, 5.0, 55.0)
    crit = rng.random() * 100.0 < crit_rate
    crit_mult = 1.35 if crit else 1.0

    mitigation = 1.0 - min(0.14, defender.end * 0.0045)
    reduction = defender.end * 0.18
    damage = cpp_round(raw * crit_mult * mitigation - reduction)
    return max(1, min(255, damage)), crit, False


def with_mot(stats: BattleStats, mot: int) -> BattleStats:
    return BattleStats(stats.siz, stats.str, stats.end, stats.spd, stats.spi, mot)


def tempo_score(stats: BattleStats) -> int:
    return max(1, 45 + stats.spd * 3)


def fight_atb(a: BattleStats,
              b: BattleStats,
              rng: random.Random,
              max_actions: int = 40) -> FightResult:
    a_hp = compute_hp(a)
    b_hp = compute_hp(b)
    a_max_hp = a_hp
    b_max_hp = b_hp
    a_mot = a.mot
    b_mot = b.mot
    a_gauge = 0.0
    b_gauge = 0.0
    a_score = tempo_score(a)
    b_score = tempo_score(b)

    actions = 0
    a_attacks = b_attacks = 0
    a_crits = b_crits = 0
    a_dodges = b_dodges = 0

    while actions < max_actions and a_hp > 0 and b_hp > 0:
        a_time = (1.0 - a_gauge) / a_score
        b_time = (1.0 - b_gauge) / b_score
        if abs(a_time - b_time) < 1e-12:
            a_first = a_score >= b_score
            delta_time = a_time
        elif a_time < b_time:
            a_first = True
            delta_time = a_time
        else:
            a_first = False
            delta_time = b_time

        a_gauge = min(1.0, a_gauge + delta_time * a_score)
        b_gauge = min(1.0, b_gauge + delta_time * b_score)

        if a_first:
            damage, crit, dodged = compute_attack(with_mot(a, a_mot), with_mot(b, b_mot), rng)
            b_hp -= damage
            a_gauge = 0.0
            a_attacks += 1
            a_crits += int(crit)
            a_dodges += int(dodged)
        else:
            damage, crit, dodged = compute_attack(with_mot(b, b_mot), with_mot(a, a_mot), rng)
            a_hp -= damage
            b_gauge = 0.0
            b_attacks += 1
            b_crits += int(crit)
            b_dodges += int(dodged)

        a_hp = max(0, a_hp)
        b_hp = max(0, b_hp)
        a_mot = min(100, max(0, a_mot - compute_mot_loss(a.spi, a_mot)))
        b_mot = min(100, max(0, b_mot - compute_mot_loss(b.spi, b_mot)))
        actions += 1

    if a_hp > 0 and b_hp <= 0:
        a_won = True
    elif a_hp <= 0 and b_hp > 0:
        a_won = False
    else:
        a_won = (a_hp / a_max_hp) >= (b_hp / b_max_hp)

    return FightResult(a_won, actions, a_attacks, b_attacks,
                       a_crits, b_crits, a_dodges, b_dodges)


def random_base(rng: random.Random, min_stat: int, max_stat: int, mot: int) -> BattleStats:
    return BattleStats(*(rng.randint(min_stat, max_stat) for _ in ATTRS), mot=mot)


def add_attr(stats: BattleStats, attr_index: int, amount: int = 1) -> BattleStats:
    values = list(stats.values())
    values[attr_index] += amount
    return BattleStats(*values, mot=stats.mot)


def apply_temperament(stats: BattleStats, temperament: str, mot: int) -> BattleStats:
    values = [
        max(1, cpp_round(value * mult))
        for value, mult in zip(stats.values(), TEMPERAMENT_MULTIPLIERS[temperament])
    ]
    return BattleStats(*values, mot=mot)


def print_aggregate_line(label: str, aggregate: Aggregate) -> None:
    print(
        f"  {label:10s} win={aggregate.win_rate * 100:6.2f}% "
        f"actions={aggregate.avg_actions:5.2f} "
        f"own_atk={aggregate.avg_attacks:5.2f} "
        f"crit/atk={aggregate.crit_rate * 100:4.1f}% "
        f"dodge/atk={aggregate.dodge_rate * 100:4.1f}%"
    )


def simulate_marginal(args: argparse.Namespace, mot: int, rng: random.Random) -> None:
    print(f"\nMarginal +1 vs same base, MOT={mot}, trials={args.trials}")
    for index, attr in enumerate(ATTRS):
        agg = Aggregate()
        for _ in range(args.trials):
            base = random_base(rng, args.min_stat, args.max_stat, mot)
            challenger = add_attr(base, index)

            result = fight_atb(challenger, base, rng)
            agg.add(result.a_won, result.actions, result.a_attacks,
                    result.a_crits, result.a_dodges)

            result = fight_atb(base, challenger, rng)
            agg.add(not result.a_won, result.actions, result.b_attacks,
                    result.b_crits, result.b_dodges)
        print_aggregate_line(f"+1 {attr}", agg)


def simulate_temperaments(args: argparse.Namespace, mot: int, rng: random.Random) -> None:
    totals = {name: Aggregate() for name in TEMPERAMENTS}
    pair_wins: dict[tuple[str, str], list[int]] = {}

    for _ in range(args.trials):
        base = random_base(rng, args.min_stat, args.max_stat, mot)
        variants = {
            name: apply_temperament(base, name, mot)
            for name in TEMPERAMENTS
        }

        for left_name, right_name in itertools.combinations(TEMPERAMENTS, 2):
            pair_key = (left_name, right_name)
            pair_wins.setdefault(pair_key, [0, 0])
            for a_name, b_name in ((left_name, right_name), (right_name, left_name)):
                result = fight_atb(variants[a_name], variants[b_name], rng)
                a_won = result.a_won
                totals[a_name].add(a_won, result.actions, result.a_attacks,
                                   result.a_crits, result.a_dodges)
                totals[b_name].add(not a_won, result.actions, result.b_attacks,
                                   result.b_crits, result.b_dodges)

                left_won = a_won if a_name == left_name else not a_won
                pair_wins[pair_key][0] += int(left_won)
                pair_wins[pair_key][1] += 1

    print(f"\nTemperament round robin, MOT={mot}, trials={args.trials}")
    for name, aggregate in sorted(totals.items(),
                                  key=lambda item: item[1].win_rate,
                                  reverse=True):
        print_aggregate_line(name, aggregate)

    print("\nVs BALANCED")
    for name in TEMPERAMENTS:
        if name == "BALANCED":
            continue
        key = (name, "BALANCED") if TEMPERAMENTS.index(name) < TEMPERAMENTS.index("BALANCED") else ("BALANCED", name)
        wins_for_first, fights = pair_wins[key]
        win_rate = wins_for_first / fights if key[0] == name else 1.0 - wins_for_first / fights
        print(f"  {name:10s} vs BALANCED win={win_rate * 100:6.2f}%")

    if args.matrix:
        print("\nPairwise matrix: row win% vs column")
        print(" " * 11 + " ".join(f"{name[:3]:>7s}" for name in TEMPERAMENTS))
        for row in TEMPERAMENTS:
            cells = []
            for col in TEMPERAMENTS:
                if row == col:
                    cells.append("     --")
                    continue
                key = (row, col) if TEMPERAMENTS.index(row) < TEMPERAMENTS.index(col) else (col, row)
                wins_for_first, fights = pair_wins[key]
                rate = wins_for_first / fights if key[0] == row else 1.0 - wins_for_first / fights
                cells.append(f"{rate * 100:7.1f}")
            print(f"{row[:10]:>10s} " + " ".join(cells))


def simulate_fixed_base(args: argparse.Namespace, mot: int, rng: random.Random) -> None:
    base_value = args.fixed_base
    base = BattleStats(base_value, base_value, base_value, base_value, base_value, mot)
    variants = {
        name: apply_temperament(base, name, mot)
        for name in TEMPERAMENTS
    }

    print(f"\nFixed base {base_value}/{base_value}/{base_value}/{base_value}/{base_value}, MOT={mot}")
    for name in TEMPERAMENTS:
        print(f"  {name:10s} {variants[name]}")

    print("\nFixed base vs BALANCED")
    for name in TEMPERAMENTS:
        if name == "BALANCED":
            continue
        agg = Aggregate()
        for _ in range(args.trials):
            result = fight_atb(variants[name], variants["BALANCED"], rng)
            agg.add(result.a_won, result.actions, result.a_attacks,
                    result.a_crits, result.a_dodges)
            result = fight_atb(variants["BALANCED"], variants[name], rng)
            agg.add(not result.a_won, result.actions, result.b_attacks,
                    result.b_crits, result.b_dodges)
        print_aggregate_line(name, agg)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--trials", type=int, default=5000,
                        help="Trials per simulation block.")
    parser.add_argument("--seed", type=int, default=42,
                        help="Random seed.")
    parser.add_argument("--min-stat", type=int, default=6,
                        help="Random base min stat.")
    parser.add_argument("--max-stat", type=int, default=16,
                        help="Random base max stat.")
    parser.add_argument("--mot", type=int, nargs="+", default=[100],
                        help="Initial MOT values to simulate.")
    parser.add_argument("--fixed-base", type=int, default=12,
                        help="Stat value for the fixed-base temperament check.")
    parser.add_argument("--matrix", action="store_true",
                        help="Print the pairwise temperament matrix.")
    parser.add_argument("--skip-marginal", action="store_true",
                        help="Skip the +1 stat marginal check.")
    parser.add_argument("--skip-temperament", action="store_true",
                        help="Skip temperament round robin.")
    parser.add_argument("--skip-fixed", action="store_true",
                        help="Skip fixed-base temperament check.")
    args = parser.parse_args()

    if args.min_stat > args.max_stat:
        parser.error("--min-stat must be <= --max-stat")

    for mot in args.mot:
        rng = random.Random(args.seed + mot * 1009)
        print(
            f"\n=== PokeBug ATB balance simulation "
            f"seed={args.seed + mot * 1009} stat_range={args.min_stat}-{args.max_stat} ==="
        )
        if not args.skip_marginal:
            simulate_marginal(args, mot, rng)
        if not args.skip_temperament:
            simulate_temperaments(args, mot, rng)
        if not args.skip_fixed:
            simulate_fixed_base(args, mot, rng)


if __name__ == "__main__":
    main()
