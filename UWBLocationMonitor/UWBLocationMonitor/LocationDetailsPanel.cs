using System;
using System.Collections.Generic;


namespace UWBLocationMonitor
{
    public class LocationDetailsPanel : Panel
    {
        private TextBox locationInfo;

        public LocationDetailsPanel()
        {
            this.BackColor = Color.White; // Set background white

            // Creating and settings for main TextBox
            locationInfo = new TextBox();
            locationInfo.Multiline = true;
            locationInfo.ReadOnly = true;
            locationInfo.ScrollBars = ScrollBars.Vertical;
            locationInfo.Font = new Font("Monospaced", 12);

            // Use docking to fill the panel with the text box
            locationInfo.Dock = DockStyle.Fill;
            this.Controls.Add(locationInfo);

            // Set panel border
            this.BorderStyle = BorderStyle.FixedSingle;

            // Subscribe to the TagsUpdated event
            TagManager.Instance.TagsUpdated += RefreshDetails;
        }

        // Rewrite all text in TextBox
        public void RefreshDetails()
        {
            locationInfo.Clear(); // Clear TextBox
            var tags = TagManager.Instance.GetTags(); // Get all tags

            var tagsCopy = tags.ToList();

            // Print all tags in list
            foreach (var tag in tagsCopy)
            {
                string formattedText = String.Format("Tag ID: {0,-10} X: {1,-10} Y: {2,-10}\r\n", tag.tagID, tag.tagX, tag.tagY);
                locationInfo.AppendText(formattedText);
            }
        }
    }
}
