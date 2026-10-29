// ============================================================================
//  Champions.cpp — the roster: art, perks and specials.
// ============================================================================
#include "Champions.h"

const ChampionDef CHAMPIONS[CHAMPION_COUNT] = {
    { .name = "Spellwoven", .art = nullptr, .crop = { 0, 0, 1, 1 }, .cloak = { 70, 96, 120, 255 },
      .perk = "Powers cost 25% less mana.",
      .special = "Arcane Surge", .specialDesc = "Refill 40 mana.",
      .kind = Special::ArcaneSurge, .cooldown = 12, .manaCostMul = 0.75f },

    { .name = "Atanas", .art = "atanas.png", .crop = { 0.20f, 0.000f, 0.60f, 0.277f }, .cloak = { 120, 150, 200, 255 },
      .perk = "+20 max health.",
      .special = "Aura of Calmness", .specialDesc = "Heal 25. Enemies and their shots move at half speed for 3 s.",
      .kind = Special::Aura, .cooldown = 10, .hpBonus = 20 },

    { .name = "Gakev", .art = "gakev.png", .crop = { 0.18f, 0.060f, 0.58f, 0.435f }, .cloak = { 200, 160, 60, 255 },
      .perk = "Every kill drops an extra coin.",
      .special = "Coin Dash", .specialDesc = "Dash a short way, untouchable.",
      .kind = Special::CoinDash, .cooldown = 4, .extraCoins = 1 },

    { .name = "Nikolay", .art = "nikolay.png", .crop = { 0.14f, 0.100f, 0.76f, 0.760f }, .cloak = { 140, 96, 60, 255 },
      .perk = "+25 max health.",
      .special = "Mug-Shield", .specialDesc = "For 2 s, enemy shots shatter against you.",
      .kind = Special::MugShield, .cooldown = 6, .hpBonus = 25 },

    { .name = "Pupesh", .art = "pupesh.png", .crop = { 0.10f, 0.085f, 0.80f, 0.369f }, .cloak = { 90, 160, 70, 255 },
      .perk = "+4 blade damage.",
      .special = "Melon Roll", .specialDesc = "Roll forward, untouchable, hitting each enemy for 20.",
      .kind = Special::MelonRoll, .cooldown = 5, .bladeBonus = 4 },

    { .name = "Ristyo", .art = "ristyo.png", .crop = { 0.17f, 0.220f, 0.58f, 0.435f }, .cloak = { 150, 70, 70, 255 },
      .perk = "Casts 30% faster.",
      .special = "Drink", .specialDesc = "Heal 20, but your aim wobbles for 4 s.",
      .kind = Special::Drink, .cooldown = 6, .castCdMul = 0.7f },

    { .name = "Iliqn Puh", .art = "iliqnpuh.png", .crop = { 0.08f, 0.250f, 0.52f, 0.240f }, .cloak = { 170, 120, 50, 255 },
      .perk = "Moves 10% faster.",
      .special = "Bear Jump", .specialDesc = "Leap, untouchable, and slam down for 18 around you.",
      .kind = Special::BearJump, .cooldown = 3, .speedMul = 1.1f },

    { .name = "Daniil & Vasil", .art = "daniilandvasil.png", .crop = { 0.24f, 0.000f, 0.546f, 1.000f }, .cloak = { 90, 90, 140, 255 },
      .perk = "Twin strike: +5 blade damage.",
      .special = "Coin Barrage", .specialDesc = "Your hits charge it. When full, fire a ring of 16 coins.",
      .kind = Special::CoinBarrage, .cooldown = 0.5f, .bladeBonus = 5 },

    { .name = "The Storyteller", .art = "thestoryteller.png", .crop = { 0.14f, 0.150f, 0.66f, 0.495f }, .cloak = { 60, 120, 130, 255 },
      .perk = "Heals 2 health a second while standing still.",
      .special = "Release the Koi", .specialDesc = "Three koi swim out and hunt the nearest enemies.",
      .kind = Special::Koi, .cooldown = 7, .stillRegen = 2 },

    { .name = "The Fairy", .art = "thefairy.png", .crop = { 0.38f, 0.320f, 0.56f, 0.258f }, .cloak = { 200, 120, 200, 255 },
      .perk = "Spells curve toward enemies.",
      .special = "Blink", .specialDesc = "Teleport to the cursor for 10 fairy dust.",
      .kind = Special::Blink, .cooldown = 0.3f, .spellHoming = 2.5f },
};
