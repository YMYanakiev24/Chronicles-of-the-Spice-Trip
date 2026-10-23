/*
 * HubController.cs
 * ----------------------------------------------------------------------------
 * Navigation for the Citadel hub, ported from Scenes.js:
 *
 *   main menu → ▶ Play / the castle → THE PATH (boss map)
 *     ├─ click a boss → BOSS BATTLE → back to the path
 *     └─ Enter the Castle → CORRIDOR (two doors)
 *          ├─ Marin the Wizard → fog wipe → APOTHECARY (+ Enchanted Loom)
 *          └─ Changing Room → CHARACTER SELECT
 *
 * It owns the wallet and stash (Marin's shop) and the defeated-boss record.
 * ----------------------------------------------------------------------------
 */

using System;
using System.Collections.Generic;
using SpiceTrip.Core;
using SpiceTrip.Data;
using UnityEngine.UIElements;

namespace SpiceTrip.Hub
{
    public class HubController
    {
        public const string MusicMenu = "menu";

        public readonly GameContext Ctx;
        public readonly PathScreen Path;
        public readonly CorridorScreen Corridor;
        public readonly ShopScreen Shop;
        public readonly LoomModal Loom;
        public readonly CharacterSelectModal CharSelect;
        public readonly VisualElement Fog;

        /// <summary>null | path | corridor | shop | battle</summary>
        public string Active { get; private set; }

        public CharacterDef SelectedChar { get; private set; }
        public readonly HashSet<string> DefeatedBosses;

        readonly Action _showMenu, _hideMenu;
        readonly Action<BossDef, CharacterDef> _startBattle;
        readonly IVisualElementScheduler _scheduler;

        public HubController(GameContext ctx, VisualElement sceneLayer, VisualElement modalLayer, VisualElement fogLayer,
            Action showMenu, Action hideMenu, Action<BossDef, CharacterDef> startBattle)
        {
            Ctx = ctx;
            _showMenu = showMenu;
            _hideMenu = hideMenu;
            _startBattle = startBattle;
            _scheduler = sceneLayer.schedule;
            DefeatedBosses = new HashSet<string>(ctx.Hub.defeated);
            SelectedChar = ctx.Db.Character(ctx.Hub.selectedChar);
            if (SelectedChar == null || SelectedChar.empty) SelectedChar = ctx.Db.characters.Count > 0 ? ctx.Db.characters[0] : null;

            Path = new PathScreen(ctx, FightBoss, () => { ctx.Audio.Sfx("ui"); EnterCorridor(); }, () => { ctx.Audio.Sfx("ui"); Back("menu"); });
            Corridor = new CorridorScreen(ctx, EnterShop, () => { ctx.Audio.Sfx("ui"); OpenCharacterSelect(); }, () => { ctx.Audio.Sfx("ui"); Back("path"); });
            Shop = new ShopScreen(ctx, e => Buy(e.id, e.price), OpenTapestry, () => { ctx.Audio.Sfx("ui"); Back("corridor"); });
            sceneLayer.Add(Path.Root);
            sceneLayer.Add(Corridor.Root);
            sceneLayer.Add(Shop.Root);

            Loom = new LoomModal(ctx, FileDialog.PickImage);
            CharSelect = new CharacterSelectModal(ctx, SelectChar);
            modalLayer.Add(CharSelect.Root);
            modalLayer.Add(Loom.Root);

            Fog = UiKit.El("scene-fog", "scene-fog");
            Fog.pickingMode = PickingMode.Ignore;
            UiKit.SetBackground(Fog, UiKit.FogTexture());
            fogLayer.Add(Fog);
        }

        // The gold shown and spent: the hub wallet (the RPG's live purse joins in Phase 2).
        public int Wallet
        {
            get => Ctx.Hub.gold;
            set { Ctx.Hub.gold = value; Ctx.SaveHub(); }
        }

        // =====================================================================
        // Navigation
        // =====================================================================
        void HideAllScenes()
        {
            Path.Show(false);
            Corridor.Show(false);
            Shop.Show(false);
        }

        // Play / menu-castle land here: the boss map.
        public void EnterPath(bool fromBattle = false)
        {
            if (!fromBattle) _hideMenu();
            HideAllScenes();
            Path.Show(true);
            Active = "path";
            Path.RefreshBossMarks(DefeatedBosses);
        }

        public void EnterCorridor()
        {
            _hideMenu();
            HideAllScenes();
            Corridor.Show(true);
            Active = "corridor";
        }

        public void EnterShop()
        {
            Ctx.Audio.Sfx("quest");
            FogWipe(() =>
            {
                HideAllScenes();
                Shop.Show(true);
                Active = "shop";
                Shop.SetGold(Wallet);
            });
        }

        public void Back(string target)
        {
            if (target == "menu")
            {
                HideAllScenes();
                Active = null;
                _showMenu();
            }
            else if (target == "path")
            {
                HideAllScenes();
                Path.Show(true);
                Active = "path";
                Path.RefreshBossMarks(DefeatedBosses);
            }
            else if (target == "corridor")
            {
                FogWipe(() =>
                {
                    HideAllScenes();
                    Corridor.Show(true);
                    Active = "corridor";
                });
            }
        }

        // A mystical fog wipe: fade fog in, swap scenes at the peak, fade out.
        public bool FogShowing { get; private set; }

        void FogWipe(Action swap)
        {
            FogShowing = true;
            Fog.AddToClassList("show");
            _scheduler.Execute(swap).StartingIn(360);
            _scheduler.Execute(() => { Fog.RemoveFromClassList("show"); FogShowing = false; }).StartingIn(420);
        }

        // --- Bosses ---------------------------------------------------------------
        public void FightBoss(BossDef boss)
        {
            Ctx.Audio.Sfx("quest");
            HideAllScenes();
            Active = "battle";
            _startBattle(boss, SelectedChar);
        }

        public void MarkBossDefeated(string id)
        {
            DefeatedBosses.Add(id);
            Ctx.Hub.defeated = new List<string>(DefeatedBosses);
            Ctx.SaveHub();
        }

        // --- Character select (Changing Room) -------------------------------------
        public void OpenCharacterSelect() => CharSelect.Open(SelectedChar?.id);

        public void SelectChar(CharacterDef ch)
        {
            SelectedChar = ch;
            Ctx.Hub.selectedChar = ch.id;
            Ctx.SaveHub();
            Ctx.Audio.Sfx("pickup");
            CharSelect.Open(ch.id);   // re-render with the new selection highlighted
            _scheduler.Execute(CharSelect.Close).StartingIn(220);
        }

        // =====================================================================
        // Shop
        // =====================================================================
        public bool Buy(string itemId, int price)
        {
            if (Wallet < price)
            {
                Shop.SetMarinLine("Not enough coin, friend. Come back richer.");
                Ctx.Audio.Sfx("ui");
                return false;
            }
            Ctx.Hub.AddToStash(itemId);
            Wallet -= price;   // saves the hub (wallet + stash)
            Shop.SetGold(Wallet);
            var item = Ctx.Db.Item(itemId);
            Shop.SetMarinLine($"A fine choice — one {item?.name ?? itemId}. It will await you on the road.");
            Ctx.Audio.Sfx("pickup");
            return true;
        }

        public void OpenTapestry() => Loom.Open();

        // =====================================================================
        // Per-frame animation of the procedural scenes and the Loom
        // =====================================================================
        public void Tick(float time, float dt)
        {
            if (Active == "corridor") Corridor.Tick(time);
            else if (Active == "shop") Shop.Tick(time);
            else if (Active == "path") Path.Tick(time);
            Loom.Tick(dt);
        }
    }
}
