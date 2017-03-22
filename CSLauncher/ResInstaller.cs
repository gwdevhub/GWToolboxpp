using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO;

namespace CSLauncher {
    class ResInstaller {
        public void Install() {
            string toolboxdir = Environment.GetEnvironmentVariable("LocalAppData") + "\\GWToolboxpp\\";

            string imgdir = toolboxdir + "img\\";

            EnsureDirectoryExists(toolboxdir);

            // Config font
            EnsureFileExists(Properties.Resources.Font, toolboxdir + "Font.ttf");
        }

        private void EnsureDirectoryExists(string path) {
            if (!Directory.Exists(path)) {
                Directory.CreateDirectory(path);
            }
        }

        private void EnsureFileExists(System.Drawing.Bitmap file, string path) {
            if (!File.Exists(path)) {
                file.Save(path);
            }
        }

        private void EnsureFileExists(byte[] file, string path) {
            if (!File.Exists(path)) {
                File.WriteAllBytes(path, file);
            }
        }

        private void EnsureFileExists(string file, string path) {
            if (!File.Exists(path)) {
                File.WriteAllText(path, file);
            }
        }
    }
}
