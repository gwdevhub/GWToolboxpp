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
        public bool TBDirExists()
        {
            return Directory.Exists(Environment.GetEnvironmentVariable("LocalAppData") + "\\GWToolboxpp");
        }
        public bool DLLExists()
        {
            return File.Exists(Environment.GetEnvironmentVariable("LocalAppData") + "\\GWToolboxpp\\GWToolbox.dll");
        }

        public void Install()
        {
            string toolboxdir = Environment.GetEnvironmentVariable("LocalAppData") + "\\GWToolboxpp\\";

            string imgdir = toolboxdir + "img\\";

            // Panel Icons
            Properties.Resources.comment.Save(imgdir + "comment.png");
            Properties.Resources.cupcake.Save(imgdir + "cupcake.png");
            Properties.Resources.feather.Save(imgdir + "feather.png");
            Properties.Resources.info.Save(imgdir + "info.png");
            Properties.Resources.list.Save(imgdir + "list.png");
            Properties.Resources.keyboard.Save(imgdir + "keyboard.png");
            Properties.Resources.plane.Save(imgdir + "plane.png");
            Properties.Resources.settings.Save(imgdir + "settings.png");

            // Bond Skills
            Properties.Resources.balthspirit.Save(imgdir + "balthspirit.jpg");
            Properties.Resources.lifebond.Save(imgdir + "lifebond.jpg");
            Properties.Resources.protbond.Save(imgdir + "protbond.jpg");

            // Cons
            Properties.Resources.Armor_of_Salvation.Save(imgdir + "Armor_of_Salvation.png");
            Properties.Resources.Birthday_Cupcake.Save(imgdir + "Birthday_Cupcake.png");
            Properties.Resources.Blue_Rock_Candy.Save(imgdir + "Blue_Rock_Candy.png");
            Properties.Resources.Bottle_of_Grog.Save(imgdir + "Bottle_of_Grog.png");
            Properties.Resources.Bowl_of_Skalefin_Soup.Save(imgdir + "Bowl_of_Skalefin_Soup.png");
            Properties.Resources.Candy_Apple.Save(imgdir + "Candy_Apple.png");
            Properties.Resources.Candy_Corn.Save(imgdir + "Candy_Corn.png");
            Properties.Resources.Drake_Kabob.Save(imgdir + "Drake_Kabob.png");
            Properties.Resources.Dwarven_Ale.Save(imgdir + "Dwarven_Ale.png");
            Properties.Resources.Essence_of_Celerity.Save(imgdir + "Essence_of_Celerity.png");
            Properties.Resources.Fruitcake.Save(imgdir + "Fruitcake.png");
            Properties.Resources.Golden_Egg.Save(imgdir + "Golden_Egg.png");
            Properties.Resources.Grail_of_Might.Save(imgdir + "Grail_of_Might.png");
            Properties.Resources.Green_Rock_Candy.Save(imgdir + "Green_Rock_Candy.png");
            Properties.Resources.Lunar_Fortune.Save(imgdir + "Lunar_Fortune.png");
            Properties.Resources.Pahnai_Salad.Save(imgdir + "Pahnai_Salad.png");
            Properties.Resources.Red_Rock_Candy.Save(imgdir + "Red_Rock_Candy.png");
            Properties.Resources.Slice_of_Pumpkin_Pie.Save(imgdir + "Slice_of_Pumpkin_Pie.png");
            Properties.Resources.Sugary_Blue_Drink.Save(imgdir + "Sugary_Blue_Drink.png");
            Properties.Resources.War_Supplies.Save(imgdir + "War_Supplies.png");

            Properties.Resources.Tick_v2.Save(imgdir + "Tick.png");

            // Config files and fonts
            File.WriteAllBytes(toolboxdir + "Font.ttf",Properties.Resources.Friz_Quadrata_Regular);
            File.WriteAllText(toolboxdir + "GWToolbox.ini", Properties.Resources.DefaultSettings);
            File.WriteAllText(toolboxdir + "Theme.txt", Properties.Resources.DefaultTheme);
        }
    }
}
