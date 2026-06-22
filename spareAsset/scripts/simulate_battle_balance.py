#!/usr/bin/env python3
"""Monte Carlo balance check for PokeBug battle formulas.

Usage:
  python3 spareAsset/scripts/simulate_battle_balance.py
  python3 spareAsset/scripts/simulate_battle_balance.py --trials 50000 --seed 42

The script mirrors src/game/BattleCalc.h and is intended as a quick regression
check when tuning SIZ/STR/END/SPD/SPI battle constants.
"""

from __future__ import annotations

import argparse
import random
from dataclasses import dataclass


@dataclass(frozen=True)
class BattleStats:
    siz: int
    str: int
    end: int
    spd: int
    spi: int
    mot: int = 50


ATTRS = ("SIZ", "STR", "END", "SPD", "SPI")


def clamp(value: float, lo: float, hi: float) -> float:
    return max(lo, min(hi, value))


def compute_hp(stats: BattleStats) -> int:
    return 20 + stats.siz * 3 + stats.end * 2


def compute_initiative(stats: BattleStats) -> int:
    return stats.spd * 6 + stats.mot + random.randint(-8, 8)


def compute_mot_loss(stats: BattleStats) -> int:
    return int(clamp(round(9.0 - stats.spi / 3.0), 2, 9))


def compute_attack(attacker: BattleStats, defender: BattleStats) -> tuple[int, bool, bool]:
    dodge_rate = 0.0
    spd_diff = defender.spd - attacker.spd
    if spd_diff > 0:
        dodge_rate += spd_diff * 3.0
    dodge_rate += defender.spi / 8.0
    dodge_rate = clamp(dodge_rate, 0.0, 22.0)
    if random.random() * 100.0 < dodge_rate:
        return 0, False, True

    str_power = 1.5 + attacker.str * 0.80
    size_bonus = 1.0 + 0.45 * attacker.siz / (attacker.siz + 8.0)
    mot_mult = 0.75 + attacker.mot / 200.0
    raw = str_power * size_bonus * mot_mult

    crit_rate = 5.0 + attacker.spi * 2.2 + attacker.spd * 0.5 - defender.spi * 1.5
    crit_rate = clamp(crit_rate, 5.0, 55.0)
    crit = random.random() * 100.0 < crit_rate
    crit_mult = 1.35 if crit else 1.0

    mitigation = 1.0 - min(0.14, defender.end * 0.0045)
    reduction = defender.end * 0.18
    damage = round(raw * crit_mult * mitigation - reduction)
    return max(1, damage), crit, False


def with_mot(stats: BattleStats, mot: int) -> BattleStats:
    return BattleStats(stats.siz, stats.str, stats.end, stats.spd, stats.spi, mot)


def fight(a: BattleStats, b: BattleStats, max_rounds: int = 30) -> tuple[bool, int, int, int]:
    a_hp = compute_hp(a)
    b_hp = compute_hp(b)
    a_max = a_hp
    b_max = b_hp
    a_mot = a.mot
    b_mot = b.mot
    dodges = 0
    crits = 0

    rounds = 0
    for rounds in range(1, max_rounds + 1):
        cur_a = with_mot(a, a_mot)
        cur_b = with_mot(b, b_mot)
        a_first = compute_initiative(cur_a) >= compute_initiative(cur_b)

        if a_first:
            damage, crit, dodged = compute_attack(cur_a, cur_b)
            crits += int(crit)
            dodges += int(dodged)
            b_hp -= damage
            if b_hp > 0:
                damage, crit, dodged = compute_attack(cur_b, cur_a)
                crits += int(crit)
                dodges += int(dodged)
                a_hp -= damage
        else:
            damage, crit, dodged = compute_attack(cur_b, cur_a)
            crits += int(crit)
            dodges += int(dodged)
            a_hp -= damage
            if a_hp > 0:
                damage, crit, dodged = compute_attack(cur_a, cur_b)
                crits += int(crit)
                dodges += int(dodged)
                b_hp -= damage

        a_mot = max(0, a_mot - compute_mot_loss(cur_a))
        b_mot = max(0, b_mot - compute_mot_loss(cur_b))
        if a_hp <= 0 or b_hp <= 0:
            break

    if a_hp > 0 and b_hp <= 0:
        return True, rounds, crits, dodges
    if a_hp <= 0 and b_hp > 0:
        return False, rounds, crits, dodges
    return (a_hp / a_max) >= (b_hp / b_max), rounds, crits, dodges


def random_base(min_stat: int, max_stat: int) -> BattleStats:
    return BattleStats(
        random.randint(min_stat, max_stat),
        random.randint(min_stat, max_stat),
        random.randint(min_stat, max_stat),
        random.randint(min_stat, max_stat),
        random.randint(min_stat, max_stat),
    )


def add_attr(stats: BattleStats, attr_index: int, amount: int = 1) -> BattleStats:
    values = [stats.siz, stats.str, stats.end, stats.spd, stats.spi]
    values[attr_index] += amount
    return BattleStats(*values)


def simulate_marginal(trials: int, min_stat: int, max_stat: int) -> None:
    print(f"Random base {min_stat}-{max_stat}, +1 marginal ({trials} trials each)")
    for idx, name in enumerate(ATTRS):
        wins = 0
        total_rounds = 0
        total_crits = 0
        total_dodges = 0
        for _ in range(trials):
            base = random_base(min_stat, max_stat)
            challenger = add_attr(base, idx)
            won, rounds, crits, dodges = fight(challenger, base)
            wins += int(won)
            total_rounds += rounds
            total_crits += crits
            total_dodges += dodges
        print(
            f"{name:3s} win={wins / trials:.3f} "
            f"rounds={total_rounds / trials:.2f} "
            f"crits={total_crits / trials:.2f} dodges={total_dodges / trials:.2f}"
        )


def simulate_archetypes(trials: int) -> None:
    base = BattleStats(10, 10, 10, 10, 10)
    archetypes = (
        ("Tank_END", BattleStats(10, 10, 16, 8, 10)),
        ("Brute_STR", BattleStats(10, 16, 8, 10, 8)),
        ("Giant_SIZ", BattleStats(16, 11, 10, 8, 8)),
        ("Swift_SPD", BattleStats(9, 10, 9, 16, 10)),
        ("Spirit_SPI", BattleStats(9, 9, 10, 10, 16)),
        ("Balanced", BattleStats(12, 12, 12, 12, 12)),
    )

    print(f"\nArchetypes vs {base} ({trials} trials each)")
    for name, stats in archetypes:
        wins = 0
        total_rounds = 0
        for _ in range(trials):
            won, rounds, _, _ = fight(stats, base)
            wins += int(won)
            total_rounds += rounds
        print(f"{name:12s} win={wins / trials:.3f} rounds={total_rounds / trials:.2f} {stats}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--trials", type=int, default=20000, help="Trials per scenario")
    parser.add_argument("--seed", type=int, default=42, help="Random seed")
    parser.add_argument("--min-stat", type=int, default=6, help="Random base min stat")
    parser.add_argument("--max-stat", type=int, default=16, help="Random base max stat")
    args = parser.parse_args()

    random.seed(args.seed)
    simulate_marginal(args.trials, args.min_stat, args.max_stat)
    simulate_archetypes(max(2000, args.trials // 4))


if __name__ == "__main__":
    main()
