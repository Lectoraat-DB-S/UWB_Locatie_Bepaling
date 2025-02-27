namespace UWBLocationMonitor
{
    partial class LogPanel
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
            this.printLogButton = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // printLogButton
            // 
            this.printLogButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.printLogButton.Location = new System.Drawing.Point(668, 409);
            this.printLogButton.Name = "printLogButton";
            this.printLogButton.Size = new System.Drawing.Size(94, 29);
            this.printLogButton.TabIndex = 0;
            this.printLogButton.Text = "Print";
            this.printLogButton.UseVisualStyleBackColor = true;
            this.printLogButton.Click += new System.EventHandler(this.printLogButton_Click);
            // 
            // LogPanel
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(8F, 20F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(800, 450);
            this.Controls.Add(this.printLogButton);
            this.Name = "LogPanel";
            this.Text = "LogPanel";
            this.ResumeLayout(false);

        }

        #endregion

        private Button printLogButton;
    }
}