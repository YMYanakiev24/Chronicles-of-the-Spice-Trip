/*
 * GameContext.cs
 * ----------------------------------------------------------------------------
 * The shared services every screen needs: content, saves, settings and sound.
 * Created once by GameRoot and handed to each screen (the web version reached
 * these through the global Game object).
 * ----------------------------------------------------------------------------
 */

using SpiceTrip.Data;

namespace SpiceTrip.Core
{
    public interface ISfx
    {
        void Sfx(string name);
        void PlayMusic(string mood);
    }

    public class GameContext
    {
        public readonly GameDatabase Db;
        public readonly SaveService Save;
        public readonly HubData Hub;
        public readonly SettingsData Settings;
        public readonly ISfx Audio;

        public GameContext(GameDatabase db, SaveService save, ISfx audio)
        {
            Db = db;
            Save = save;
            Audio = audio;
            Settings = save.LoadSettings();
            Hub = save.LoadHub(db.hubStartingGold);
        }

        public void SaveHub() => Save.SaveHub(Hub);
    }
}
