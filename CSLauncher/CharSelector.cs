using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Diagnostics;
using GWCA.Memory;

namespace CSLauncher
{
    public partial class CharSelector : Form
    {
        private Process[] procs;

        private Process selected_process;
        public Process SelectedProcess
        {
            get { return selected_process; }
        }

        public CharSelector()
        {
            InitializeComponent();
        }

        private void CharSelector_Load(object sender, EventArgs e)
        {
            procs = Process.GetProcessesByName("Gw");
            IntPtr charnameAddr;

            {
                GWCAMemory firstprocmems = new GWCAMemory(procs[0]);
                firstprocmems.InitScanner(new IntPtr(0x401000),0x49A000);
                charnameAddr = firstprocmems.ScanForPtr(new byte[] { 0x6A, 0x14, 0x8D, 0x96, 0xBC },0x9, true);
                firstprocmems.TerminateScanner();
            }

            foreach (Process proc in procs)
            {
                GWCAMemory mem = new GWCAMemory(proc);
                string charname = mem.ReadWString(charnameAddr,30);
                comboBox1.Items.Add(charname);
            }
            comboBox1.SelectedIndex = 0;
        }

        private void buttonLaunch_Click(object sender, EventArgs e)
        {
            selected_process = procs[comboBox1.SelectedIndex];
            this.Close();
        }
    }
}
