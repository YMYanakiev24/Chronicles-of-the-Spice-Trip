// ============================================================================
//  Champions.cpp — the roster: art, walking figures, perks and specials.
// ============================================================================
#include "Champions.h"

const ChampionDef CHAMPIONS[CHAMPION_COUNT] = {
    { .name = "Spellwoven", .art = nullptr, .walk = "walk-hero.png",
      .crop = { 0, 0, 1, 1 }, .cloak = { 70, 96, 120, 255 },
      .perk = "Powers cost 25% less mana.",
      .special = "Arcane Surge", .specialDesc = "Refill 40 mana.",
      .kind = Special::ArcaneSurge, .cooldown = 12, .manaCostMul = 0.75f,
      .quote = "The Veil talks. I talk back.", .winLine = "Another page for the spellbook." },

    { .name = "Atanas", .art = "atanas.png", .walk = "walk-atanas.png",
      .crop = { 0.20f, 0.000f, 0.60f, 0.277f }, .cloak = { 120, 150, 200, 255 },
      .perk = "+20 max health.",
      .special = "Aura of Calmness", .specialDesc = "Heal 25. Enemies and their shots move at half speed for 3 s.",
      .kind = Special::Aura, .cooldown = 10, .hpBonus = 20,
      .quote = "Breathe in. Now hit it.", .winLine = "Calm wins again." },

    { .name = "Gakev", .art = "gakev.png", .walk = "walk-gakev.png",
      .crop = { 0.18f, 0.060f, 0.58f, 0.435f }, .cloak = { 200, 160, 60, 255 },
      .perk = "Every kill drops three extra coins.",
      .special = "Coin Dash", .specialDesc = "Dash a short way, untouchable.",
      .kind = Special::CoinDash, .cooldown = 4, .extraCoins = 3,
      .quote = "Every coin has a story. Mine end in my pocket.", .winLine = "Coins out, boss down." },

    { .name = "Nikolay", .art = "nikolay.png", .walk = "walk-nikolay.png",
      .crop = { 0.14f, 0.100f, 0.76f, 0.760f }, .cloak = { 140, 96, 60, 255 },
      .perk = "+25 max health.",
      .special = "Mug-Shield", .specialDesc = "For 2 s, enemy shots shatter against you.",
      .kind = Special::MugShield, .cooldown = 6, .hpBonus = 25,
      .quote = "Hold my mug.", .winLine = "Not a drop spilled." },

    { .name = "Pupesh", .art = "pupesh.png", .walk = "walk-pupesh.png",
      .crop = { 0.10f, 0.085f, 0.80f, 0.369f }, .cloak = { 90, 160, 70, 255 },
      .perk = "+4 blade damage.",
      .special = "Melon Roll", .specialDesc = "Roll forward, untouchable, hitting each enemy for 20.",
      .kind = Special::MelonRoll, .cooldown = 5, .bladeBonus = 4,
      .quote = "Melons are a lifestyle.", .winLine = "Rolled right over you." },

    { .name = "Ristyo", .art = "ristyo.png", .walk = "walk-ristyo.png",
      .crop = { 0.17f, 0.220f, 0.58f, 0.435f }, .cloak = { 150, 70, 70, 255 },
      .perk = "Casts 30% faster.",
      .special = "Drink", .specialDesc = "Heal 20, but your aim wobbles for 4 s.",
      .kind = Special::Drink, .cooldown = 6, .castCdMul = 0.7f,
      .quote = "One for the road. Then the boss.", .winLine = "Cheers to that!" },

    { .name = "Iliqn Puh", .art = "iliqnpuh.png", .walk = "walk-iliqnpuh.png",
      .crop = { 0.08f, 0.250f, 0.52f, 0.240f }, .cloak = { 170, 120, 50, 255 },
      .perk = "Moves 10% faster.",
      .special = "Bear Jump", .specialDesc = "Leap, untouchable, and slam down for 18 around you.",
      .kind = Special::BearJump, .cooldown = 3, .speedMul = 1.1f,
      .quote = "Bears don't knock.", .winLine = "Hibernation can wait." },

    // Daniil and his slingshot: walk sheet and poses cut from his sheet (see README).
    { .name = "Daniil", .art = "daniil.png", .walk = "walk-daniil.png",
      .crop = { 0.028f, 0.033f, 0.320f, 0.177f }, .cloak = { 30, 30, 40, 255 },
      .perk = "Street fighter: +5 blade damage.",
      .special = "Coin Slingshot", .specialDesc = "Your hits charge it. When full, sling 8 coins at the aim, 12 damage each.",
      .kind = Special::Slingshot, .cooldown = 0.5f, .bladeBonus = 5,
      .quote = "Every coin finds its mark.", .winLine = "Paid in full." },

    { .name = "The Storyteller", .art = "thestoryteller.png", .walk = "walk-storyteller.png",
      .crop = { 0.14f, 0.150f, 0.66f, 0.495f }, .cloak = { 60, 120, 130, 255 },
      .perk = "Heals 2 health a second while standing still.",
      .special = "Release the Koi", .specialDesc = "Three koi swim out and hunt the nearest enemies.",
      .kind = Special::Koi, .cooldown = 7, .stillRegen = 2,
      .quote = "Sit down, this one has a twist.", .winLine = "And that's how the story ends." },

    { .name = "The Fairy", .art = "thefairy.png", .walk = "walk-fairy.png",
      .crop = { 0.38f, 0.320f, 0.56f, 0.258f }, .cloak = { 200, 120, 200, 255 },
      .perk = "Spells curve toward enemies.",
      .special = "Blink", .specialDesc = "Teleport to the cursor for 10 fairy dust.",
      .kind = Special::Blink, .cooldown = 0.3f, .spellHoming = 2.5f,
      .quote = "Blink and you'll miss me.", .winLine = "Sparkles everywhere!" },

    // Momiche walks with the frames cut from her own sprite sheet.
    { .name = "Momiche", .art = "momiche.png", .walk = "walk-momiche.png",
      .crop = { 0.817f, 0.860f, 0.179f, 0.140f }, .cloak = { 150, 100, 70, 255 },
      .perk = "Her axe hits harder: +3 blade damage.",
      .special = "Scare", .specialDesc = "Every enemy runs from you for 3 s and can't attack.",
      .kind = Special::Scare, .cooldown = 8, .fearFace = true, .bladeBonus = 3,
      .quote = "Boo! ...was that too much?", .winLine = "Scared you, didn't I?" },

    // The Gym Rat: creatine in one hand, C++ in the other.
    { .name = "The Gym Rat", .art = "gymrat.png", .walk = "walk-gymrat.png",
      .crop = { 0.35f, 0.080f, 0.29f, 0.340f }, .cloak = { 30, 30, 36, 255 },
      .perk = "Never skips leg day: +15 max health, +2 blade damage.",
      .special = "Creatine & C++", .specialDesc = "Arms out: five C++ lasers ahead hit everything for 22, and a creatine cloud behind shoves enemies away.",
      .kind = Special::CreatineCpp, .cooldown = 6, .hpBonus = 15, .bladeBonus = 2,
      .quote = "Creatine in one hand, C++ in the other.", .winLine = "New PR. Personal record AND pull request." },

    // Vanessa: her walk sheet is cut from her two character sheets (see README).
    { .name = "Vanessa", .art = "vanessa.png", .walk = "walk-vanessa.png",
      .crop = { 0.300f, 0.090f, 0.365f, 0.192f }, .cloak = { 34, 26, 40, 255 },
      .perk = "Heartbreaker: every kill heals her 3.",
      .special = "Blown Kiss", .specialDesc = "Five hearts fly out and hit for 12. A monster one lands on is lovestruck for 3.5 s: it stands and can't attack.",
      .kind = Special::BlownKiss, .cooldown = 7, .killHeal = 3,
      .quote = "Careful. I break hearts.", .winLine = "Another heart broken." },

    // Galin: the weapon-shop owner, out from behind his counter. His card art is his sheet.
    { .name = "Galin", .art = "Galin.png", .walk = "walk-galin.png",
      .crop = { 0.340f, 0.000f, 0.300f, 0.300f }, .cloak = { 28, 28, 32, 255 },
      .perk = "Smoke breaks: standing still heals him 1.5 a second.",
      .special = "Smoke Break", .specialDesc = "Smoke a cig and heal 30, then stub it out on the nearest monster: 24 damage and it burns.",
      .kind = Special::SmokeBreak, .cooldown = 8, .stillRegen = 1.5f,
      .quote = "Give me a minute. I'm on my break.", .winLine = "Now that's worth a smoke." },

    // Filqcheto and his grinders: walk sheet, poses and grinders cut from his sheet (see README).
    { .name = "Filqcheto", .art = "filqcheto.png", .walk = "walk-filqcheto.png",
      .crop = { 0.003f, 0.105f, 0.199f, 0.110f }, .cloak = { 28, 28, 34, 255 },
      .perk = "Chilled out: takes 12% less damage.",
      .special = "Grinders", .specialDesc = "Throws the next grinder: a fan of three, then a Meta-Grinder that bursts, then a Grinder Storm that drags monsters in.",
      .kind = Special::GrinderThrow, .cooldown = 4, .damageTakenMul = 0.88f,
      .quote = "Fresh grind, coming through.", .winLine = "Ground to dust." },

    // Krisa: walk sheet and her punches cut from her sheet (see README).
    { .name = "Krisa", .art = "krisa.png", .walk = "walk-krisa.png",
      .crop = { 0.003f, 0.105f, 0.199f, 0.110f }, .cloak = { 40, 44, 70, 255 },
      .perk = "Quick hands: swings her weapon 25% faster.",
      .special = "Haymaker", .specialDesc = "Dash the way you aim and punch: 28 damage, and monsters fly back dazed for 1.5 s.",
      .kind = Special::Haymaker, .cooldown = 5, .swingMul = 0.75f,
      .quote = "Hold my cap.", .winLine = "Knocked out cold." },
};
