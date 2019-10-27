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
using System.Security.Principal;
using System.Runtime.InteropServices;

namespace CSLauncher
{
    public partial class CharSelector : Form {
        private Process[] procs;
		private string[] charnames;
        private bool expanded = false;

        public List<Process> SelectedProcesses { get; private set; }

        public CharSelector() {
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

            for (int i = 0; i < check_procs.Length; i++)
			{
                if (!isElevated && ProcessHelper.IsProcessOwnerAdmin(check_procs[i]))
                    continue; // Guild wars has higher privileges
				GWCAMemory mem = new GWCAMemory(check_procs[i]);
				if (mem.Read<Int32>(new IntPtr(0x00DE0000)) != 0)
					continue;
				if (mem.HaveModule("GWToolbox.dll"))
					continue;
				if(charnameAddr == IntPtr.Zero) {
					mem.InitScanner(new IntPtr(0x401000), 0x49A000);
					charnameAddr = mem.ScanForPtr(new byte[] { 0x6A, 0x14, 0x8D, 0x96, 0xBC }, 0x9, true);
					mem.TerminateScanner();
				}
				if (charnameAddr == IntPtr.Zero) continue;
				tmp_procs[validProcs] = check_procs[i];
				tmp_charnames[validProcs] = mem.ReadWString(charnameAddr, 60);
				validProcs++;
			}
			charnames = new string[validProcs];
			procs = new Process[validProcs];
			for (int i=0;i < validProcs;i++)
			{
				procs[i] = tmp_procs[i];
				charnames[i] = tmp_charnames[i];
			}
			return procs;
		}
        private void CharSelector_Load(object sender, EventArgs e) {
			GetValidProcesses();
			for (int i = 0; i < charnames.Length; i++)
			{
				if (charnames[i] == null) continue;
                comboBox.Items.Add(charnames[i]);
                checkedListBox.Items.Add(charnames[i], CheckState.Unchecked);
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
