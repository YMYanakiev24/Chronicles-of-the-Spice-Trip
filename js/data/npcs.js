// NPC definitions with dialogue trees
export const NPCS = {
  magister_aelion: {
    id: 'magister_aelion',
    name: 'Magister Aelion',
    icon: '🧙‍♂️',
    role: 'Quest Giver — Wizard',
    color: 0x4444aa,
    robeColor: 0x223388,
    questId: 'stolen_codex',
    position: { x: -8, z: -5 },
    dialogues: {
      greeting: {
        text: "Ah, traveler! Thank the ancient stars you have come. I am Magister Aelion, keeper of the Sacred Codex... or I was, until those cursed Shadow Wraiths stole it from my study three nights past.",
        options: [
          { text: "Tell me more about this codex.", next: 'codex_info' },
          { text: "I'll help you recover it.", next: 'accept_quest', action: 'start_quest:stolen_codex' },
          { text: "I must be going.", next: null }
        ]
      },
      codex_info: {
        text: "The Codex of Aelion contains the accumulated spell-knowledge of twelve god-mages. In the wrong hands, it could reshape reality itself. The Shadow Wraiths lair in the Dark Ruins, northeast of the village. Please, recover it!",
        options: [
          { text: "I'll retrieve it for you.", next: 'accept_quest', action: 'start_quest:stolen_codex' },
          { text: "I'll return when I'm ready.", next: null }
        ]
      },
      accept_quest: {
        text: "Bless you, champion! The Shadow Wraiths are dangerous — use fire magic against them, it disrupts their dark essence. The ruins are northeast. Return the codex, and you shall be handsomely rewarded.",
        options: [
          { text: "I will succeed.", next: null }
        ]
      },
      quest_active: {
        text: "The Shadow Wraiths are creatures of darkness — fire magic is their bane. Find the codex in the ruins to the northeast, beyond the dead oaks.",
        options: [
          { text: "I'll head there now.", next: null }
        ]
      },
      quest_complete: {
        text: "You found it! The Codex of Aelion, returned at last! I... I cannot express my gratitude. Take this Arcane Focus Crystal — it will amplify your magical abilities greatly. You have saved this realm from tremendous darkness.",
        options: [
          { text: "The realm is worth protecting.", next: null }
        ]
      }
    }
  },

  wounded_traveler: {
    id: 'wounded_traveler',
    name: 'Mira the Wanderer',
    icon: '🤕',
    role: 'Quest Giver — Wounded Traveler',
    color: 0x996633,
    questId: 'stone_curse',
    position: { x: 12, z: 3 },
    dialogues: {
      greeting: {
        text: "*wincing in pain* Please... help me. I was traveling the eastern road when that monstrous serpent appeared from the ruins. It turned my companion to stone! I barely escaped. Those ruins... are cursed.",
        options: [
          { text: "What creature did this?", next: 'creature_info' },
          { text: "I'll deal with this creature.", next: 'accept_quest', action: 'start_quest:stone_curse' },
          { text: "I'll return when I can help.", next: null }
        ]
      },
      creature_info: {
        text: "A Basilisk — enormous, scaled like emerald armor. Its gaze... my companion looked at it directly and was turned to stone in an instant. It lives in the Ancient Ruins to the east. Please, no more travelers should suffer this fate.",
        options: [
          { text: "I'll slay the Basilisk.", next: 'accept_quest', action: 'start_quest:stone_curse' },
          { text: "I need to be stronger first.", next: null }
        ]
      },
      accept_quest: {
        text: "Thank you... thank you so much. Remember — never meet its eyes directly! Use lightning magic if you have it. The Basilisk's stone-hard scales crack under electrical discharge. May the gods guide your blade.",
        options: [
          { text: "Rest now. I'll handle this.", next: null }
        ]
      },
      quest_active: {
        text: "The Ancient Ruins are east of the forest path. The Basilisk won't go further than its lair — it guards something there. Please be careful...",
        options: [
          { text: "I will be.", next: null }
        ]
      },
      quest_complete: {
        text: "It's... it's truly dead? You've done it! My companion is still stone, but at least no others will suffer. Here, take this — everything I have left from my journey. You are truly a hero of the Emerald Realm.",
        options: [
          { text: "Keep safe, Mira.", next: null }
        ]
      }
    }
  },

  old_bramble: {
    id: 'old_bramble',
    name: 'Old Bramble',
    icon: '🌳',
    role: 'Ancient Tree Spirit',
    color: 0x336622,
    questId: 'corrupted_grove',
    position: { x: 5, z: -20 },
    dialogues: {
      greeting: {
        text: "*ancient groaning voice* Traveler... I am Old Bramble, spirit of the Northern Grove. I have stood for a thousand years... but now darkness seeps through my roots. The corruption... it comes from three nodes of shadow magic planted by the servants of the Shadow Titan.",
        options: [
          { text: "How can I help?", next: 'help_info' },
          { text: "I'll cleanse the grove.", next: 'accept_quest', action: 'start_quest:corrupted_grove' },
          { text: "I'll return when I'm ready.", next: null }
        ]
      },
      help_info: {
        text: "The three corruption nodes must be destroyed with spell-fire. Then the Dark Treant — once my beloved guardian — must be freed from corruption through battle. Only then will the grove heal. I cannot ask this of you lightly, but the forest... it weeps.",
        options: [
          { text: "I will cleanse the grove.", next: 'accept_quest', action: 'start_quest:corrupted_grove' },
          { text: "I'll return with more power.", next: null }
        ]
      },
      accept_quest: {
        text: "The ancient wood remembers kindness. Follow the corrupted stream north. The nodes pulse with dark light — you cannot miss them. The Dark Treant will be the greatest challenge. Use fire — corruption fears flame.",
        options: [
          { text: "The grove will bloom again.", next: null }
        ]
      },
      quest_active: {
        text: "Three nodes of darkness still poison my roots. Destroy them, then face the Treant. The forest will guide you...",
        options: [
          { text: "I'm on my way.", next: null }
        ]
      },
      quest_complete: {
        text: "*voice fills with warm resonance* The darkness... recedes. After so many cycles of corruption, the grove breathes again. Take this amulet, traveler. It carries the blessing of every tree in the Emerald Realm. Wear it with the honor you have shown today.",
        options: [
          { text: "The forest shall flourish.", next: null }
        ]
      }
    }
  },

  ancient_spirit: {
    id: 'ancient_spirit',
    name: 'Aurelius Echo',
    icon: '✨',
    role: 'Divine Spirit',
    color: 0xddaa00,
    questId: 'gods_relic',
    position: { x: -25, z: 10 },
    dialogues: {
      greeting: {
        text: "*shimmering voice like wind through gold* I am the Echo of Aurelius, sky-god of the elder age. I have waited long for a soul worthy of the trial. My Sigil rests in the Forgotten Temple to the west, guarded by my ancient sentinels. Will you reclaim what was lost?",
        options: [
          { text: "What is the Aureate Sigil?", next: 'sigil_info' },
          { text: "I accept the divine trial.", next: 'accept_quest', action: 'start_quest:gods_relic' },
          { text: "I am not yet ready.", next: null }
        ]
      },
      sigil_info: {
        text: "The Aureate Sigil is the anchor of divine balance in this realm. Without it, the veil between mortal world and Shadow Titan's domain thins. The Shadow Titan seeks it. You must reach it first. My temple guardians will test you — prove yourself worthy.",
        options: [
          { text: "I will face the trial.", next: 'accept_quest', action: 'start_quest:gods_relic' }
        ]
      },
      accept_quest: {
        text: "The temple lies west, past the broken archway of the old gods. My stone sentinels will challenge you — but they are not your enemies, only tests of your power. Prove yourself worthy, claim the Sigil, and the path to my realm shall open before you.",
        options: [
          { text: "I shall not fail.", next: null }
        ]
      },
      quest_active: {
        text: "The temple of Aurelius awaits you to the west. The sentinels guard it still. Only the worthy may claim what lies within.",
        options: [
          { text: "I press on.", next: null }
        ]
      },
      quest_complete: {
        text: "*blazing with divine light* You have proven yourself worthy of the old gods' blessing! The Sigil is restored, the divine balance holds, and the path to my realm stands open. Take the Sigil, champion. Let its light guide you through the darkness to come.",
        options: [
          { text: "The realm shall be protected.", next: null }
        ]
      }
    }
  },

  sentinel_gareth: {
    id: 'sentinel_gareth',
    name: 'Sentinel Gareth',
    icon: '⚔️',
    role: 'Village Guardian',
    color: 0x778899,
    questId: 'village_siege',
    position: { x: 0, z: 8 },
    isIntroNPC: true,
    dialogues: {
      greeting: {
        text: "Halt, stranger! ...Actually, thank the gods you've come. I'm Gareth, captain of Emberkeep's guard. Dark creatures massed on the forest edge last night. We barely held them back. I need a spell-blade — someone who can fight AND cast. That's you, isn't it?",
        options: [
          { text: "Tell me what happened.", next: 'crisis_info' },
          { text: "I'll defend the village.", next: 'accept_quest', action: 'start_quest:village_siege' },
          { text: "First, tell me about this land.", next: 'lore_info' }
        ]
      },
      crisis_info: {
        text: "Three nights ago, the forest wards went dark — someone disabled them from inside. Now Shadow Wraiths and worse things test our defenses each night. Tonight will be the worst yet. I can feel it. Please — our village needs you.",
        options: [
          { text: "I'll defend Emberkeep.", next: 'accept_quest', action: 'start_quest:village_siege' },
          { text: "I need time to prepare.", next: null }
        ]
      },
      lore_info: {
        text: "You're new to the Emerald Realm? This is Emberkeep — last free village in the Thornwood region. Beyond the forest to the northeast lie ancient ruins. To the east, the crumbling kingdom of the old crusaders. West holds the Forgotten Temple. North... well, best you discover north for yourself.",
        options: [
          { text: "I'll help defend the village.", next: 'accept_quest', action: 'start_quest:village_siege' },
          { text: "Thank you for the information.", next: null }
        ]
      },
      accept_quest: {
        text: "Thank you, champion. Use your spells wisely — the first wave will be Shadow Wraiths, fast and shadow-touched. Then Stone Golems. The third wave... I've never seen their commander, but the scouts say he wears a mask of bone. Be ready.",
        options: [
          { text: "The village will stand.", next: null }
        ]
      },
      quest_active: {
        text: "The creatures gather at the forest edge when darkness falls. Stand ready — the attack could come at any moment. Protect the village center at all costs.",
        options: [
          { text: "I'm prepared.", next: null }
        ]
      },
      quest_complete: {
        text: "By the ancient gods, you did it! The siege is broken! Emberkeep stands! I've never seen fighting like that in all my years as a sentinel. Take these — the Hero's Emblem marks you as a defender of this village. You are welcome here always.",
        options: [
          { text: "For Emberkeep. Always.", next: null }
        ]
      }
    }
  },

  kaelos_alchemist: {
    id: 'kaelos_alchemist',
    name: 'Kaelos the Alchemist',
    icon: '⚗️',
    role: 'Merchant & Alchemist',
    color: 0x884400,
    isShop: true,
    position: { x: 6, z: 5 },
    dialogues: {
      greeting: {
        text: "Welcome, welcome! Kaelos's Curious Concoctions — finest potions and reagents in all the Emerald Realm! What say you? Perhaps something to keep you alive out there? The dark grows bold these days...",
        options: [
          { text: "Show me your wares.", next: null, action: 'open_shop' },
          { text: "Do you know anything about the area?", next: 'area_info' },
          { text: "Farewell for now.", next: null }
        ]
      },
      area_info: {
        text: "Aye, I know the realm well enough! The forest north is badly cursed — pay Old Bramble a visit if you dare. East ruins are basilisk territory. West holds a temple that makes even the soldiers nervous. And whatever you do... don't go to the dark hill at midnight. That's how my last assistant disappeared.",
        options: [
          { text: "Let me see your shop.", next: null, action: 'open_shop' },
          { text: "Thanks for the warning.", next: null }
        ]
      }
    },
    shop: [
      { id: 'health_potion', name: 'Vitality Potion', icon: '🧪', desc: 'Restores 50 HP instantly.', cost: 25, effect: { type: 'heal', value: 50 } },
      { id: 'mana_potion', name: 'Arcane Draught', icon: '💧', desc: 'Restores 40 mana instantly.', cost: 20, effect: { type: 'mana', value: 40 } },
      { id: 'fire_shard', name: 'Ember Shard', icon: '🔥', desc: '+10 fire spell damage for 60s.', cost: 40, effect: { type: 'buff', stat: 'fireDamage', value: 10, duration: 60 } },
      { id: 'speed_elixir', name: 'Swiftfoot Elixir', icon: '⚡', desc: '+50% movement speed for 30s.', cost: 35, effect: { type: 'buff', stat: 'speed', value: 1.5, duration: 30 } }
    ]
  }
};
