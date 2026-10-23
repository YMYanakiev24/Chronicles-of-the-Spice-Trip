/*
 * FileDialog.cs
 * ----------------------------------------------------------------------------
 * "Upload image" for the Enchanted Loom. The browser gave the web version a
 * file picker for free; here it is the Editor's open panel, or the native
 * Windows open-file dialog (comdlg32) in a standalone build.
 * ----------------------------------------------------------------------------
 */

using System;
using System.Runtime.InteropServices;

namespace SpiceTrip.Hub
{
    public static class FileDialog
    {
        const string Title = "Choose an image for the Loom";

        public static string PickImage()
        {
#if UNITY_EDITOR
            var path = UnityEditor.EditorUtility.OpenFilePanelWithFilters(Title, "", new[] { "Images", "png,jpg,jpeg", "All files", "*" });
            return string.IsNullOrEmpty(path) ? null : path;
#elif UNITY_STANDALONE_WIN
            return Win32.Open(Title, "Images (*.png;*.jpg;*.jpeg)\0*.png;*.jpg;*.jpeg\0All files\0*.*\0\0");
#else
            return null;
#endif
        }

#if UNITY_STANDALONE_WIN && !UNITY_EDITOR
        static class Win32
        {
            [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
            struct OpenFileName
            {
                public int lStructSize;
                public IntPtr hwndOwner;
                public IntPtr hInstance;
                public string lpstrFilter;
                public string lpstrCustomFilter;
                public int nMaxCustFilter;
                public int nFilterIndex;
                public IntPtr lpstrFile;
                public int nMaxFile;
                public string lpstrFileTitle;
                public int nMaxFileTitle;
                public string lpstrInitialDir;
                public string lpstrTitle;
                public int Flags;
                public short nFileOffset;
                public short nFileExtension;
                public string lpstrDefExt;
                public IntPtr lCustData;
                public IntPtr lpfnHook;
                public string lpTemplateName;
                public IntPtr pvReserved;
                public int dwReserved;
                public int FlagsEx;
            }

            [DllImport("comdlg32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
            static extern bool GetOpenFileNameW(ref OpenFileName ofn);

            [DllImport("user32.dll")]
            static extern IntPtr GetActiveWindow();

            const int OFN_FILEMUSTEXIST = 0x1000, OFN_PATHMUSTEXIST = 0x800, OFN_NOCHANGEDIR = 0x8, OFN_EXPLORER = 0x80000;
            const int MaxPath = 4096;

            public static string Open(string title, string filter)
            {
                var buffer = Marshal.AllocHGlobal(MaxPath * 2);
                try
                {
                    Marshal.WriteInt16(buffer, 0);
                    var ofn = new OpenFileName
                    {
                        lStructSize = Marshal.SizeOf<OpenFileName>(),
                        hwndOwner = GetActiveWindow(),
                        lpstrFilter = filter,
                        nFilterIndex = 1,
                        lpstrFile = buffer,
                        nMaxFile = MaxPath,
                        lpstrTitle = title,
                        Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER,
                    };
                    return GetOpenFileNameW(ref ofn) ? Marshal.PtrToStringUni(buffer) : null;
                }
                finally
                {
                    Marshal.FreeHGlobal(buffer);
                }
            }
        }
#endif
    }
}
