using System;
using System.Collections.Generic;
using System.Collections.Concurrent;

namespace UWBLocationMonitor
{
    public static class LogManager
    {
        private static ConcurrentQueue<string> logMessages = new ConcurrentQueue<string>(); // Array for all log messages.
        public static event Action<string> OnLogUpdate; // Event when log updates

        // Adds message to the logPanel and log
        public static void Log(string message)
        {
            logMessages.Enqueue(message);
            OnLogUpdate?.Invoke(message);
        }
        
        // Return log array
        public static IEnumerable<string> GetLogMessages()
        {
            return logMessages.ToArray();
        }

        // Remove all log messages
        public static void ClearLogMessages()
        {
            while (logMessages.TryDequeue(out _)) { }
        }

        // Print log to .csv file
        public static void printLogToCSV()
        {
            var tempPath = Path.GetTempPath(); // Path of log file
            var fileName = "log_" + DateTime.Now.ToString("dd-MM-yyyy") + " " + DateTime.Now.ToString("HH_mm_ss") + ".csv"; // Name of log file
            var path = Path.Combine(tempPath, fileName); // Full path of logfile

            // Write each array element to the given path
            using var sw = new StreamWriter(path);
            foreach (var line in logMessages)
            {
                sw.WriteLine(line);
            }
        }
    }
}
