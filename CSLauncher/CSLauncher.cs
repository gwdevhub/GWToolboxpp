using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Diagnostics;
using System.Threading;
using System.IO;
using System.Net;
using System.Security.Principal;
using GWCA.Memory;
using System.Security.Cryptography.X509Certificates;
using System.Net.Security;
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
        const string DLL_NAME = "JonsGWToolbox.dll";

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

        [STAThread]
        static void Main(string[] args) {
            Application.EnableVisualStyles();

            // Check for admin rights.
            WindowsIdentity identity = WindowsIdentity.GetCurrent();
            WindowsPrincipal principal = new WindowsPrincipal(identity);
            bool isElevated = principal.IsInRole(WindowsBuiltInRole.Administrator);

            // names and paths
            string toolboxdir = Environment.GetEnvironmentVariable("LocalAppData") + "\\GWToolboxpp\\";
            string inifile = toolboxdir + "GWToolbox.ini";

            // Install resources
            ResInstaller installer = new ResInstaller();
            installer.Install();

#if DEBUG
            // do nothing, we'll use GWToolbox.dll in /Debug
            string dllfile = System.IO.Path.GetDirectoryName(Application.ExecutablePath) + "\\JonsGWToolbox.dll";
#else
            // Download or update if needed
            string dllfile = toolboxdir + DLL_NAME;
            if (File.Exists(dllfile) && (new FileInfo(dllfile).Length) < 1)
                File.Delete(dllfile); // Delete file if exists with 0 length
            if (!File.Exists(dllfile)) {
                ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls12;
                WebClient host = new WebClient();
                host.Headers.Add(HttpRequestHeader.UserAgent, "GWToolboxpp Launcher");
                string remoteversion = "";
                string dllurl = "";
                int tries = 0;

                while(tries < 3 && remoteversion.Length == 0)
                {
                    try
                    {
                        string json = host.DownloadString("https://api.github.com/repos/3vcloud/GWToolboxpp/releases");
                        JavaScriptSerializer serializer = new JavaScriptSerializer();
                        var items = serializer.Deserialize<List<GithubRelease>>(json);
                        foreach(var release in items)
                        {
                            int version_number_len = release.tag_name.IndexOf("_Release");
                            if (version_number_len == -1)
                                continue;
                            foreach(var asset in release.assets)
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
                        if(remoteversion.Length == 0)
                            remoteversion = "0";
                    }
                    catch (Exception e)
                    {
                        Console.WriteLine(e.Message);
                        // todo
                    }
                    tries++;
                }
                if(remoteversion.Length == 0)
                {
                    MessageBox.Show("Failed to fetch current GWToolbox++ version after " + tries +" attempts.\n Check your internet connection and try again",
                        "GWToolbox++ Error",
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Error);
                    return;
                }
                tries = 0;
                // This bit will take a while...
                while (tries < 3 && dllurl.Length > 0 && !File.Exists(dllfile))
                {
                    try
                    {
                        host.DownloadFile(dllurl, dllfile);
                        if (File.Exists(dllfile) && (new System.IO.FileInfo(dllfile).Length) < 1)
                            File.Delete(dllfile); // Delete file if exists with 0 length
                    }
                    catch (Exception e)
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
#endif
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
                    MessageBox.Show("No Guild Wars clients found.\n" +
                                    (!isElevated ? 
                                    "If Guild Wars is currently running as Admin, this launcher also needs Admin permission to run GWToolbox++" 
                                    : "Please log into Guild Wars first."), 
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
}
