namespace UWBLocationMonitor
{
    public partial class Form1 : Form
    {
        // Instance definition for log panel
        private LogPanel logPanelInstance;

        public Form1()
        {
            // Initialize main GUI
            InitializeComponent();
            this.AutoScaleMode = AutoScaleMode.Font;
            this.AutoSize = false;
            this.AutoSizeMode = AutoSizeMode.GrowOnly;
            this.MinimumSize = new Size(800, 600);
           
            // Set up Map and Detail panels
            SetupCustomPanels();
           
            // Make main form take control of the TagManager class
            TagManager.Instance.SetUIControl(this);

            // Connect to tag
            NetworkConnection networkConnection = new NetworkConnection("145.44.116.114", 8080);
            networkConnection.StartListening();
            
            // Hard coded anchor positions
            TagManager.Instance.UpdateTag("Anchor1", 420, 1500);
            TagManager.Instance.UpdateTag("Anchor2", 420, 0);
            TagManager.Instance.UpdateTag("Anchor3", 0, 0);
        }

        // Setup function for map and detail panel
        private void SetupCustomPanels()
        {
            // Setup for map panel
            LocationMapPanel locationMapPanel = new LocationMapPanel();
            locationMapPanel.Dock = DockStyle.Fill; // Fill the panel
            this.splitContainer1.Panel1.Controls.Add(locationMapPanel);

            // Setup for detail panel
            LocationDetailsPanel locationDetailsPanel = new LocationDetailsPanel();
            locationDetailsPanel.Dock = DockStyle.Fill; // Fill the panel
            this.splitContainer1.Panel2.Controls.Add(locationDetailsPanel);
        }
        
        // Next functions are not used but are there if needed.
        /*
        private void splitContainer1_Panel1_Paint(object sender, PaintEventArgs e)
        {

        }

        private void splitContainer1_Panel2_Paint(object sender, PaintEventArgs e)
        {

        }

        private void splitContainer1_SplitterMoved(object sender, SplitterEventArgs e)
        {

        }

        private void panel1_Paint(object sender, PaintEventArgs e)
        {

        } 
        */
        
        // Button for log panel
        private void logButton_Click(object sender, EventArgs e)
        {
            // Check if panel is already open/existings
            if (logPanelInstance == null || logPanelInstance.IsDisposed)
            {
                logPanelInstance = new LogPanel();
            }

            logPanelInstance.Show();
            logPanelInstance.BringToFront();
        }
    }
}