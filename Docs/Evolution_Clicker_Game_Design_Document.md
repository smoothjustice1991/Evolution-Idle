**EVOLUTION CLICKER**

Game Design Document

*From a primordial cell to a conscious sphere of pure energy*

Version 1.0 \| Design baseline \| October 2026

# Document Overview

This document consolidates the concept, interface, progression,
spend-to-upgrade economy, automation, balancing rules, technical
structure, and development roadmap discussed for Evolution Clicker.
Numbers are starting design targets and must be validated through
playtesting.

  -----------------------------------------------------------------------
  **Field**                           **Definition**
  ----------------------------------- -----------------------------------
  Genre                               Idle / incremental auto-clicker

  Core fantasy                        Guide life from its earliest
                                      cellular form through humanity and
                                      beyond physical existence into pure
                                      energy.

  Primary action                      Click a prominent DNA double helix
                                      to generate Evolution Points (EP).

  Primary strategic choice            Spend EP on production upgrades
                                      now, or save EP to purchase the
                                      next evolution stage.

  Target feel                         Simple to understand, satisfying
                                      moment-to-moment feedback,
                                      increasingly strategic long-term
                                      investment.

  Recommended initial engine          Godot 4 for a desktop game;
                                      HTML/CSS/JavaScript is a viable
                                      browser alternative.
  -----------------------------------------------------------------------

# 1. High Concept and Design Pillars

The player begins as a primordial cell. Manual clicking generates EP,
which can be spent on upgrades or on the next evolution. Upgrades
increase manual click power and/or passive EP production, allowing the
player to build an increasingly productive system. The journey begins
with biological evolution, progresses through human development, and
ends in speculative post-human and cosmic forms.

## Design pillars

-   Readable core loop: click, earn EP, invest or save, unlock a new
    form.

-   Meaningful trade-offs: spending EP reduces the available balance and
    can delay the next evolution.

-   Automation as the long-term strategy: upgrades should eventually
    outperform manual clicking.

-   Visible transformation: the central subject and interface evolve
    alongside the player.

-   Expandable data-driven design: stages and upgrades should be added
    without rewriting core systems.

# 2. Core Gameplay Loop

1.  The player clicks the central DNA strand and earns EP.

2.  Owned auto-clickers generate EP passively over time.

3.  The player chooses to save EP toward the next evolution or spend it
    on an upgrade.

4.  Purchasing an upgrade immediately deducts its cost and activates its
    production or multiplier effect.

5.  When the player has enough current EP, they can purchase the next
    evolution stage.

6.  The new stage changes the visual presentation and may unlock new
    upgrade tiers or rewards.

7.  The player repeats the loop, with prices, requirements, and
    production scaling upward over time.

# 3. Currency and Evolution Economy

## 3.1 Currency definitions

  -----------------------------------------------------------------------
  **Resource**            **Behavior**            **Purpose**
  ----------------------- ----------------------- -----------------------
  Current EP              Spendable balance;      Purchases upgrades and
                          increases through       evolution stages.
                          clicks and automation   
                          and decreases when      
                          spent.                  

  Lifetime EP             Cumulative record of    Statistics,
                          all EP generated; never achievements, and
                          decreases when EP is    optional challenges.
                          spent.                  

  EP/sec                  Derived rate from owned Communicates passive
                          production upgrades and production and helps
                          multipliers.            compare upgrades.
  -----------------------------------------------------------------------

## 3.2 Evolution purchase rule

Recommended rule: evolution requires the player to have the required
amount of CURRENT EP and deducts that amount when purchased. This is
consistent with the spend-to-upgrade economy: all major purchases
compete for the same resource. If a stage requirement is 1,500 EP and
the player has 1,000 EP, the player must earn 500 more EP to purchase
it. If the player instead spends 500 EP on an upgrade, the balance
becomes 500 EP and the evolution is further away.

This replaces the earlier idea of using lifetime EP as the evolution
threshold. Lifetime EP remains useful for records, but should not itself
unlock a stage unless a later design revision intentionally changes the
economy.

## 3.3 Purchase feedback

-   Always show the current EP balance, upgrade cost, and expected
    benefit before purchase.

-   Disable purchases the player cannot afford and clearly show the
    shortfall.

-   On purchase, deduct the full cost immediately and update EP/sec or
    click power immediately.

-   Do not allow a purchase to make the balance negative.

-   Use a brief purchase animation or confirmation, but avoid modal
    dialogs that interrupt frequent buying.

# 4. Evolution Campaign

The campaign contains 20 stages across five eras. It is a creative game
progression, not a claim that evolution follows a simple linear ladder.
The first three eras draw from biological history; the final two
transition into speculative science fiction.

  ---------------------------------------------------------------------------
  **Stage**         **Name**          **Illustrative EP **Visual / narrative
                                      threshold**       direction**
  ----------------- ----------------- ----------------- ---------------------
  1                 Primordial Cell   0 EP              Single microscopic
                                                        cell; introductory
                                                        stage.

  2                 Replicating Cell  250 EP            Cell division and
                                                        replication.

  3                 Multicellular     1,000 EP          Multiple cooperating
                    Organism                            cells.

  4                 Early Aquatic     3,000 EP          More complex life in
                    Life                                water.

  5                 Primitive Fish    8,000 EP          Early
                                                        vertebrate-inspired
                                                        form.

  6                 Early Amphibian   20,000 EP         Transition toward
                                                        land.

  7                 Primitive Reptile 50,000 EP         Land-adapted form.

  8                 Early Mammal      125,000 EP        Mammalian traits
                                                        emerge.

  9                 Early Primate     300,000 EP        Primate-inspired
                                                        form.

  10                Early Hominin     750,000 EP        Early
                                                        hominin-inspired
                                                        form.

  11                Homo erectus      2,000,000 EP      A major human-line
                                                        milestone.

  12                Homo sapiens      5,000,000 EP      Modern human stage.

  13                Enhanced Human    15,000,000 EP     Speculative
                                                        biological
                                                        enhancement.

  14                Cybernetic Human  45,000,000 EP     Human-machine
                                                        integration.

  15                Digital           150,000,000 EP    Mind represented
                    Consciousness                       digitally.

  16                Disembodied       500,000,000 EP    Intelligence
                    Intelligence                        independent of a
                                                        biological body.

  17                Energy Form       2 billion EP      Conscious
                                                        energy-based form.

  18                Stellar           10 billion EP     Cosmic-scale
                    Consciousness                       awareness.

  19                Cosmic Entity     50 billion EP     Entity operating at
                                                        cosmic scale.

  20                Pure Energy       250 billion EP    Final campaign form:
                                                        a luminous sphere of
                                                        energy.
  ---------------------------------------------------------------------------

Thresholds above were initially proposed as progression targets. Under
the revised spend-to-evolve model, they should be treated as the current
EP price to purchase each stage, not lifetime totals. This makes the
campaign more demanding; tune values against production rates and
playtest data.

## Era groupings

  -----------------------------------------------------------------------
  **Era**                 **Stages**              **Theme and unlock
                                                  direction**
  ----------------------- ----------------------- -----------------------
  1\. The Beginning       1--4                    Microscopic cells,
                                                  primordial water,
                                                  organic motion; teach
                                                  clicks and the first
                                                  inexpensive automation.

  2\. Life Conquers Land  5--8                    Fish, amphibians,
                                                  reptiles, early
                                                  mammals; introduce
                                                  stronger production
                                                  tiers.

  3\. Rise of Humanity    9--12                   Primates through Homo
                                                  sapiens; expand upgrade
                                                  variety and introduce
                                                  more meaningful
                                                  investments.

  4\. Beyond Humanity     13--16                  Enhanced, cybernetic,
                                                  and digital forms;
                                                  shift visuals toward
                                                  neural circuitry and
                                                  technology.

  5\. Transcendence       17--20                  Energy, stellar
                                                  consciousness, cosmic
                                                  entity, pure energy;
                                                  cosmic effects and
                                                  endgame automation.
  -----------------------------------------------------------------------

# 5. Main GUI and UX

The interface is a simple, dark, futuristic layout. The DNA double helix
is the primary clickable element. The right side of the game window
contains a persistent upgrade panel. The interface should prioritize
readability over visual clutter.

## 5.1 Layout

  -----------------------------------------------------------------------
  **Region**                          **Contents**
  ----------------------------------- -----------------------------------
  Top status bar                      Game title, current stage, current
                                      EP, lifetime EP, settings, and
                                      progression menu.

  Center / left evolution area        Current life-form illustration or
                                      model, stage name, stage
                                      description, evolution
                                      progress/price, and prominent
                                      clickable DNA strand.

  Right upgrade panel                 Upgrade categories, cards, current
                                      level/ownership, price, benefit,
                                      purchase controls, and
                                      affordability states.

  Production summary                  EP per manual click, passive
                                      EP/sec, active multiplier, and
                                      optionally estimated time to afford
                                      the next purchase.
  -----------------------------------------------------------------------

## 5.2 Visual direction

-   Use a dark navy or near-black base with cyan/blue highlights for the
    early game.

-   Use a luminous DNA double helix as the recognizable centerpiece.

-   Add responsive pulses, particles, and floating +EP text when
    clicking.

-   Change lighting, colors, background, and central art across eras.

-   Gradually transform the DNA into a more technological strand, then
    an energy core and finally a sphere of pure energy.

-   Provide reduced-motion, sound, and animation settings.

# 6. Clicking and Production Mechanics

## 6.1 Manual clicking

Starting target: each click grants +1 EP before upgrades and
multipliers.

-   Pulse or briefly scale the DNA strand on click.

-   Display a floating +EP value.

-   Update current EP and lifetime EP.

-   Recalculate the progress toward the next affordable stage or
    purchase.

-   Check achievements and unlock conditions.

## 6.2 Optional click mechanics

-   Critical clicks: small chance to award multiple times the normal
    click yield.

-   Combo streaks: sustained clicking temporarily increases output.

-   Evolution bursts: one-time EP rewards for major milestones.

-   Milestone bonuses: permanent improvements earned for completing an
    era.

These mechanics are optional and should be added after the base economy
feels balanced.

# 7. Upgrade System

Upgrades are the main long-term investment. Every purchase deducts EP
from the current balance. The immediate setback is intentional: the
player sacrifices current purchasing power to increase future
production.

  -----------------------------------------------------------------------
  **Upgrade category**    **Effect**              **Notes**
  ----------------------- ----------------------- -----------------------
  Auto-clickers           Generate EP passively.  Main path to idle
                                                  progression; units may
                                                  be purchased
                                                  repeatedly.

  Auto-clicker            Increase production per Choose between buying
  improvements            owned unit or improve a more units and
                          tier.                   upgrading existing
                                                  production.

  Manual click power      Increases EP per click. Keeps active play
                                                  useful without letting
                                                  it dominate the whole
                                                  game.

  Production multiplier   Multiplies EP from      Powerful and therefore
                          manual and automated    should cost more or
                          sources.                unlock later.

  Critical click chance   Increases chance of     Optional active-play
                          bonus manual clicks.    support.

  Offline evolution       Grants EP while the     Use a clear cap and
                          game is closed.         save timestamp to avoid
                                                  runaway gains.

  Transcendence upgrades  Powerful permanent or   Tie to endgame and
                          late-game bonuses.      optional prestige.
  -----------------------------------------------------------------------

## 7.1 Illustrative starting upgrades

  -----------------------------------------------------------------------
  **Upgrade**             **Initial cost**        **Illustrative
                                                  benefit**
  ----------------------- ----------------------- -----------------------
  Bio-Replicator          50 EP                   +1 EP/sec per unit.

  Advanced Replicator     250 EP                  +5 EP/sec per unit.

  Neural Automator        1,000 EP                +20 EP/sec per unit.

  Genetic Optimization    500 EP                  +1 additional EP per
                                                  manual click per level.

  Evolution Multiplier    2,500 EP                +25% EP production at
                                                  the next level.
  -----------------------------------------------------------------------

These are concept values rather than a fully balanced economy. Before
implementation, define whether each benefit is additive or
multiplicative, whether it applies per unit or per level, and which
upgrades are unlocked in each era.

## 7.2 Upgrade price scaling

Recommended baseline formula:

**Cₙ = floor(C₀ × rⁿ × S)**

Cₙ = next cost; C₀ = base cost; r = repeated-purchase scaling factor; n
= number of previous purchases of that upgrade; S = era cost multiplier.
A starting repeated-purchase factor around 1.20 is suggested.

  -----------------------------------------------------------------------
  **Era**                             **Illustrative cost multiplier**
  ----------------------------------- -----------------------------------
  Primordial life                     ×1

  Complex organisms                   ×5

  Early humanity                      ×25

  Advanced humanity                   ×200

  Cosmic transcendence                ×2,000
  -----------------------------------------------------------------------

The multipliers above are placeholders to guide tuning. New eras must
also introduce upgrades with appropriately larger production benefits.

## 7.3 Payback period

**Payback time = Upgrade cost ÷ Additional EP per second**

For example, an upgrade costing 500 EP and adding 10 EP/sec has a
nominal 50-second payback period. Use this metric to compare purchases
and tune them so that investment is rewarding without making every
decision automatic.

# 8. Economy Rules and Balance Principles

-   Current EP is the only currency spent on standard upgrades and
    evolution purchases.

-   Lifetime EP is cumulative and never decreases.

-   Upgrade purchases deduct their full price immediately.

-   Evolution purchases deduct their full stage price immediately.

-   Auto-clickers produce EP continuously while the game is active.

-   The player cannot spend more EP than they currently have.

-   Show the price and benefit before purchase; disable unaffordable
    purchase buttons.

-   Automation should be stronger over long periods than manual
    clicking, while manual clicks remain useful.

-   Saving for an evolution must remain a valid strategy; investing in
    automation should usually pay off over a longer horizon.

-   Tune stage costs and upgrade prices together, not independently,
    because they compete for the same EP balance.

-   Avoid dead-end purchases that leave the player unable to make
    progress for an excessive time.

-   Use playtests to measure time-to-first-upgrade,
    time-to-first-evolution, purchase frequency, and idle/active
    progression.

# 9. Stage-Up Experience and Rewards

8.  Pause or soften gameplay briefly when a new stage is purchased.

9.  Play a glow/pulse transformation on the DNA and central life-form
    art.

10. Reveal the new stage name and a short description.

11. Apply any stage reward and unlock the next upgrade tier.

12. Resume play quickly; provide an option to skip or reduce the
    animation.

Possible era rewards include a permanent production bonus, new
auto-clicker tiers, improved offline production, or access to
specialized upgrades. If era bonuses are permanent, make their effect
visible in the production breakdown.

# 10. Endgame and Optional Prestige

## 10.1 Campaign ending

Stage 20, Pure Energy, is the campaign\'s final transformation. The DNA
strand is replaced or transformed into a glowing sphere of pure energy
in a cosmic environment. The ending should feel like a culmination of
the entire visual journey.

## 10.2 Cosmic Rebirth prestige (optional post-MVP)

After completing the campaign, the player may optionally reset ordinary
stage and upgrade progression to earn Cosmic Essence. Cosmic Essence
buys permanent bonuses, such as stronger starting click power, better
auto-clicker output, reduced upgrade costs, alternate visual themes, or
additional endgame challenges. Prestige should be optional and should
not be required to finish the main campaign.

# 11. Save Data and Offline Progression

## Data to persist

-   Current EP and lifetime EP.

-   Current stage and unlocked stage/upgrade definitions.

-   Owned auto-clicker quantities and upgrade levels.

-   Prestige currency and permanent bonuses, if implemented.

-   Achievement progress and player settings.

-   Last save timestamp and last offline-earnings timestamp.

## Offline earnings

Initial target: cap offline earnings at 8 hours and grant 50% of the
player\'s normal passive production during that period. Both values are
tunable. Calculate offline gains from the saved timestamp, apply the
cap, and grant the result once so the same interval cannot be claimed
repeatedly.

# 12. Technical Architecture

Recommended initial implementation: Godot 4 with GDScript for a desktop
game. A browser implementation using HTML, CSS, and JavaScript is also
appropriate if easy web access is the priority.

## 12.1 Suggested systems

  -----------------------------------------------------------------------
  **System**                          **Responsibility**
  ----------------------------------- -----------------------------------
  GameState / EconomyManager          Current EP, lifetime EP, costs,
                                      purchases, production totals, and
                                      validation.

  EvolutionManager                    Current stage, stage purchase
                                      rules, unlocks, rewards, and stage
                                      transitions.

  UpgradeManager                      Upgrade definitions, prices,
                                      levels, effects, and unlock
                                      requirements.

  ProductionManager                   Passive production tick,
                                      multipliers, click yield, and
                                      offline earnings calculation.

  SaveManager                         Serialization, automatic saving,
                                      load validation, and timestamps.

  UI controllers                      Render state, show purchase costs,
                                      update bars/counters, and send user
                                      actions to game systems.

  Animation/audio layer               Click feedback, purchase effects,
                                      stage transformations, sound, and
                                      settings.
  -----------------------------------------------------------------------

## 12.2 Data-driven definitions

Store stages and upgrades as data records or resources rather than
hardcoding each one in interface logic. Stage records should include ID,
name, description, current EP cost, art references, era, and unlock
rewards. Upgrade records should include ID, name, description, base
cost, cost scaling, effect type, effect amount, current level/owned
count, and unlock condition.

## 12.3 Main gameplay flow

13. Manual click or production tick adds EP to current and lifetime
    totals.

14. The economy system updates production and validates numeric values.

15. UI counters and affordability states refresh.

16. Purchase requests validate affordability, deduct EP, and apply the
    effect.

17. Evolution purchase validates current EP, deducts the cost, changes
    stage, and grants unlocks.

18. SaveManager records changes and periodically autosaves.

# 13. MVP Scope

The first playable version should validate the spend-to-upgrade loop
before building the full campaign.

-   Clickable DNA strand with click feedback.

-   Current EP, lifetime EP, EP/click, and EP/sec displays.

-   Progress/evolution purchase indicator.

-   Five initial evolution stages using placeholder art.

-   At least three upgrade types, including passive production.

-   Scaling upgrade prices and immediate EP deduction.

-   Evolution purchase that deducts current EP.

-   Automatic local saving and basic offline earnings.

-   Basic purchase validation and unaffordable button states.

# 14. Development Roadmap

  -----------------------------------------------------------------------
  **Phase**               **Scope**               **Exit criteria**
  ----------------------- ----------------------- -----------------------
  1\. Playable prototype  DNA clicking, currency, Core loop works and
                          five placeholder        stage purchase rules
                          stages, simple save.    are clear.

  2\. Economy and         Auto-clickers,          Investment decisions
  automation              upgrades, scaling       feel meaningful; no
                          costs, multipliers,     negative balances or
                          offline earnings.       duplicate rewards.

  3\. Full evolution      All 20 stages, stage    Every stage has a
  campaign                art, era unlocks,       defined cost, visual,
                          stage-up animations.    and progression role.

  4\. Polish and endgame  Achievements,           Save/load is reliable
                          accessibility settings, and progression remains
                          sound, prestige,        balanced across active
                          performance.            and idle play.
  -----------------------------------------------------------------------

# 15. Testing and Acceptance Criteria

-   A click adds the correct amount of EP to both current and lifetime
    totals.

-   Purchasing an upgrade deducts exactly its displayed cost and applies
    its effect once.

-   Purchasing an evolution deducts exactly its displayed cost and
    unlocks the correct next stage.

-   Lifetime EP never decreases when currency is spent.

-   Unaffordable purchases are blocked and cannot create negative EP.

-   Auto-clicker production matches the displayed EP/sec over a measured
    interval.

-   Upgrade prices increase according to the defined scaling formula.

-   Save/load preserves balances, stages, upgrades, and timestamps.

-   Offline earnings respect the cap and cannot be claimed twice for the
    same interval.

-   The interface stays readable at supported window sizes and
    reduced-motion settings work.

-   Playtesting checks both active clicking and idle-only progression
    for excessive stalls.

# 16. Open Decisions Before Production

-   Should evolution prices be exactly the illustrative values in this
    document or be recalibrated after the economy prototype?

-   Should each era grant a permanent production bonus, or should all
    progression power come from purchasable upgrades?

-   Should auto-clicker upgrades be per-unit, global to a tier, or a mix
    of both?

-   Should the main game ship as a desktop app in Godot or as a browser
    game?

-   Should Cosmic Rebirth be part of the first full release or a
    post-launch feature?

-   What target time should the player take to reach the first
    auto-clicker, Homo sapiens, and Pure Energy?

# Appendix A: Key Formulas

  -----------------------------------------------------------------------
  **Mechanic**                        **Formula / rule**
  ----------------------------------- -----------------------------------
  Upgrade cost                        Cₙ = floor(C₀ × rⁿ × S)

  Passive EP/sec                      Sum of each owned unit quantity ×
                                      its production, then apply
                                      applicable multipliers.

  Manual EP/click                     Base click power × click upgrades ×
                                      applicable multipliers.

  Payback time                        Upgrade cost ÷ additional EP/sec.

  Current EP                          Earned EP minus all upgrade and
                                      evolution costs spent.

  Lifetime EP                         Sum of all EP generated; spending
                                      does not reduce it.

  Offline earnings                    Eligible passive production ×
                                      offline duration × offline
                                      efficiency, capped by the maximum
                                      duration.
  -----------------------------------------------------------------------

Implementation note: decide and document the order of additive and
multiplicative bonuses before building a large upgrade catalog. Use
consistent rounding and large-number formatting throughout the game.
