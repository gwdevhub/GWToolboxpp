using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO;

namespace CSLauncher
{
    class ResInstaller
    {
        public bool DLLExists()
        {
            return File.Exists(Environment.GetEnvironmentVariable("LocalAppData") + "\\GWToolboxpp\\GWToolbox.dll");
        }

        public void Install()
        {
            string toolboxdir = Environment.GetEnvironmentVariable("LocalAppData") + "\\GWToolboxpp\\";

            string imgdir = toolboxdir + "img\\";

            EnsureDirectoryExists(toolboxdir);
            EnsureDirectoryExists(imgdir);
            EnsureDirectoryExists(toolboxdir + "location logs\\");

            // Panel Icons
            EnsureFileExists(Properties.Resources.comment, imgdir + "comment.png");
            EnsureFileExists(Properties.Resources.cupcake, imgdir + "cupcake.png");
            EnsureFileExists(Properties.Resources.feather, imgdir + "feather.png");
            EnsureFileExists(Properties.Resources.info, imgdir + "info.png");
            EnsureFileExists(Properties.Resources.list, imgdir + "list.png");
            EnsureFileExists(Properties.Resources.keyboard, imgdir + "keyboard.png");
            EnsureFileExists(Properties.Resources.plane, imgdir + "plane.png");
            EnsureFileExists(Properties.Resources.settings, imgdir + "settings.png");

            // Bond Skills
            EnsureFileExists(Properties.Resources.balthspirit, imgdir + "balthspirit.jpg");
            EnsureFileExists(Properties.Resources.lifebond, imgdir + "lifebond.jpg");
            EnsureFileExists(Properties.Resources.protbond, imgdir + "protbond.jpg");

            // Cons
            EnsureFileExists(Properties.Resources.Armor_of_Salvation, imgdir + "Armor_of_Salvation.png");
            EnsureFileExists(Properties.Resources.Birthday_Cupcake, imgdir + "Birthday_Cupcake.png");
            EnsureFileExists(Properties.Resources.Blue_Rock_Candy, imgdir + "Blue_Rock_Candy.png");
            EnsureFileExists(Properties.Resources.Bottle_of_Grog, imgdir + "Bottle_of_Grog.png");
            EnsureFileExists(Properties.Resources.Bowl_of_Skalefin_Soup, imgdir + "Bowl_of_Skalefin_Soup.png");
            EnsureFileExists(Properties.Resources.Candy_Apple, imgdir + "Candy_Apple.png");
            EnsureFileExists(Properties.Resources.Candy_Corn, imgdir + "Candy_Corn.png");
            EnsureFileExists(Properties.Resources.Drake_Kabob, imgdir + "Drake_Kabob.png");
            EnsureFileExists(Properties.Resources.Dwarven_Ale, imgdir + "Dwarven_Ale.png");
            EnsureFileExists(Properties.Resources.Essence_of_Celerity, imgdir + "Essence_of_Celerity.png");
            EnsureFileExists(Properties.Resources.Fruitcake, imgdir + "Fruitcake.png");
            EnsureFileExists(Properties.Resources.Golden_Egg, imgdir + "Golden_Egg.png");
            EnsureFileExists(Properties.Resources.Grail_of_Might, imgdir + "Grail_of_Might.png");
            EnsureFileExists(Properties.Resources.Green_Rock_Candy, imgdir + "Green_Rock_Candy.png");
            EnsureFileExists(Properties.Resources.Lunar_Fortune, imgdir + "Lunar_Fortune.png");
            EnsureFileExists(Properties.Resources.Pahnai_Salad, imgdir + "Pahnai_Salad.png");
            EnsureFileExists(Properties.Resources.Red_Rock_Candy, imgdir + "Red_Rock_Candy.png");
            EnsureFileExists(Properties.Resources.Slice_of_Pumpkin_Pie, imgdir + "Slice_of_Pumpkin_Pie.png");
            EnsureFileExists(Properties.Resources.Sugary_Blue_Drink, imgdir + "Sugary_Blue_Drink.png");
            EnsureFileExists(Properties.Resources.War_Supplies, imgdir + "War_Supplies.png");

            EnsureFileExists(Properties.Resources.Tick_v2, imgdir + "Tick.png");

            // Config files and fonts
            if (!File.Exists(toolboxdir + "Font.ttf"))
                File.WriteAllBytes(toolboxdir + "Font.ttf",Properties.Resources.Friz_Quadrata_Regular);
            if (!File.Exists(toolboxdir + "GWToolbox.ini"))
                File.WriteAllText(toolboxdir + "GWToolbox.ini", Properties.Resources.DefaultSettings);
            if (!File.Exists(toolboxdir + "Theme.txt"))
                File.WriteAllText(toolboxdir + "Theme.txt", Properties.Resources.DefaultTheme);
        }

        private void EnsureDirectoryExists(string path)
        {
            if (!Directory.Exists(path))
            {
                Directory.CreateDirectory(path);
            }
        }

        private void EnsureFileExists(System.Drawing.Bitmap file, string path)
        {
            if (!File.Exists(path))
            {
                file.Save(path);
            }
        }
    }
}
