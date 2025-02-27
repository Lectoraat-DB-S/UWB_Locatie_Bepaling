using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace UWBLocationMonitor
{
    public partial class LogPanel : Form
    {
        private RichTextBox richTextBox; // Main log text box
        public LogPanel()
        {
            // Log panel settings
            this.AutoScaleMode = AutoScaleMode.Font;
            this.AutoSize = false;
            this.AutoSizeMode = AutoSizeMode.GrowOnly;
            this.MinimumSize = new Size(800, 600);

            // Initialize panel
            InitializeComponent();
            InitializeLoggingComponents();

            // Add messages to log panel
            PopulateLogs();
            // 
            LogManager.OnLogUpdate += AppendLog;

            this.FormClosing += LogPanel_FormClosing;
        }

        // Hide log panel
        private void LogPanel_FormClosing(object sender, FormClosingEventArgs e)
        {
            e.Cancel = true;
            this.Hide();
        }

        // Initialize the RichTextBox for logs
        private void InitializeLoggingComponents()
        {
            // Settings for the RichTextBox
            richTextBox = new RichTextBox
            {
                Dock = DockStyle.Fill,
                ReadOnly = true,
                BackColor = Color.Black,
                ForeColor = Color.Lime,
                Font = new Font("Consolas", 10),
                ScrollBars = RichTextBoxScrollBars.ForcedVertical
            };
            this.Controls.Add(richTextBox);
            
            // Fix button to the bottom right corner
            printLogButton.Anchor = AnchorStyles.Bottom | AnchorStyles.Right;
        }

        // Print all messages to the log panel
        private void PopulateLogs()
        {
            foreach (var message in LogManager.GetLogMessages())
            {
                richTextBox.AppendText(message + Environment.NewLine);
            }
        }
        
        // Safely add log message to the text box
        public void AppendLog(string message)
        {
            // Check if the function is coming from a different thread
            if (richTextBox.InvokeRequired) 
            {
                // If function gets called from a different thread then reinvoke the message from this thread
                richTextBox.Invoke(new Action<string>(AppendLog), new object[] { message } );
            }
            else
            {
                // If function gets called from this thread then add message
                richTextBox.AppendText(message + Environment.NewLine);
                richTextBox.ScrollToCaret();
            }
        }

        // Detach all events used for the LogPanel (Not used)
        private void DetachEventHandlers()
        {
            LogManager.OnLogUpdate -= AppendLog;
        }
        
        // Dispose panel (Not used)
        private void DisposePanel()
        {
            DetachEventHandlers();
            this.Dispose();
        }

        // Print log button
        private void printLogButton_Click(object sender, EventArgs e)
        {
            LogManager.printLogToCSV();
        }
    }
}
