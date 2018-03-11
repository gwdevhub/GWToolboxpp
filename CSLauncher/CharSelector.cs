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

        public List<Process> SelectedProcesses { get; private set; }

        public CharSelector()
        {
            InitializeComponent();
			SelectedProcesses = new List<Process>();
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
                if (mem.Read<Int32>(new IntPtr(0x00DE0000)) != 0)
                    continue;
                if (mem.HaveModule("GWToolbox.dll"))
                    continue;
                string charname = mem.ReadWString(charnameAddr,30);
	            checkedListBox1.Items.Add(charname, CheckState.Unchecked);
            }
        }

        private void buttonLaunch_Click(object sender, EventArgs e)
        {
			SelectedProcesses.Clear();

	        foreach (int index in checkedListBox1.CheckedIndices)
	        {
				SelectedProcesses.Add(procs[index]);
	        }

	        if (SelectedProcesses.Count == 0)
	        {
		        MessageBox.Show("Please select at least one process", "Error No Process Selected", MessageBoxButtons.OK);
		        return;
	        }

            this.Close();
        }

		private void buttonCheckAll_Click(object sender, EventArgs e)
		{
			for (int i = 0; i < checkedListBox1.Items.Count; i++)
			{
				checkedListBox1.SetItemCheckState(i, CheckState.Checked);
			}
		}

		private void buttonUncheckAll_Click(object sender, EventArgs e)
		{
			for (int i = 0; i < checkedListBox1.Items.Count; i++)
			{
				checkedListBox1.SetItemCheckState(i, CheckState.Unchecked);
			}
		}
	}
}
