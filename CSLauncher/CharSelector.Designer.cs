namespace CSLauncher
{
    partial class CharSelector
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(CharSelector));
			this.groupBoxCharSelect = new System.Windows.Forms.GroupBox();
			this.checkedListBox1 = new System.Windows.Forms.CheckedListBox();
			this.buttonLaunch = new System.Windows.Forms.Button();
			this.buttonCheckAll = new System.Windows.Forms.Button();
			this.buttonUncheckAll = new System.Windows.Forms.Button();
			this.groupBoxCharSelect.SuspendLayout();
			this.SuspendLayout();
			// 
			// groupBoxCharSelect
			// 
			this.groupBoxCharSelect.Controls.Add(this.buttonUncheckAll);
			this.groupBoxCharSelect.Controls.Add(this.buttonCheckAll);
			this.groupBoxCharSelect.Controls.Add(this.checkedListBox1);
			this.groupBoxCharSelect.Controls.Add(this.buttonLaunch);
			this.groupBoxCharSelect.Location = new System.Drawing.Point(12, 12);
			this.groupBoxCharSelect.Name = "groupBoxCharSelect";
			this.groupBoxCharSelect.Size = new System.Drawing.Size(244, 201);
			this.groupBoxCharSelect.TabIndex = 0;
			this.groupBoxCharSelect.TabStop = false;
			this.groupBoxCharSelect.Text = "Select Character...";
			// 
			// checkedListBox1
			// 
			this.checkedListBox1.CheckOnClick = true;
			this.checkedListBox1.FormattingEnabled = true;
			this.checkedListBox1.Location = new System.Drawing.Point(7, 20);
			this.checkedListBox1.Name = "checkedListBox1";
			this.checkedListBox1.Size = new System.Drawing.Size(143, 169);
			this.checkedListBox1.TabIndex = 2;
			// 
			// buttonLaunch
			// 
			this.buttonLaunch.Location = new System.Drawing.Point(156, 18);
			this.buttonLaunch.Name = "buttonLaunch";
			this.buttonLaunch.Size = new System.Drawing.Size(75, 23);
			this.buttonLaunch.TabIndex = 1;
			this.buttonLaunch.Text = "Launch";
			this.buttonLaunch.UseVisualStyleBackColor = true;
			this.buttonLaunch.Click += new System.EventHandler(this.buttonLaunch_Click);
			// 
			// buttonCheckAll
			// 
			this.buttonCheckAll.Location = new System.Drawing.Point(156, 47);
			this.buttonCheckAll.Name = "buttonCheckAll";
			this.buttonCheckAll.Size = new System.Drawing.Size(75, 23);
			this.buttonCheckAll.TabIndex = 3;
			this.buttonCheckAll.Text = "Check All";
			this.buttonCheckAll.UseVisualStyleBackColor = true;
			this.buttonCheckAll.Click += new System.EventHandler(this.buttonCheckAll_Click);
			// 
			// buttonUncheckAll
			// 
			this.buttonUncheckAll.Location = new System.Drawing.Point(156, 76);
			this.buttonUncheckAll.Name = "buttonUncheckAll";
			this.buttonUncheckAll.Size = new System.Drawing.Size(75, 23);
			this.buttonUncheckAll.TabIndex = 4;
			this.buttonUncheckAll.Text = "Uncheck All";
			this.buttonUncheckAll.UseVisualStyleBackColor = true;
			this.buttonUncheckAll.Click += new System.EventHandler(this.buttonUncheckAll_Click);
			// 
			// CharSelector
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(268, 216);
			this.Controls.Add(this.groupBoxCharSelect);
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.Name = "CharSelector";
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
			this.Text = "Toolbox++ Char Selector";
			this.TopMost = true;
			this.Load += new System.EventHandler(this.CharSelector_Load);
			this.groupBoxCharSelect.ResumeLayout(false);
			this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.GroupBox groupBoxCharSelect;
        private System.Windows.Forms.Button buttonLaunch;
		private System.Windows.Forms.CheckedListBox checkedListBox1;
		private System.Windows.Forms.Button buttonUncheckAll;
		private System.Windows.Forms.Button buttonCheckAll;
	}
}