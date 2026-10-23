/*
 * AutopilotRunner.cs
 * ----------------------------------------------------------------------------
 * Runs the in-game Autopilot inside the Unity Editor's Play mode, for machines
 * where unsigned standalone builds can't be launched (Windows Smart App
 * Control). Opens the Boot scene and presses Play; GameRoot picks up the
 * -autopilot/-savedir arguments and the Autopilot exits the editor when done.
 *
 *   Unity.exe -projectPath . -executeMethod SpiceTrip.EditorTools.AutopilotRunner.Run -autopilot <dir> -savedir <dir>/save
 * ----------------------------------------------------------------------------
 */

using UnityEditor;
using UnityEditor.SceneManagement;

namespace SpiceTrip.EditorTools
{
    public static class AutopilotRunner
    {
        public static void Run()
        {
            ProjectBuilder.RebuildAll();
            EditorSceneManager.OpenScene(ProjectBuilder.BootScene);
            EditorApplication.isPlaying = true;
        }
    }
}
