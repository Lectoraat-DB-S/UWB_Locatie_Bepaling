using System;
using System.Collections.Generic;

namespace UWBLocationMonitor
{
    public class LocationMapPanel : Panel
    { 
        private int locationOffsetX = 350; // X offset
        private int locationOffsetY = 100; // Y offset
        private int panelScaling = 2;      // Panel scaling

        public LocationMapPanel()
        {
            this.BackColor = Color.White; // Make background white
            this.BorderStyle = BorderStyle.FixedSingle; // Add border
            this.DoubleBuffered = true; // Prevent flickering

            // Call functien when tags update
            TagManager.Instance.TagsUpdated += HandleTagsUpdated;
        }

        // Rewdraw panel
        private void HandleTagsUpdated()
        {
            this.Invalidate();
        }

        // Override OnPaint function
        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);
            DrawTags(e.Graphics);
        }

        // Go trough tag array and draw tag
        private void DrawTags(Graphics g)
        {
            var tags = TagManager.Instance.GetTags();
            foreach (var tag in tags)
            {
                DrawTag(g, tag);
            }
        }

        // Draw tag
        private void DrawTag(Graphics g, Tag tag)
        {
            using (Brush brush = new SolidBrush(Color.Pink))
            {
                int diameter = 10;
                int radius = diameter / 2;
                int x = tag.tagX - radius;
                int y = tag.tagY - radius;

                // Draw circle of tag at given location
                g.FillEllipse(brush, (x / panelScaling) + locationOffsetX, (y / panelScaling) + locationOffsetY, diameter, diameter);

                // Add text underneath the circle
                using (Font font = new Font("Arial", 8))
                {
                    StringFormat sf = new StringFormat();
                    sf.Alignment = StringAlignment.Center;
                    sf.LineAlignment = StringAlignment.Center;

                    // Position of the text for the tagID
                    g.DrawString(tag.tagID, font, Brushes.Black, (tag.tagX / panelScaling) + locationOffsetX, ((tag.tagY + diameter) / panelScaling) + locationOffsetY , sf) ;
                }
            }
        }
    }
}
