using System;
using System.Collections.Generic;
using System.Windows.Forms;
using System.Diagnostics;
using System.IO;
using System.Security.Principal;
using GWCA.Memory;
using System.Runtime.InteropServices;
using System.Net;
using System.Web.Script.Serialization;

namespace CSLauncher {
	struct GithubAsset
    {
        public string name { get; set; }
        public string browser_download_url { get; set; }
    }
    struct GithubRelease
    {
        public string tag_name { get; set; }
        public string body { get; set; }
        public List<GithubAsset> assets { get; set; }
    };
    static class CSLauncher {
        const string DLL_NAME = "JonsGWToolbox.dll"; // "GWToolbox.dll";

        const string GITHUB_USER = "3vcloud"; // "HasKha";

        static readonly string[] LOADMODULE_RESULT_MESSAGES = {
            DLL_NAME +" successfully loaded.",
            DLL_NAME +" not found.",
            "kernel32.dll not found.\nHow the fuck did you do this.",
            "LoadLibraryW not found in kernel32.dll... what",
            "VirtualAllocEx allocation unsuccessful.",
            "WriteProcessMemory not able to write path.",
            "Remote thread not spawned.",
            "Remote thread did not finish dll initialization.",
            "VirtualFreeEx deallocation unsuccessful."
        };

        [DllImport("user32.dll")]
        private static extern IntPtr FindWindowEx(IntPtr hWndParent, IntPtr hWndChildAfter, string lpszClass,string lpszWindow);
        [DllImport("user32.dll", SetLastError = true)]
        private static extern uint GetWindowThreadProcessId(IntPtr hWnd, out Int32 lpdwProcessId);
        [DllImport("kernel32.dll")]
        static extern IntPtr LoadLibraryEx(string lpFileName, IntPtr hFile, uint dwFlags);
        [DllImport("kernel32.dll")]
        static extern bool FreeLibrary(IntPtr hModule);
        [DllImport("kernel32.dll")]
        public static extern IntPtr GetProcAddress(IntPtr hModule, string procedureName);
        static bool hasElevatedGWProcesses()
        {
            IntPtr hWndTargetWindow = FindWindowEx(IntPtr.Zero, IntPtr.Zero, "ArenaNet_Dx_Window_Class", null);
            Process[] processList = Process.GetProcesses();
            while (hWndTargetWindow != IntPtr.Zero)
            {
                Int32 pid = 0;
                GetWindowThreadProcessId(hWndTargetWindow, out pid);
                foreach (Process p in processList)
                {
                    if (p.Id != pid)
                        continue;
                    if (ProcessHelper.IsProcessOwnerAdmin(p))
                        return true;
                    break;
                }
                hWndTargetWindow = FindWindowEx(IntPtr.Zero, hWndTargetWindow, "ArenaNet_Dx_Window_Class", null);
            }
            return false;
        }
        static string GetLatestVersion()
        {
            ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls12;
            WebClient host = new WebClient();
            host.Headers.Add(HttpRequestHeader.UserAgent, "GWToolboxpp Launcher");
            string remoteversion = "";
            string dllurl = "";
            int tries = 0;

            while (tries < 3 && remoteversion.Length == 0)
            {
                try
                {
                    string json = host.DownloadString("https://api.github.com/repos/" + GITHUB_USER +"/GWToolboxpp/releases");
                    JavaScriptSerializer serializer = new JavaScriptSerializer();
                    var items = serializer.Deserialize<List<GithubRelease>>(json);
                    foreach (var release in items)
                    {
                        int version_number_len = release.tag_name.IndexOf("_Release");
                        if (version_number_len == -1)
                            continue;
                        foreach (var asset in release.assets)
                        {
                            if (!asset.name.Equals(DLL_NAME))
                                continue;
                            remoteversion = release.tag_name.Substring(0, version_number_len);
                            dllurl = asset.browser_download_url;
                            break;
                        }
                        if (remoteversion.Length > 0)
                            break;
                    }
                    if (remoteversion.Length == 0)
                        remoteversion = "0";
                }
                catch (Exception e)
                {
                    Console.WriteLine(e.Message);
                    // todo
                }
                tries++;
            }
            return remoteversion;

        }
        static string GetLocalVersion(string dllfile)
        {
            if (!File.Exists(dllfile))
                return "";
            const uint LOAD_LIBRARY_AS_DATAFILE = 0x00000002;
            IntPtr hToolbox = LoadLibraryEx(dllfile,IntPtr.Zero, LOAD_LIBRARY_AS_DATAFILE);
            if (hToolbox == IntPtr.Zero)
                return "";
            IntPtr GWToolboxVersion_Func = GetProcAddress(hToolbox, "GWToolboxVersion");
            if (GWToolboxVersion_Func == IntPtr.Zero)
                return "";
            return Marshal.PtrToStringAnsi(GWToolboxVersion_Func);
        }
        [STAThread]
        static void Main(string[] args) {
            Application.EnableVisualStyles();

            // Check for admin rights.
            WindowsIdentity identity = WindowsIdentity.GetCurrent();
            WindowsPrincipal principal = new WindowsPrincipal(identity);
            bool isElevated = principal.IsInRole(WindowsBuiltInRole.Administrator);

            // names and paths
            string toolboxdir = Environment.GetEnvironmentVariable("LocalAppData") + Path.DirectorySeparatorChar + "GWToolboxpp" + Path.DirectorySeparatorChar;
            string inifile = toolboxdir + "GWToolbox.ini";
			string dllfile = Path.GetDirectoryName(Application.ExecutablePath) + Path.DirectorySeparatorChar + DLL_NAME;

			// Install resources
			ResInstaller installer = new ResInstaller();
            installer.Install();

            // Download or update if needed. If dll file exists in current directory, use it.
			if(!File.Exists(dllfile))
				dllfile = toolboxdir + DLL_NAME;
            if (File.Exists(dllfile) && (new FileInfo(dllfile).Length) < 1)
                File.Delete(dllfile); // Delete file if exists with 0 length
            if (!File.Exists(dllfile)) {
                ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls12;
                WebClient host = new WebClient();
                host.Headers.Add(HttpRequestHeader.UserAgent, "GWToolboxpp Launcher");
                string remoteversion = GetLatestVersion();
                if (remoteversion.Length == 0)
                {
                    MessageBox.Show("Failed to fetch latest GWToolbox++ version.\n Check your internet connection and try again",
                        "GWToolbox++ Error",
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Error);
                    return;
                }
                string dllurl = "https://github.com/" + GITHUB_USER + "/GWToolboxpp/releases/download/" + remoteversion + "_Release/" + DLL_NAME;
                int tries = 0;
                // This bit will take a while...
                while (tries < 3 && dllurl.Length > 0 && !File.Exists(dllfile))
                {
                    try
                    {
                        host.DownloadFile(dllurl, dllfile);
                        if (File.Exists(dllfile) && (new System.IO.FileInfo(dllfile).Length) < 1)
                            File.Delete(dllfile); // Delete file if exists with 0 length
                    }
                    catch (Exception)
                    {
                        // todo
                    }
                    tries++;
                }
                if (!File.Exists(dllfile))
                {
                    MessageBox.Show("Failed to download GWToolbox++ dll.\n Check your internet connection and try again",
                        "GWToolbox++ Error",
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Error);
                    return;
                }
            }
            // check again after download/update/build
            if (!File.Exists(dllfile)) {
                MessageBox.Show("Cannot find " + DLL_NAME, "GWToolbox++ Launcher Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            // Look for gw processes.
            List<Process> processesToInject = new List<Process>();

			CharSelector chargui = new CharSelector();
			Process[] gwprocs = chargui.GetValidProcesses();
            switch(gwprocs.Length) {
                case 0: // No gw processes found.
                    if(!isElevated && hasElevatedGWProcesses())
                    {
                        MessageBox.Show("Guild Wars is running as Admin.\n" +
                            "Restart Guild Wars without Admin, or run this launcher as Admin to run GWToolbox++",
                            "GWToolbox++ Error",
                            MessageBoxButtons.OK,
                            MessageBoxIcon.Error);
                        return;
                    }
                    MessageBox.Show("No Guild Wars clients found.\n" +
                                    "Please log into Guild Wars first.", 
                                    "GWToolbox++ Error", 
                                    MessageBoxButtons.OK, 
                                    MessageBoxIcon.Error);
                    break;
                case 1: // Only one process found, injecting.
                    processesToInject.Add(gwprocs[0]);
                    break;
                default: // More than one found, make user select client.
                    Application.Run(chargui);
                    processesToInject.AddRange(chargui.SelectedProcesses);
                    break;
            }

            if (processesToInject.Count == 0) return;

            for (int i = 0; i < processesToInject.Count; ++i) {
                IntPtr dll_return;
                GWCAMemory mem = new GWCAMemory(processesToInject[i]);
                GWCAMemory.LOADMODULERESULT result = mem.LoadModule(dllfile, out dll_return);
                if (result == GWCAMemory.LOADMODULERESULT.SUCCESSFUL && dll_return != IntPtr.Zero) {
                    // continue
                } else if (result == GWCAMemory.LOADMODULERESULT.SUCCESSFUL) {
                    MessageBox.Show("Error loading DLL: ExitCode " + dll_return,
                                    "GWToolbox++ Error",
                                    MessageBoxButtons.OK,
                                    MessageBoxIcon.Error);
                } else {
                    MessageBox.Show("Module Load Error.\n" +
                                    LOADMODULE_RESULT_MESSAGES[(uint)result] + "\n",
                                    "GWToolbox++ Error",
                                    MessageBoxButtons.OK,
                                    MessageBoxIcon.Error);
                    return;
                }
            }
        }
    }
    static class ProcessHelper
    {
        [DllImport("advapi32.dll", SetLastError = true)]
        private static extern bool OpenProcessToken(IntPtr ProcessHandle, UInt32 DesiredAccess, out IntPtr TokenHandle);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool CloseHandle(IntPtr hObject);

        private const int STANDARD_RIGHTS_REQUIRED = 0xF0000;
        private const int TOKEN_ASSIGN_PRIMARY = 0x1;
        private const int TOKEN_DUPLICATE = 0x2;
        private const int TOKEN_IMPERSONATE = 0x4;
        private const int TOKEN_QUERY = 0x8;
        private const int TOKEN_QUERY_SOURCE = 0x10;
        private const int TOKEN_ADJUST_GROUPS = 0x40;
        private const int TOKEN_ADJUST_PRIVILEGES = 0x20;
        private const int TOKEN_ADJUST_SESSIONID = 0x100;
        private const int TOKEN_ADJUST_DEFAULT = 0x80;
        private const int TOKEN_ALL_ACCESS = (STANDARD_RIGHTS_REQUIRED | TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_IMPERSONATE | TOKEN_QUERY | TOKEN_QUERY_SOURCE | TOKEN_ADJUST_PRIVILEGES | TOKEN_ADJUST_GROUPS | TOKEN_ADJUST_SESSIONID | TOKEN_ADJUST_DEFAULT);

        public static bool IsProcessOwnerAdmin(Process proc)
        {
            IntPtr ph = IntPtr.Zero;
            try
            {
                OpenProcessToken(proc.Handle, TOKEN_ALL_ACCESS, out ph);
            }
            catch (Exception)
            {
                return true; // Presume true if we can't even access it...
            }
            WindowsIdentity iden = new WindowsIdentity(ph);
            bool result = false;
            foreach (IdentityReference role in iden.Groups)
            {
                if (!role.IsValidTargetType(typeof(SecurityIdentifier)))
                    continue;
                SecurityIdentifier sid = role as SecurityIdentifier;
                if (!sid.IsWellKnown(WellKnownSidType.AccountAdministratorSid) || sid.IsWellKnown(WellKnownSidType.BuiltinAdministratorsSid))
                    continue;
                result = true;
                break;
            }
            CloseHandle(ph);
            return result;
        }
        public static bool IsProcessOwnerAdmin(string processName)
        {
            return IsProcessOwnerAdmin(Process.GetProcessesByName(processName)[0]);
        }
    }
}
