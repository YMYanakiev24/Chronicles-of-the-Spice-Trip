# Custom art drop-in

Put your own pixel-art images here and the game will use them automatically
(no code changes needed). Each file is optional — anything missing falls back to
the built-in procedurally-drawn scene, so the game always runs.

| File | Where it appears | Best size / shape |
| --- | --- | --- |
| `menu.png` | Main-menu background (the castle scene). The on-screen title is hidden so your art's own title shows. The clickable "Enter the Citadel" hotspot sits over the upper-right, so keep the castle there. | 16:9, e.g. 1280×720 |
| `corridor.png` | The stone hall you enter from the castle. The two clickable doors are at roughly **left 9–34%** (Marin) and **right 65–91%** (Changing Room) of the width — line your doors up there. | 16:9, e.g. 1280×720 |
| `shop.png` | Marin's apothecary interior (shown after the fog wipe). | 16:9, e.g. 1280×720 |
| `marin.png` | Marin's character portrait, framed at the lower-left of the shop. | tall/portrait, e.g. 480×640 |

## How to add them

1. Save your images into this `assets/` folder with the exact names above.
2. Reload the game (refresh the browser tab). That's it.

> Note: the shop's items stay fantasy apothecary wares (potions, elixirs,
> relics, and the Enchanted Loom). Your room art and Marin portrait are used as
> scenery/character art only.
