using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Net;
using System.Threading.Tasks;
using System.Windows.Forms;
using INI;

namespace CSLauncher
{
    public partial class Updater : Form
    {
        const string REMOTE_HOST = "http://fbgmguild.com/GWToolboxpp/";
        WebClient host = null;
        INI_Reader ini = null;

        public Updater()
        {
            InitializeComponent();
            host = new WebClient();
        }

        /// <summary>
        /// Download dll into toolbox directory without going through prompt.
        /// </summary>
        public void DownloadDLL()
        {
            string toolboxdir = Environment.GetEnvironmentVariable("LocalAppData") + "\\GWToolboxpp\\";
            host.DownloadFile(REMOTE_HOST + "GWToolbox.dll", toolboxdir + "GWToolbox.dll");
        }

        public string GetRemoteVersion()
        {
            string remoteversion = host.DownloadString(REMOTE_HOST + "version.txt");
            return remoteversion;
        }

        public void CheckForUpdates()
        {
            ini = new INI_Reader(Environment.GetEnvironmentVariable("LocalAppData") + "\\GWToolboxpp\\GWToolbox.ini");

            string remoteversion = host.DownloadString(REMOTE_HOST + "version.txt");
            string localversion = ini.IniReadValue("launcher","dllversion");

            if (localversion == remoteversion) return;

            Application.Run(this);

            ini.IniWriteValue("launcher","dllversion",remoteversion);

        }

        private void Updater_Load(object sender, EventArgs e)
        {
            string updatenotes = host.DownloadString(REMOTE_HOST + "changelog.txt");

            // To remove the window height from changelog, remove if you decide to drop this idea.
            updatenotes = updatenotes.Substring(updatenotes.IndexOf('\n'));

            richTextBoxUpdateNotes.Text = updatenotes;
        }

        private void buttonDownload_Click(object sender, EventArgs e)
        {
            buttonDownload.Enabled = false;
            buttonDownload.Text = "Downloading...";

            DownloadDLL();

            buttonDownload.Text = "Done! (Close this window to continue)";
        }
    }
}
