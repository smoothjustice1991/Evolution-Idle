# Steam store assets

Upload these on the Steamworks partner site: **App Admin → Store Page Admin → Graphical Assets**.
They are not packaged into the game.

| File | Steam slot | Size |
|---|---|---|
| `header_capsule_920x430.png` | Header Capsule (also usable as the Library Header Capsule) | 920 × 430 |
| `small_capsule_462x174.png` | Small Capsule | 462 × 174 |
| `main_capsule_1232x706.png` | Main Capsule | 1232 × 706 |
| `vertical_capsule_748x896.png` | Vertical Capsule | 748 × 896 |
| `library_capsule_600x900.png` | Library Capsule | 600 × 900 |
| `header_capsule_920x430.png` | Library Header Capsule (same file as the store header) | 920 × 430 |
| `library_hero_3840x1240.png` | Library Hero (art only) | 3840 × 1240 PNG |
| `library_logo_1189x720.png` | Library Logo (transparent, shown over the Hero) | 1189 × 720 PNG |

The capsules and hero are opaque PNGs (the original store capsules had a few semi-transparent edge
pixels, flattened onto the art's deep navy). The library logo keeps its transparency; the supplied
1280 x 733 file was trimmed of empty margin and scaled to 1189 x 720, because Steam's limit is
1280 wide and/or 720 tall and 733 would exceed the height.

## Still needed before release

| Asset | Size | Notes |
|---|---|---|
| Screenshots | at least 1920 × 1080, 16:9 | At least 5 for the store page |
| Shortcut icon | 256 × 256 .png or .ico | |
| App icon | 184 × 184 .jpg | |
| Page background | 1438 × 810 | Optional, subtle and dark |

Sizes from the Steamworks documentation: https://partner.steamgames.com/doc/store/assets
