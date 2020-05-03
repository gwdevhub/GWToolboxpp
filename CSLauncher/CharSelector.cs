using System;
using System.Collections.Generic;
using System.Drawing;
using System.Windows.Forms;
using System.Diagnostics;
using GWCA.Memory;
using System.Security.Principal;

namespace CSLauncher
{
    public partial class CharSelector : Form
    {
        private Process[] procs;
        private string[] charnames;
        private bool expanded = false;

        public List<Process> SelectedProcesses { get; private set; }

        public CharSelector()
        {
            InitializeComponent();
            SelectedProcesses = new List<Process>();
            procs = new Process[0];
            charnames = new string[0];
        }
        public Process[] GetValidProcesses()
        {
            Process[] check_procs = Process.GetProcessesByName("Gw");
            Process[] tmp_procs = new Process[check_procs.Length];
            string[] tmp_charnames = new string[check_procs.Length];
            IntPtr charnameAddr = IntPtr.Zero;
            if (check_procs.Length < 1)
                return check_procs;
            int validProcs = 0;
            // Check for admin rights.
            WindowsIdentity identity = WindowsIdentity.GetCurrent();
            WindowsPrincipal principal = new WindowsPrincipal(identity);
            bool isElevated = principal.IsInRole(WindowsBuiltInRole.Administrator);
            procs = new Process[check_procs.Length];
            for (int i = 0; i < check_procs.Length; i++)
            {
                if (!isElevated && ProcessHelper.IsProcessOwnerAdmin(check_procs[i]))
                    continue; // Guild wars has higher privileges
                GWCAMemory mem = new GWCAMemory(check_procs[i]);
                if (mem.HaveModule("GWToolbox.dll"))
                    continue;
                Tuple<IntPtr, int> imagebase = mem.GetImageBase();
                mem.InitScanner(imagebase.Item1, imagebase.Item2);
                charnameAddr = mem.ScanForPtr(new byte[] { 0x8B, 0xF8, 0x6A, 0x03, 0x68, 0x0F, 0x00, 0x00, 0xC0, 0x8B, 0xCF, 0xE8 }, -0x42, true);
                mem.TerminateScanner();
                if (charnameAddr == IntPtr.Zero) continue;
                tmp_procs[validProcs] = check_procs[i];
                tmp_charnames[validProcs] = mem.ReadWString(charnameAddr, 40);
                validProcs++;
            }
            charnames = new string[validProcs];
            procs = new Process[validProcs];
            for (int i = 0; i < validProcs; i++)
            {
                procs[i] = tmp_procs[i];
                charnames[i] = tmp_charnames[i];
            }
            return procs;
        }
        private void CharSelector_Load(object sender, EventArgs e)
        {
            GetValidProcesses();
            for (int i = 0; i < charnames.Length; i++)
            {
                if (charnames[i] == null) continue;
                comboBox.Items.Add(charnames[i]);
                checkedListBox.Items.Add(charnames[i], CheckState.Unchecked);
            }
            comboBox.SelectedIndex = 0;
            if (checkedListBox.Items.Count > 0)
            {
                checkedListBox.SetItemCheckState(0, CheckState.Checked);
            }
        }

        private void buttonLaunch_Click(object sender, EventArgs e)
        {
            SelectedProcesses.Clear();
            if (expanded)
            {
                foreach (int index in checkedListBox.CheckedIndices)
                {
                    SelectedProcesses.Add(procs[index]);
                }
                if (SelectedProcesses.Count == 0)
                {
                    MessageBox.Show("Please select at least one process", "Error: No process selected", MessageBoxButtons.OK);
                    return;
                }

            }
            else
            {
                SelectedProcesses.Add(procs[comboBox.SelectedIndex]);
            }
            this.Close();
        }

        private void buttonExpand_Click(object sender, EventArgs e)
        {
            this.expanded = !this.expanded;
            if (this.expanded)
            {
                this.groupBoxCharSelect.Size = new Size(264, 216);
                this.ClientSize = new System.Drawing.Size(288, 240);
                this.groupBoxCharSelect.Controls.Add(this.buttonCheckAll);
                this.groupBoxCharSelect.Controls.Add(this.buttonUncheckAll);
                this.groupBoxCharSelect.Controls.Add(this.checkedListBox);
                this.groupBoxCharSelect.Controls.Remove(this.comboBox);
            }
            else
            {
                this.groupBoxCharSelect.Size = new Size(264, 54);
                this.ClientSize = new System.Drawing.Size(288, 78);
                this.groupBoxCharSelect.Controls.Remove(this.buttonCheckAll);
                this.groupBoxCharSelect.Controls.Remove(this.buttonUncheckAll);
                this.groupBoxCharSelect.Controls.Remove(this.checkedListBox);
                this.groupBoxCharSelect.Controls.Add(this.comboBox);
            }
        }

        private void buttonCheckAll_Click(object sender, EventArgs e)
        {
            for (int i = 0; i < checkedListBox.Items.Count; ++i)
            {
                checkedListBox.SetItemCheckState(i, CheckState.Checked);
            }
        }

        private void buttonUncheckAll_Click(object sender, EventArgs e)
        {
            for (int i = 0; i < checkedListBox.Items.Count; ++i)
            {
                checkedListBox.SetItemCheckState(i, CheckState.Unchecked);
            }
        }
    }
}
    
