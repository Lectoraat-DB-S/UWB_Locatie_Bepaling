using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;

namespace UWBLocationMonitor
{
    public class NetworkConnection
    {
        private UdpClient udpClient;
        private int listenPort;
        private bool isListening;
        private IPAddress allowedSenderIPAddress;
        private Dictionary<string, (int X, int Y, DateTime Timestamp)> lastKnownCoordinates = new Dictionary<string, (int X, int Y, DateTime Timestamp)>();

        // Connect to given address
        public NetworkConnection(string allowedIP, int port)
        {
            listenPort = port;
            allowedSenderIPAddress = IPAddress.Parse(allowedIP);
            udpClient = new UdpClient(listenPort);
        }

        // Start listening for messages received from the connection
        public void StartListening()
        {
            isListening = true;
            Task.Run(() => ListenForMessages());
            LogManager.Log("Started connection");
            LogManager.Log("Time;Tag;Anchor1;Dist(cm);Anchor2;Dist(cm);Anchor3;Dist(cm);X;Y");
        }

        private void ListenForMessages()
        {
            try
            {
                while (isListening)
                {
                    IPEndPoint remoteEndPoint = new IPEndPoint(IPAddress.Any, 0);
                    byte[] receivedBytes = udpClient.Receive(ref remoteEndPoint); // Receive data
                    string receivedData = Encoding.UTF8.GetString(receivedBytes); // Parse data to string

                    // Parse data to function
                    OnMessageReceived(receivedData);
                }
            }
            catch (Exception ex)
            {
                LogManager.Log($"Exception in listening: {ex.Message}");
            }
            finally
            {
                udpClient.Close();
            }
        }

        // Stop listening (Not used)
        public void StopListening()
        {
            isListening = false;
            udpClient.Close();
        }

        // Handle message
        protected virtual void OnMessageReceived(string message)
        {
            int maxDeltaDistance = 25; // Max difference in locations
            TimeSpan maxDeltaTime = TimeSpan.FromSeconds(1); // Max time difference for allowing updates

            // Message format:
            // TagID;AnchorID1;DistanceAnchor1;AnchorID2;DistanceAnchor2;AnchorID3;DistanceAnchor3
            // Example:
            // B0:A7:32:AB:19:94;b0a732ab;0.85;34987a74;1.41;34987a72;0.24
            try
            {
                // Split message in parts
                string[] parts = message.Split(";");

                // Check for error message
                if (parts.Length < 2)
                {
                    throw new ArgumentException(message);
                }

                // Check for full message
                if (parts.Length < 7)
                {
                    throw new ArgumentException("Message format is incorrect");
                }

                string tag = parts[0];

                int[] coordinatesAnchor1 = GetAnchorCoordinates(parts[1]);
                int R1 = int.Parse(parts[2]);

                int[] coordinatesAnchor2 = GetAnchorCoordinates(parts[3]);
                int R2 = int.Parse(parts[4]);

                int[] coordinatesAnchor3 = GetAnchorCoordinates(parts[5]);
                int R3 = int.Parse(parts[6]);

                // Calculate tag position to get the new coordinates without updating the map
                var tagResult = LocationService.CalculateTagPosWithoutUpdate(
                    coordinatesAnchor1[0], coordinatesAnchor1[1], R1,
                    coordinatesAnchor2[0], coordinatesAnchor2[1], R2,
                    coordinatesAnchor3[0], coordinatesAnchor3[1], R3, tag);

                // Extract the calculated coordinates
                int newX = tagResult.Item2;
                int newY = tagResult.Item3;
                DateTime newTimestamp = DateTime.Now;

                // Check if the tag has previous coordinates
                if (lastKnownCoordinates.TryGetValue(tag, out var lastData))
                {
                    // Calculate differences
                    int dX = Math.Abs(newX - lastData.X);
                    int dY = Math.Abs(newY - lastData.Y);
                    TimeSpan deltaTime = newTimestamp - lastData.Timestamp;

                    // If the difference is too big and the time difference is too small, ignore update
                    if (dX > maxDeltaDistance || dY > maxDeltaDistance)
                    {
                        if (deltaTime <= maxDeltaTime)
                        {
                            LogManager.Log($"Ignored update for tag {tag} due to large movement: {dX} in X, {dY} in Y");
                            return;
                        }
                    }
                }

                // Update the last known coordinates and timestamp
                lastKnownCoordinates[tag] = (newX, newY, newTimestamp);

                // Now call CalculateTagPos to update the map
                LocationService.CalculateTagPos(
                    coordinatesAnchor1[0], coordinatesAnchor1[1], R1,
                    coordinatesAnchor2[0], coordinatesAnchor2[1], R2,
                    coordinatesAnchor3[0], coordinatesAnchor3[1], R3, tag);

                // Convert coordinates to string
                string tagCoordinates = $"{tagResult.Item2};{tagResult.Item3}";

                // Add time to message
                DateTime currentTime = DateTime.Now;
                string timeStampedMessage = currentTime.ToString("HH:mm:ss") + ";" + message;
                // Log message with time and coordinates
                LogManager.Log(timeStampedMessage + ";" + tagCoordinates);

            }
            catch (Exception ex)
            {
                LogManager.Log($"Error message: {ex.Message}");
            }
        }

        // Hard coded anchor positions
        private int[] GetAnchorCoordinates(string anchorID)
        {
            switch (anchorID)
            {
                case "34987a74":
                    return new int[] { 420, 1650 };
                case "34987a72":
                    return new int[] { 0, 0 };
                default:
                    return new int[] { 420, 0 };
            }
        }
    }
}
