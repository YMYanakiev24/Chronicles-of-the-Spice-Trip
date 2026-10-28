// Exports the web game's content as JSON so the Unity data assets are generated
// from the original JavaScript instead of being retyped by hand.
//
//   node export-source-data.mjs <web-repo> <out.json>
//
// GameData.js is a pure ES module and is imported directly. BOSSES, CHARACTERS
// and SHOP_STOCK are private consts in Scenes.js (which touches the DOM), so
// their array literals are cut out of the source text and evaluated alone.

import fs from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

const [, , repo, out] = process.argv;
if (!repo || !out) { console.error('usage: node export-source-data.mjs <web-repo> <out.json>'); process.exit(2); }

const gd = await import(pathToFileURL(path.join(repo, 'js/data/GameData.js')).href);
const scenesSrc = fs.readFileSync(path.join(repo, 'js/ui/Scenes.js'), 'utf8');
const saveSrc = fs.readFileSync(path.join(repo, 'js/core/SaveManager.js'), 'utf8');

function arrayConst(src, name) {
  const m = src.match(new RegExp(`const ${name} = (\\[[\\s\\S]*?\\n\\]);`));
  if (!m) throw new Error(`could not find const ${name}`);
  return Function(`"use strict"; return (${m[1]});`)();
}

const hubDefault = saveSrc.match(/return raw \? JSON\.parse\(raw\) : \{ gold: (\d+), stash: \{\} \};/);
if (!hubDefault) throw new Error('could not find the hub default gold in SaveManager.js');

// Objects keyed by id become arrays (Unity's JsonUtility has no dictionaries);
// key order is preserved because it is the display order in the original.
const asList = (obj) => Object.entries(obj).map(([key, v]) => ({ key, ...v }));

const data = {
  items: asList(gd.ITEMS),
  bosses: arrayConst(scenesSrc, 'BOSSES'),
  characters: arrayConst(scenesSrc, 'CHARACTERS'),
  shopStock: arrayConst(scenesSrc, 'SHOP_STOCK'),
  tips: gd.TIPS,
  hubStartingGold: Number(hubDefault[1]),
  // The level curve (60 · L^1.5), shared by the Dungeon's levels.
  xpForLevel: Array.from({ length: 30 }, (_, i) => gd.xpForLevel(i + 1)),
};

fs.mkdirSync(path.dirname(out), { recursive: true });
fs.writeFileSync(out, JSON.stringify(data, null, 2) + '\n');
console.log(`wrote ${out}: ${data.items.length} items, ${data.bosses.length} bosses, ${data.characters.length} characters, ${data.shopStock.length} shop entries`);
