// ============================================================================
//  Champions.cpp — the roster: art, walking figures, perks and specials.
// ============================================================================
#include "Champions.h"
#include <cstdio>
#include <cstring>

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

// Beray's outfits: every champion's own three (see Champions.h, and tools/outfits.py for how they look).
const OutfitDef OUTFITS[CHAMPION_COUNT][OUTFIT_COUNT] = {
    {   // hero
        { nullptr, "Own Clothes", "What they walked in with.", 0, { { 120, 110, 140, 255 }, { 70, 64, 84, 255 } } },
        { "starlit", "Starlit Robes", "Midnight-blue robes sewn with gold stars, and a cap to match.", 120, { { 46, 42, 138, 255 }, { 242, 201, 76, 255 } } },
        { "ember", "Ember Magus", "Crimson robes with flames licking up the hem, and a black cape.", 180, { { 158, 28, 20, 255 }, { 255, 160, 32, 255 } } },
        { "archmage", "Archmage of the Veil", "White and gold, a violet cape, and a glow that hums.", 280, { { 234, 232, 242, 255 }, { 154, 104, 224, 255 } } },
    },
    {   // atanas
        { nullptr, "Own Clothes", "What they walked in with.", 0, { { 120, 110, 140, 255 }, { 70, 64, 84, 255 } } },
        { "blackbelt", "Black Belt", "A black gi with gold trim and a red belt. Earned, probably.", 120, { { 30, 30, 38, 255 }, { 196, 32, 42, 255 } } },
        { "saffron", "Saffron Robe", "Monk's robes and prayer beads. Very calm. Suspiciously calm.", 180, { { 224, 120, 24, 255 }, { 122, 46, 8, 255 } } },
        { "zen", "Zen Master", "Pale robes, and an aura you can see from the next room.", 280, { { 207, 232, 240, 255 }, { 122, 232, 240, 255 } } },
    },
    {   // gakev
        { nullptr, "Own Clothes", "What they walked in with.", 0, { { 120, 110, 140, 255 }, { 70, 64, 84, 255 } } },
        { "collector", "Coin Collector", "Green, a gold coin on the chest, a chain and gold soles.", 120, { { 23, 128, 60, 255 }, { 242, 201, 76, 255 } } },
        { "tracksuit", "Red Tracksuit", "Two white stripes down each side. Faster on the dash.", 180, { { 200, 32, 42, 255 }, { 244, 244, 248, 255 } } },
        { "goldplated", "Gold Plated", "Head to toe in gold, with a crown and a shine.", 280, { { 217, 165, 32, 255 }, { 255, 226, 122, 255 } } },
    },
    {   // nikolay
        { nullptr, "Own Clothes", "What they walked in with.", 0, { { 120, 110, 140, 255 }, { 70, 64, 84, 255 } } },
        { "knight", "Knight of the Mug", "Steel plate, a blue tabard, and a mug on the chest.", 120, { { 150, 160, 172, 255 }, { 42, 92, 184, 255 } } },
        { "tavern", "Tavern Keeper", "A white shirt, a leather apron and a red headband.", 180, { { 236, 230, 218, 255 }, { 154, 90, 44, 255 } } },
        { "paladin", "Golden Paladin", "Gold plate, a white cape, and a holy glow.", 280, { { 217, 165, 32, 255 }, { 255, 255, 255, 255 } } },
    },
    {   // pupesh
        { nullptr, "Own Clothes", "What they walked in with.", 0, { { 120, 110, 140, 255 }, { 70, 64, 84, 255 } } },
        { "watermelon", "Watermelon", "Striped green rind, pink trim and a pink cap.", 120, { { 35, 128, 46, 255 }, { 232, 72, 96, 255 } } },
        { "honeydew", "Honeydew", "Pale green and speckled, like a ripe one.", 180, { { 200, 224, 160, 255 }, { 106, 138, 80, 255 } } },
        { "melonlord", "Melon Lord", "Cantaloupe netting, a green cape, and a crown.", 280, { { 232, 138, 40, 255 }, { 35, 128, 46, 255 } } },
    },
    {   // ristyo
        { nullptr, "Own Clothes", "What they walked in with.", 0, { { 120, 110, 140, 255 }, { 70, 64, 84, 255 } } },
        { "partyshirt", "Party Shirt", "Teal with flowers on, cream shorts and shades.", 120, { { 22, 176, 168, 255 }, { 255, 138, 74, 255 } } },
        { "bartender", "Bartender", "A black waistcoat, white sleeves and a red bow tie.", 180, { { 36, 36, 46, 255 }, { 196, 32, 32, 255 } } },
        { "disco", "Disco Fever", "A white suit that sparkles, gold shoes and a pink glow.", 280, { { 244, 240, 250, 255 }, { 224, 64, 192, 255 } } },
    },
    {   // iliqnpuh
        { nullptr, "Own Clothes", "What they walked in with.", 0, { { 120, 110, 140, 255 }, { 70, 64, 84, 255 } } },
        { "honey", "Honey Hunter", "Honeycomb amber, with a drop of honey on the chest.", 120, { { 224, 154, 28, 255 }, { 255, 224, 112, 255 } } },
        { "grizzly", "Grizzly", "Brown fur and a claw mark. The Bear Jump, but bigger.", 180, { { 94, 58, 28, 255 }, { 232, 220, 192, 255 } } },
        { "polar", "Polar King", "White fur, ice-blue trim, an ice crown and a frosty glow.", 280, { { 234, 242, 250, 255 }, { 95, 183, 239, 255 } } },
    },
    {   // daniil
        { nullptr, "Own Clothes", "What they walked in with.", 0, { { 120, 110, 140, 255 }, { 70, 64, 84, 255 } } },
        { "fightergi", "Fighter's Gi", "A white gi, a black belt and a red headband.", 120, { { 236, 235, 230, 255 }, { 204, 32, 32, 255 } } },
        { "biker", "Biker Jacket", "Brown leather with studs, shades and a chain.", 180, { { 94, 58, 32, 255 }, { 216, 220, 228, 255 } } },
        { "goldsling", "Golden Slingshot", "A gold bomber with a coin on it, and a shine.", 280, { { 217, 165, 32, 255 }, { 20, 20, 26, 255 } } },
    },
    {   // storyteller
        { nullptr, "Own Clothes", "What they walked in with.", 0, { { 120, 110, 140, 255 }, { 70, 64, 84, 255 } } },
        { "bathrobe", "Bathrobe", "Finally. A fluffy white robe and blue slippers.", 120, { { 236, 239, 246, 255 }, { 95, 143, 208, 255 } } },
        { "koi", "Koi Kimono", "Red silk with koi swimming on it, and a black sash.", 180, { { 196, 32, 32, 255 }, { 255, 138, 32, 255 } } },
        { "bard", "Bard's Finery", "A green doublet, a purple cape and a purple cap.", 280, { { 35, 128, 46, 255 }, { 122, 52, 168, 255 } } },
    },
    {   // fairy
        { nullptr, "Own Clothes", "What they walked in with.", 0, { { 120, 110, 140, 255 }, { 70, 64, 84, 255 } } },
        { "moonlight", "Moonlight Gown", "Lavender and starry, with a soft glow.", 120, { { 184, 176, 216, 255 }, { 255, 255, 255, 255 } } },
        { "sprite", "Forest Sprite", "A leaf-green dress and a crown of flowers.", 180, { { 52, 160, 64, 255 }, { 255, 106, 154, 255 } } },
        { "queen", "Fairy Queen", "Gold, glittering, a crown, and a glow.", 280, { { 217, 165, 32, 255 }, { 255, 224, 96, 255 } } },
    },
    {   // momiche
        { nullptr, "Own Clothes", "What they walked in with.", 0, { { 120, 110, 140, 255 }, { 70, 64, 84, 255 } } },
        { "lumberjack", "Lumberjack", "Red plaid, jeans and a beanie. Goes with the axe.", 120, { { 184, 28, 28, 255 }, { 46, 70, 136, 255 } } },
        { "pumpkin", "Pumpkin Patch", "Orange, with a pumpkin on the front. Scarier for her than for them.", 180, { { 232, 122, 20, 255 }, { 20, 20, 26, 255 } } },
        { "screamqueen", "Scream Queen", "A black gown, a blood-red cape and a purple glow.", 280, { { 36, 28, 46, 255 }, { 196, 28, 60, 255 } } },
    },
    {   // gymrat
        { nullptr, "Own Clothes", "What they walked in with.", 0, { { 120, 110, 140, 255 }, { 70, 64, 84, 255 } } },
        { "neon", "Neon Stringer", "A neon tank top and a sweatband. Leg day is every day.", 120, { { 110, 208, 28, 255 }, { 244, 244, 248, 255 } } },
        { "cpp", "C++ Hoodie", "A navy hoodie that says C++, and headphones.", 180, { { 34, 54, 122, 255 }, { 122, 200, 255, 255 } } },
        { "olympia", "Mr. Olympia", "Gold, a crown and a glow. Creatine not included.", 280, { { 217, 165, 32, 255 }, { 255, 240, 160, 255 } } },
    },
    {   // vanessa
        { nullptr, "Own Clothes", "What they walked in with.", 0, { { 120, 110, 140, 255 }, { 70, 64, 84, 255 } } },
        { "redcarpet", "Red Carpet", "A red gown that glitters, and a gold chain.", 120, { { 184, 20, 42, 255 }, { 255, 208, 216, 255 } } },
        { "pinkhearts", "Pink Hearts", "Pink with white hearts, and heart-shaped shades.", 180, { { 232, 90, 154, 255 }, { 255, 255, 255, 255 } } },
        { "heartbreaker", "Heartbreaker", "A black gown of red hearts, a crown, and a red glow.", 280, { { 36, 28, 46, 255 }, { 232, 28, 60, 255 } } },
    },
    {   // galin
        { nullptr, "Own Clothes", "What they walked in with.", 0, { { 120, 110, 140, 255 }, { 70, 64, 84, 255 } } },
        { "tracksuit", "Tracksuit", "Navy, white stripes and a gold chain. The shop's dress code.", 120, { { 26, 46, 106, 255 }, { 244, 244, 248, 255 } } },
        { "leather", "Leather Jacket", "Brown leather and a gold chain. Smells of smoke.", 180, { { 94, 58, 32, 255 }, { 255, 214, 70, 255 } } },
        { "don", "The Don", "A pinstripe suit, a red tie and a fedora.", 280, { { 36, 36, 46, 255 }, { 184, 28, 28, 255 } } },
    },
    {   // filqcheto
        { nullptr, "Own Clothes", "What they walked in with.", 0, { { 120, 110, 140, 255 }, { 70, 64, 84, 255 } } },
        { "tiedye", "Tie-Dye Hoodie", "Every colour at once. He is very relaxed about it.", 120, { { 224, 64, 192, 255 }, { 96, 224, 255, 255 } } },
        { "grinder", "Grinder Green", "A green hoodie with a leaf on it, and a bucket hat.", 180, { { 35, 128, 46, 255 }, { 160, 240, 112, 255 } } },
        { "cosmic", "Cosmic Chill", "Deep purple with stars, a bucket hat and a glow.", 280, { { 46, 26, 106, 255 }, { 160, 160, 255, 255 } } },
    },
    {   // krisa
        { nullptr, "Own Clothes", "What they walked in with.", 0, { { 120, 110, 140, 255 }, { 70, 64, 84, 255 } } },
        { "boxer", "Boxing Champ", "Red with white trim, and a gold title belt.", 120, { { 200, 32, 42, 255 }, { 242, 201, 76, 255 } } },
        { "athleisure", "Athleisure", "A teal set with white stripes, and a cap.", 180, { { 20, 160, 160, 255 }, { 244, 244, 248, 255 } } },
        { "goldenglove", "Golden Glove", "Black and gold, a crown on the chest, and a shine.", 280, { { 36, 36, 46, 255 }, { 242, 201, 76, 255 } } },
    },

};

const char* outfitDir(int champion) {
    // the walk sheet's name without "walk-" and ".png": "walk-hero.png" -> "outfits/hero/"
    static char dirs[CHAMPION_COUNT][48];
    char* d = dirs[champion];
    if (!d[0]) {
        const char* w = CHAMPIONS[champion].walk;
        if (!strncmp(w, "walk-", 5)) w += 5;
        snprintf(d, sizeof dirs[0], "outfits/%.*s/", (int)(strlen(w) > 4 ? strlen(w) - 4 : strlen(w)), w);
    }
    return d;
}
