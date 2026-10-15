# Steam achievements

Evolution Idle has 32 achievements, defined in `Content/Data/GameData.json` under `"achievements"`.
The game tracks them itself (they unlock and are saved with progress even without Steam) and mirrors
each unlock to Steam when Steam is running. The in-game **Achievements** panel (top bar) lists them all
with progress.

## Setting them up in Steamworks

1. In Steamworks, open the app's **Stats & Achievements > Achievements** page.
2. Add one achievement per row below. **API Name** must match exactly (it is case-sensitive).
3. Upload the two icons for each achievement from `Marketing/Steam/Achievements/`:
   `<API name>_achieved.jpg` and `<API name>_unachieved.jpg` (64 x 64). `_preview.png` shows them all;
   256 px masters are in `masters/`. Regenerate them with `Tools/make_achievement_icons.py` after
   changing achievements.
4. **Publish** the changes (Steamworks > Publish). Unpublished achievements are ignored by Steam.
5. In `Config/DefaultEngine.ini`, replace `SteamDevAppId=480` with the game's App ID, and set
   `ReleaseAppId` to the same number before the release build.

| API Name | Display name | Description |
|---|---|---|
| `ACH_STAGE_02` | Copy That | Evolve into a Replicating Cell. |
| `ACH_STAGE_03` | Stronger Together | Evolve into a Multicellular Organism. |
| `ACH_STAGE_04` | Into the Deep | Evolve into Early Aquatic Life. |
| `ACH_STAGE_05` | Backbone | Evolve into a Primitive Fish. |
| `ACH_STAGE_06` | Testing the Waters | Evolve into an Early Amphibian. |
| `ACH_STAGE_07` | Cold-Blooded | Evolve into a Primitive Reptile. |
| `ACH_STAGE_08` | Warm Blood | Evolve into an Early Mammal. |
| `ACH_STAGE_09` | Branching Out | Evolve into an Early Primate. |
| `ACH_STAGE_10` | Standing Tall | Evolve into an Early Hominin. |
| `ACH_STAGE_11` | Fire Starter | Evolve into Homo erectus. |
| `ACH_STAGE_12` | The Wise Ones | Evolve into Homo sapiens. |
| `ACH_STAGE_13` | Upgraded | Evolve into an Enhanced Human. |
| `ACH_STAGE_14` | Machine Heart | Evolve into a Cybernetic Human. |
| `ACH_STAGE_15` | Uploaded | Evolve into a Digital Consciousness. |
| `ACH_STAGE_16` | Beyond the Body | Evolve into a Disembodied Intelligence. |
| `ACH_STAGE_17` | Radiant | Evolve into an Energy Form. |
| `ACH_STAGE_18` | Star Mind | Evolve into a Stellar Consciousness. |
| `ACH_STAGE_19` | Cosmic Scale | Evolve into a Cosmic Entity. |
| `ACH_STAGE_20` | Ascension | Complete your final evolution into Pure Energy. |
| `ACH_ERA_1` | Era Complete: The Beginning | Reach every stage of Era 1: The Beginning. |
| `ACH_ERA_2` | Era Complete: Life Conquers Land | Reach every stage of Era 2: Life Conquers Land. |
| `ACH_ERA_3` | Era Complete: Rise of Humanity | Reach every stage of Era 3: Rise of Humanity. |
| `ACH_ERA_4` | Era Complete: Beyond Humanity | Reach every stage of Era 4: Beyond Humanity. |
| `ACH_ERA_5` | Era Complete: Transcendence | Reach every stage of Era 5: Transcendence. |
| `ACH_CLICKS_100` | Tap Tap | Click the DNA 100 times. |
| `ACH_CLICKS_1000` | Busy Fingers | Click the DNA 1,000 times. |
| `ACH_CLICKS_10000` | Relentless | Click the DNA 10,000 times. |
| `ACH_CLICKS_100000` | Hundred Thousand Sparks | Click the DNA 100,000 times. |
| `ACH_AUTO_10` | Self-Sustaining | Own 10 auto-clickers in total. |
| `ACH_AUTO_100` | Living Factory | Own 100 auto-clickers in total. |
| `ACH_ALL_UPGRADES` | Complete Toolkit | Buy every upgrade at least once. |
| `ACH_OFFLINE_FULL` | Sleeping Giant | Collect the full 8 hours of offline earnings. |

## How it works

- `type` decides the condition: `stage` (reach stage *target*), `eraComplete` (reach the last stage of era
  *target*), `clicks` (counted clicks, after the 20/sec cap), `autoClickers` (auto-clicker units owned in
  total), `allUpgrades` (one level of every upgrade) and `offlineFull` (collect the full offline period,
  `offlineCapHours`).
- Unlocks are stored in the save with the unlock time and survive **Reset progress**.
- When Steam starts, the game sends Steam any unlocks it missed, and picks up unlocks that Steam already
  has (for example from another PC).
- Steam is off in Play-in-Editor (`bEnableInEditor=False` under `[EvolutionIdle.Steam]`); achievements
  still unlock locally there, so the panel can be tested in the editor.
- Development builds use App ID 480 (Valve's Spacewar test app) until it is replaced, so Steam will show
  you as playing Spacewar and will reject these achievement names. That is expected.
- To test unlocks again on Steam, the Steam console command `reset_all_stats <appid>` clears them
  (open `steam://open/console` in a browser while Steam runs).

Code: `Source/EvolutionIdle/Public/EvoAchievements.h` (conditions), `Private/EvoSteam.cpp` (Steam bridge),
`Private/UI/SEvoHUD.cpp` (panel and unlock toast).
