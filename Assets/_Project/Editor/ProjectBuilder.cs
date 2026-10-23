/*
 * ProjectBuilder.cs
 * ----------------------------------------------------------------------------
 * Rebuilds everything that would otherwise be clicked together in the editor,
 * so the project is reproducible from code and from the command line:
 *   - player settings (name, resolution, windowed default)
 *   - the UI Toolkit theme + PanelSettings
 *   - the three sprite materials for the arena shader
 *   - the GameDatabase asset (from the exported JS data)
 *   - the Boot scene (one GameRoot object) and the build scene list
 *
 *   Unity.exe -batchmode -quit -projectPath . -executeMethod SpiceTrip.EditorTools.ProjectBuilder.RebuildAll
 * ----------------------------------------------------------------------------
 */

using System.IO;
using SpiceTrip.Core;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.SceneManagement;
using UnityEngine.UIElements;

namespace SpiceTrip.EditorTools
{
    public static class ProjectBuilder
    {
        public const string BootScene = "Assets/_Project/Scenes/Boot.unity";
        const string UiFolder = "Assets/_Project/Resources/UI";
        const string ThemePath = UiFolder + "/SpiceTheme.tss";
        const string PanelPath = UiFolder + "/PanelSettings.asset";
        const string MaterialFolder = "Assets/_Project/Resources/Materials";
        const string ShaderPath = "Assets/_Project/Shaders/SpiceSprite.shader";

        [MenuItem("Spice Trip/Rebuild Project")]
        public static void RebuildAll()
        {
            ConfigurePlayer();
            BuildUiAssets();
            BuildMaterials();
            DataImporter.Import();
            BuildBootScene();
            AssetDatabase.SaveAssets();
            AssetDatabase.Refresh();
            Debug.Log("ProjectBuilder: rebuild complete");
        }

        static void ConfigurePlayer()
        {
            PlayerSettings.companyName = "YMYanakiev24";
            PlayerSettings.productName = "Chronicles of the Spice Trip";
            PlayerSettings.defaultScreenWidth = 1280;
            PlayerSettings.defaultScreenHeight = 720;
            PlayerSettings.fullScreenMode = FullScreenMode.Windowed;
            PlayerSettings.resizableWindow = true;
            PlayerSettings.runInBackground = true;
            PlayerSettings.visibleInBackground = true;
        }

        static void BuildUiAssets()
        {
            Directory.CreateDirectory(UiFolder);
            if (!File.Exists(ThemePath))
            {
                File.WriteAllText(ThemePath, "@import url(\"unity-theme://default\");\n");
                AssetDatabase.ImportAsset(ThemePath);
            }
            var theme = AssetDatabase.LoadAssetAtPath<ThemeStyleSheet>(ThemePath);

            var panel = AssetDatabase.LoadAssetAtPath<PanelSettings>(PanelPath);
            if (panel == null)
            {
                panel = ScriptableObject.CreateInstance<PanelSettings>();
                AssetDatabase.CreateAsset(panel, PanelPath);
            }
            panel.themeStyleSheet = theme;
            panel.scaleMode = PanelScaleMode.ScaleWithScreenSize;
            panel.referenceResolution = new Vector2Int(1280, 720);
            panel.screenMatchMode = PanelScreenMatchMode.MatchWidthOrHeight;
            panel.match = 0.5f;
            panel.clearColor = false;
            panel.sortingOrder = 10;
            EditorUtility.SetDirty(panel);
        }

        static void BuildMaterials()
        {
            Directory.CreateDirectory(MaterialFolder);
            AssetDatabase.ImportAsset(ShaderPath);
            var shader = AssetDatabase.LoadAssetAtPath<Shader>(ShaderPath);
            if (shader == null) { Debug.LogError("SpiceSprite shader missing"); return; }
            // source-over, 'lighter' (additive) and 'screen' — the canvas composite modes the web game used
            Material("SpriteAlpha", shader, BlendMode.SrcAlpha, BlendMode.OneMinusSrcAlpha, 0);
            Material("SpriteAdditive", shader, BlendMode.One, BlendMode.One, 1);
            Material("SpriteScreen", shader, BlendMode.OneMinusDstColor, BlendMode.One, 1);
        }

        static void Material(string name, Shader shader, BlendMode src, BlendMode dst, float premultiply)
        {
            var path = $"{MaterialFolder}/{name}.mat";
            var mat = AssetDatabase.LoadAssetAtPath<Material>(path);
            if (mat == null)
            {
                mat = new Material(shader);
                AssetDatabase.CreateAsset(mat, path);
            }
            mat.shader = shader;
            mat.SetFloat("_SrcBlend", (float)src);
            mat.SetFloat("_DstBlend", (float)dst);
            mat.SetFloat("_Premultiply", premultiply);
            mat.SetFloat("_Brightness", 1);
            EditorUtility.SetDirty(mat);
        }

        static void BuildBootScene()
        {
            Directory.CreateDirectory(Path.GetDirectoryName(BootScene));
            var scene = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene, NewSceneMode.Single);
            var root = new GameObject("GameRoot");
            root.AddComponent<GameRoot>();
            EditorSceneManager.SaveScene(scene, BootScene);
            EditorBuildSettings.scenes = new[] { new EditorBuildSettingsScene(BootScene, true) };
        }
    }
}
