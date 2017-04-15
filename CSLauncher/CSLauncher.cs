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

namespace CSLauncher {
    static class CSLauncher {
        const string DLL_DIRECTORY = "\\GWToolboxpp\\GWToolbox.dll";

        static readonly string[] LOADMODULE_RESULT_MESSAGES = {
            "GWToolbox.dll successfully loaded.",
            "GWToolbox.dll not found.",
            "kernel32.dll not found.\nHow the fuck did you do this.",
            "LoadLibraryW not found in kernel32.dll... what",
            "VirtualAllocEx allocation unsuccessful.",
            "WriteProcessMemory not able to write path.",
            "Remote thread not spawned.",
            "Remote thread did not finish dll initialization.",
            "VirtualFreeEx deallocation unsuccessful."
        };


        static Process proctoinject = null;
 
        [STAThread]
        static void Main(string[] args) {
            Application.EnableVisualStyles();

            // Check for admin rights.
            WindowsIdentity identity = WindowsIdentity.GetCurrent();
            WindowsPrincipal principal = new WindowsPrincipal(identity);
            bool isElevated = principal.IsInRole(WindowsBuiltInRole.Administrator);

            if(!isElevated) {
                MessageBox.Show("Please run the launcher as Admin.",
                                   "GWToolbox++ Error",
                                   MessageBoxButtons.OK,
                                   MessageBoxIcon.Error);
                return;
            }

            // names and paths
            string localappdata = Environment.GetEnvironmentVariable("LocalAppData");
            string settingsfolder = localappdata + "\\GWToolboxpp\\";
            string inifile = settingsfolder + "GWToolbox.ini";

            // Install resources
            ResInstaller installer = new ResInstaller();
            installer.Install();

#if DEBUG
            // do nothing, we'll use GWToolbox.dll in /Debug
            string dllfile = "GWToolbox.dll"; // same folder where the launcher is built
#else
            // Download or update if needed
            string dllfile = settingsfolder + "GWToolbox.dll";
            if (!File.Exists(dllfile)) {
                string toolboxdir = Environment.GetEnvironmentVariable("LocalAppData") + "\\GWToolboxpp\\";
                WebClient host = new WebClient();
                string remoteversion = host.DownloadString(
                    "https://raw.githubusercontent.com/HasKha/GWToolboxpp/master/resources/toolboxversion.txt");
                string dllurl = "https://github.com/HasKha/GWToolboxpp/releases/download/" 
                    + remoteversion + "_Release/GWToolbox.dll";
                host.DownloadFile(dllurl, toolboxdir + "GWToolbox.dll");
            }
#endif
            // check again after download/update/build
            if (!File.Exists(dllfile)) {
                MessageBox.Show("Cannot find GWToolbox.dll", "GWToolbox++ Launcher Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            // Look for gw processes.
            Process[] gwprocs = Process.GetProcessesByName("Gw");

            switch(gwprocs.Length) {
                case 0: // No gw processes found.
                    MessageBox.Show("No Guild Wars clients found.\n" +
                                    "Please log into Guild Wars first.", 
                                    "GWToolbox++ Error", 
                                    MessageBoxButtons.OK, 
                                    MessageBoxIcon.Error);
                    break;
                case 1: // Only one process found, injecting.
                    proctoinject = gwprocs[0];
                    break;
                default: // More than one found, make user select client.

                    CharSelector chargui = new CharSelector();

                    Application.Run(chargui);
                    
                    proctoinject = chargui.SelectedProcess;
                    break;
            }

            if (proctoinject == null) return;

            IntPtr dll_return;
            GWCAMemory mem = new GWCAMemory(proctoinject);
            GWCAMemory.LOADMODULERESULT result = mem.LoadModule(dllfile,out dll_return);
            if (result == GWCAMemory.LOADMODULERESULT.SUCCESSFUL && dll_return != IntPtr.Zero) return;
            if (result == GWCAMemory.LOADMODULERESULT.SUCCESSFUL){
                MessageBox.Show("Error loading DLL: ExitCode " + dll_return,
                                "GWToolbox++ Error",
                                MessageBoxButtons.OK,
                                MessageBoxIcon.Error);
            }
            else {
                MessageBox.Show("Module Load Error.\n" +
                                LOADMODULE_RESULT_MESSAGES[(uint)result] + "\n",
                                "GWToolbox++ Error",
                                MessageBoxButtons.OK,
                                MessageBoxIcon.Error);
            }
        }
    }
}
