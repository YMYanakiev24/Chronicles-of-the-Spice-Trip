/*
 * BuildScript.cs
 * ----------------------------------------------------------------------------
 * Command-line Windows build:
 *   Unity.exe -batchmode -quit -projectPath . -executeMethod SpiceTrip.EditorTools.BuildScript.BuildWindows
 * Produces Build/Windows/SpiceTrip.exe and exits non-zero if the build fails.
 * ----------------------------------------------------------------------------
 */

using UnityEditor;
using UnityEditor.Build.Reporting;
using UnityEngine;

namespace SpiceTrip.EditorTools
{
    public static class BuildScript
    {
        public const string Output = "Build/Windows/SpiceTrip.exe";

        [MenuItem("Spice Trip/Build Windows")]
        public static void BuildWindows()
        {
            ProjectBuilder.RebuildAll();
            var report = BuildPipeline.BuildPlayer(new BuildPlayerOptions
            {
                scenes = new[] { ProjectBuilder.BootScene },
                locationPathName = Output,
                target = BuildTarget.StandaloneWindows64,
                options = BuildOptions.None,
            });
            var s = report.summary;
            Debug.Log($"Build {s.result}: {s.totalErrors} errors, {s.totalWarnings} warnings, {s.totalSize / (1024 * 1024)} MB, {s.totalTime}");
            if (Application.isBatchMode && s.result != BuildResult.Succeeded) EditorApplication.Exit(1);
        }
    }
}
