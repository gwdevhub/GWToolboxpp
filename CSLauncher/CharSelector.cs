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
    public partial class CharSelector : Form {
        private Process[] procs;
        private bool expanded = false;

        public List<Process> SelectedProcesses { get; private set; }

        public CharSelector() {
            InitializeComponent();
            SelectedProcesses = new List<Process>();
        }

        private void CharSelector_Load(object sender, EventArgs e) {
            Process[] check_procs = Process.GetProcessesByName("Gw");
            procs = new Process[check_procs.Length];

            int validProcs = 0;
            for (int i = 0; i < check_procs.Length; i++)
            {
                GWCAMemory mem = new GWCAMemory(check_procs[i]);
                if (mem.HaveModule("GWToolbox.dll"))
                    continue;
                Tuple<IntPtr, int> imagebase = mem.GetImageBase();
                mem.InitScanner(imagebase.Item1, imagebase.Item2);
                IntPtr charnameAddr = mem.ScanForPtr(new byte[] { 0x8B, 0xF8, 0x6A, 0x03, 0x68, 0x0F, 0x00, 0x00, 0xC0, 0x8B, 0xCF, 0xE8 }, -0x42, true);
                mem.TerminateScanner();
                procs[validProcs] = check_procs[i];
                validProcs++;
                string charname = mem.ReadWString(charnameAddr, 30);
                comboBox.Items.Add(charname);
                checkedListBox.Items.Add(charname, CheckState.Unchecked);
            }
            comboBox.SelectedIndex = 0;
            if (checkedListBox.Items.Count > 0) {
                checkedListBox.SetItemCheckState(0, CheckState.Checked);
            }
        }

        private void buttonLaunch_Click(object sender, EventArgs e) {
            SelectedProcesses.Clear();
            if (expanded) {
                foreach (int index in checkedListBox.CheckedIndices) {
                    SelectedProcesses.Add(procs[index]);
                }
                if (SelectedProcesses.Count == 0) {
                    MessageBox.Show("Please select at least one process", "Error: No process selected", MessageBoxButtons.OK);
                    return;
                }

            } else {
                SelectedProcesses.Add(procs[comboBox.SelectedIndex]);
            }
            this.Close();
        }

        private void buttonExpand_Click(object sender, EventArgs e) {
            this.expanded = !this.expanded;
            if (this.expanded) {
                this.groupBoxCharSelect.Size = new Size(264, 216);
                this.ClientSize = new System.Drawing.Size(288, 240);
                this.groupBoxCharSelect.Controls.Add(this.buttonCheckAll);
                this.groupBoxCharSelect.Controls.Add(this.buttonUncheckAll);
                this.groupBoxCharSelect.Controls.Add(this.checkedListBox);
                this.groupBoxCharSelect.Controls.Remove(this.comboBox);
            } else {
                this.groupBoxCharSelect.Size = new Size(264, 54);
                this.ClientSize = new System.Drawing.Size(288, 78);
                this.groupBoxCharSelect.Controls.Remove(this.buttonCheckAll);
                this.groupBoxCharSelect.Controls.Remove(this.buttonUncheckAll);
                this.groupBoxCharSelect.Controls.Remove(this.checkedListBox);
                this.groupBoxCharSelect.Controls.Add(this.comboBox);
            }
        }

        private void buttonCheckAll_Click(object sender, EventArgs e) {
            for (int i = 0; i < checkedListBox.Items.Count; ++i) {
                checkedListBox.SetItemCheckState(i, CheckState.Checked);
            }
        }

        private void buttonUncheckAll_Click(object sender, EventArgs e) {
            for (int i = 0; i < checkedListBox.Items.Count; ++i) {
                checkedListBox.SetItemCheckState(i, CheckState.Unchecked);
            }
        }
    }
}
