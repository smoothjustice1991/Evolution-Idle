# Evolution Idle

An idle / incremental clicker for **Unreal Engine 5.8**: click a glowing DNA double helix to earn
Evolution Points (EP), invest them in automation, and evolve through 20 stages from a primordial
cell to a conscious sphere of pure energy.

The full design lives in [`Docs/Evolution_Clicker_Game_Design_Document.md`](Docs/Evolution_Clicker_Game_Design_Document.md).

The whole game is C++ (gameplay plus a Slate interface built in code), so there are no Blueprint or
UMG assets to set up. Stages, upgrades and economy numbers live in one JSON file you can edit
without recompiling.

## Getting it running

1. Install **Unreal Engine 5.8** from the Epic Games Launcher.
2. Install **Visual Studio 2022** with the **"Game development with C++"** workload
   (or JetBrains Rider with the Unreal toolchain).
3. Clone this repo, right-click `EvolutionIdle.uproject` and choose **Generate Visual Studio project files**.
4. Open `EvolutionIdle.sln`, select **Development Editor / Win64**, and build.
   (Or just double-click the `.uproject` and click **Yes** when asked to rebuild modules.)
5. In the editor, press **Play**. The project starts on the engine's empty `Entry` map and the C++
   game mode puts the interface on screen; no level setup is needed.

To compile without opening the editor (editor closed), double-click `BuildEditor.bat`; the output goes to `Saved/BuildLog.txt`.

To ship a standalone build: **Platforms > Windows > Package Project**. `Content/Data` is packaged
automatically (see `Config/DefaultGame.ini`).

## What's in this build

| GDD item | Status |
|---|---|
| Clickable DNA helix with pulse, particles and floating +EP text | Done |
| Current EP, lifetime EP, EP/click, EP/sec, multiplier breakdown | Done |
| Spend-to-evolve: stage price deducted from **current** EP (GDD 3.2) | Done |
| All 20 stages and 5 eras as data, era-coloured theme | Done |
| Floating low-poly 3D model of the current stage behind the helix (20 models, morphs on evolve) | Done |
| Next-evolution panel right of the helix: holographic scanner of the next form (revealed bottom-up as EP approaches its price), progress bar, EVOLVE button | Done |
| Upgrade indicators on the model: orbiting crystals (auto-clickers), crown shards (click power), gold pedestal halos (multipliers), flash + shockwave + sparks + "+1 name" on purchase | Done |
| Helix visuals change: organic helix (1-12), circuit helix (13-16), energy core (17-19), sphere of pure energy (20) | Done (procedural placeholder art) |
| 24 upgrades across all eras: auto-clickers, click power, multipliers | Done |
| Cost scaling `Cn = floor(C0 * r^n * S)`, shortfall and payback shown on every card | Done |
| Unaffordable purchases disabled; balance can never go negative | Done |
| Autosave every 15 s, save on evolve and on quit | Done |
| Offline earnings: 50% for up to 8 h, granted once | Done |
| Stage-up banner and flash, reduced-motion setting, reset progress | Done |
| Era production bonus (+10% at stages 5, 9, 13, 17) | Done (set to 0 in data to disable) |
| Critical clicks, combos, achievements, sound, Cosmic Rebirth prestige | Not yet (roadmap phase 4 / post-MVP) |

## Project layout

```
Content/Data/GameData.json        Stages, eras, upgrades, economy settings (edit to tune)
Source/EvolutionIdle/
  Public/EvoGameData.h            Data structs loaded from JSON
  Public/EvoEconomy.h             Pure economy rules (no UObjects, unit tested)
  Public/EvolutionGameSubsystem.h Ticking, saving, offline earnings, player actions
  Public/EvolutionSaveGame.h      Persisted state
  Public/EvolutionGameMode.h      Game mode, player controller, HUD actor
  Private/UI/SEvoHUD.*            Slate interface (status bar, evolve panel, upgrade panel, settings)
  Private/UI/SEvoHelix.*          Animated, clickable helix / energy sphere
  Private/UI/EvoStageMeshes.*     Procedural 3D models for the 20 stages (plain C++)
  Private/UI/SEvoStageModel.*     Draws the floating stage model (CPU-shaded Slate triangles)
  Private/Tests/                  Automation tests for the GDD acceptance criteria
Tools/balance_sim.py              Economy simulator (same formulas as the C++)
Tools/stage_preview/              Renders Docs/stage_models.png from the stage meshes without Unreal
```

## Economy rules

Bonus order (the GDD asks for this to be fixed before the upgrade catalog grows):

```
GlobalMultiplier = (1 + sum of multiplier upgrade levels x amount) x (1 + sum of reached stage bonuses)
EP per click     = (baseClickPower + sum of click upgrade levels x amount) x GlobalMultiplier
EP per second    = sum of auto-clicker counts x amount x GlobalMultiplier
Upgrade cost     = floor(baseCost x costScaling^owned x era costMultiplier)
```

Upgrades use the era multiplier of the era they belong to (not the player's current era), so old
upgrades don't get pricier when you evolve.

## Tuning

Edit `Content/Data/GameData.json`, then check pacing with the simulator before playing:

```
python3 Tools/balance_sim.py              # 0.5 and 3 clicks/sec profiles
python3 Tools/balance_sim.py --cps 1 5    # custom click rates
```

With the current numbers, a payback-greedy player reaches Homo sapiens in about 3.5 hours and
Pure Energy in about 12 hours (stage prices were raised in v0.2 because the early stages went by too fast), and active clicking only shaves a few minutes off, so automation is
the main strategy as the GDD intends. Real players will be slower; treat these as starting values.

## Tests

In the editor open **Tools > Session Frontend > Automation**, filter for `EvolutionIdle`, and run.
The tests cover click income, purchase deduction, blocked purchases, scaling formula, evolution
purchases, lifetime EP never decreasing, production over time, the offline cap, and save sanitizing.

## Design decisions taken for the open questions (GDD section 16)

- Evolution prices use the illustrative values as current-EP prices; upgrade output was tuned around them.
- Each new era grants a small permanent production bonus (+10%), shown in the multiplier tooltip.
- Auto-clicker upgrades are per-unit (each unit adds a fixed EP/sec); multipliers are global.
- Desktop build in Unreal Engine 5.8.
- Cosmic Rebirth prestige is left for after the first full release.

## Save location

`Saved/SaveGames/EvolutionIdle_Save.sav` in the project (editor) or the packaged game's `Saved` folder.
Delete it, or use **Settings > Reset progress**, to start over.
