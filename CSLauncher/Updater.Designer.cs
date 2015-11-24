namespace CSLauncher
{
    partial class Updater
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(Updater));
            this.groupBoxUpdate = new System.Windows.Forms.GroupBox();
            this.richTextBoxUpdateNotes = new System.Windows.Forms.RichTextBox();
            this.buttonDownload = new System.Windows.Forms.Button();
            this.groupBoxUpdate.SuspendLayout();
            this.SuspendLayout();
            // 
            // groupBoxUpdate
            // 
            this.groupBoxUpdate.Controls.Add(this.buttonDownload);
            this.groupBoxUpdate.Location = new System.Drawing.Point(12, 194);
            this.groupBoxUpdate.Name = "groupBoxUpdate";
            this.groupBoxUpdate.Size = new System.Drawing.Size(260, 55);
            this.groupBoxUpdate.TabIndex = 0;
            this.groupBoxUpdate.TabStop = false;
            this.groupBoxUpdate.Text = "Update?";
            // 
            // richTextBoxUpdateNotes
            // 
            this.richTextBoxUpdateNotes.Location = new System.Drawing.Point(12, 12);
            this.richTextBoxUpdateNotes.Name = "richTextBoxUpdateNotes";
            this.richTextBoxUpdateNotes.Size = new System.Drawing.Size(260, 176);
            this.richTextBoxUpdateNotes.TabIndex = 1;
            this.richTextBoxUpdateNotes.Text = "";
            // 
            // buttonDownload
            // 
            this.buttonDownload.Location = new System.Drawing.Point(6, 19);
            this.buttonDownload.Name = "buttonDownload";
            this.buttonDownload.Size = new System.Drawing.Size(248, 25);
            this.buttonDownload.TabIndex = 1;
            this.buttonDownload.Text = "Download";
            this.buttonDownload.UseVisualStyleBackColor = true;
            this.buttonDownload.Click += new System.EventHandler(this.buttonDownload_Click);
            // 
            // Updater
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(284, 261);
            this.Controls.Add(this.richTextBoxUpdateNotes);
            this.Controls.Add(this.groupBoxUpdate);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "Updater";
            this.Text = "GWToolbox++ - New Update";
            this.Load += new System.EventHandler(this.Updater_Load);
            this.groupBoxUpdate.ResumeLayout(false);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.GroupBox groupBoxUpdate;
        private System.Windows.Forms.Button buttonDownload;
        private System.Windows.Forms.RichTextBox richTextBoxUpdateNotes;
    }
}