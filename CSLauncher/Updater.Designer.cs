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
            this.buttonDownload = new System.Windows.Forms.Button();
            this.richTextBoxUpdateNotes = new System.Windows.Forms.RichTextBox();
            this.SuspendLayout();
            // 
            // buttonDownload
            // 
            this.buttonDownload.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
            this.buttonDownload.Dock = System.Windows.Forms.DockStyle.Bottom;
            this.buttonDownload.Location = new System.Drawing.Point(0, 230);
            this.buttonDownload.Name = "buttonDownload";
            this.buttonDownload.Size = new System.Drawing.Size(351, 31);
            this.buttonDownload.TabIndex = 1;
            this.buttonDownload.Text = "Download";
            this.buttonDownload.UseVisualStyleBackColor = true;
            this.buttonDownload.Click += new System.EventHandler(this.buttonDownload_Click);
            // 
            // richTextBoxUpdateNotes
            // 
            this.richTextBoxUpdateNotes.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.richTextBoxUpdateNotes.Location = new System.Drawing.Point(0, 0);
            this.richTextBoxUpdateNotes.Name = "richTextBoxUpdateNotes";
            this.richTextBoxUpdateNotes.ReadOnly = true;
            this.richTextBoxUpdateNotes.Size = new System.Drawing.Size(351, 230);
            this.richTextBoxUpdateNotes.TabIndex = 1;
            this.richTextBoxUpdateNotes.Text = "";
            // 
            // Updater
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(351, 261);
            this.Controls.Add(this.buttonDownload);
            this.Controls.Add(this.richTextBoxUpdateNotes);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "Updater";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "GWToolbox++ - New Update";
            this.Load += new System.EventHandler(this.Updater_Load);
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Button buttonDownload;
        private System.Windows.Forms.RichTextBox richTextBoxUpdateNotes;
    }
}