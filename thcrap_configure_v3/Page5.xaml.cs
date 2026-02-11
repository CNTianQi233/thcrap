using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using thcrap_cs_lib;
using ShortcutDestinations = thcrap_cs_lib.GlobalConfig.ShortcutDestinations;

namespace thcrap_configure_v3
{
    /// <summary>
    /// Interaction logic for Page5.xaml
    /// </summary>
    public partial class Page5 : UserControl
    {
        bool? isUTLPresent = null;
        GlobalConfig config = null;

        public Page5()
        {
            InitializeComponent();
        }

        public void Enter()
        {
            warningImage.Source = Imaging.CreateBitmapSourceFromHIcon(
                SystemIcons.Warning.Handle, Int32Rect.Empty, BitmapSizeOptions.FromEmptyOptions());

            if (File.Exists("bin\\Universal THCRAP Launcher.exe"))
                warningImage.Visibility = Visibility.Collapsed;

            if (config == null)
            {
                config = GlobalConfig.get();
                ShortcutDestinations dest = config.default_shortcut_destinations;

                checkboxDesktop.IsChecked      = dest.HasFlag(ShortcutDestinations.Desktop);
                checkboxStartMenu.IsChecked    = dest.HasFlag(ShortcutDestinations.StartMenu);
                checkboxGamesFolder.IsChecked  = dest.HasFlag(ShortcutDestinations.GamesFolder);
                checkboxThcrapFolder.IsChecked = dest.HasFlag(ShortcutDestinations.ThcrapFolder);
            }

            if (config.developer_mode)
            {
                shortcutTypePanel.Visibility = Visibility.Visible;
                shortcutType.SelectedIndex = (int)config.shortcuts_type;
            }
        }

        private void checkbox_Checked(object sender, RoutedEventArgs e)
        {
            if (warningPanel == null)
                return;

            if (isUTLPresent == null)
                isUTLPresent = File.Exists("bin\\Universal THCRAP Launcher.exe");

            if (checkboxDesktop.IsChecked == true || checkboxStartMenu.IsChecked == true ||
                checkboxGamesFolder.IsChecked == true || checkboxThcrapFolder.IsChecked == true || isUTLPresent == true)
                warningPanel.Visibility = Visibility.Collapsed;
            else
                warningPanel.Visibility = Visibility.Visible;
        }

        private void NoShortcutsMoreDetails(object sender, MouseButtonEventArgs e)
        {
            MessageBox.Show("按照 thcrap 的工作方式，游戏原文件不会被修改。完成配置后，直接运行游戏原始 exe 仍然是未打补丁状态。\n\n" +
                            "如果你希望像传统补丁那样，在游戏目录中也能直接启动补丁版本，请选择在“游戏目录”创建快捷方式。",
                            "关于使用 thcrap 启动游戏的说明", MessageBoxButton.OK, MessageBoxImage.Information);
        }

        private void CreateShortcuts(string configName, IEnumerable<ThcrapDll.games_js_entry> games, ThcrapDll.ShortcutsDestination destination)
        {
            ThcrapDll.games_js_entry[] gamesArray = new ThcrapDll.games_js_entry[games.Count() + 1];
            int i = 0;
            foreach (var it in games)
            {
                gamesArray[i] = it;
                i++;
            }
            gamesArray[i] = new ThcrapDll.games_js_entry();

            ThcrapDll.CreateShortcuts(configName, gamesArray, destination, config.shortcuts_type);
        }

        public void Leave(string configName, IEnumerable<ThcrapDll.games_js_entry> games)
        {
            config.default_shortcut_destinations =
                (checkboxDesktop.IsChecked      == true ? ShortcutDestinations.Desktop      : 0) |
                (checkboxStartMenu.IsChecked    == true ? ShortcutDestinations.StartMenu    : 0) |
                (checkboxGamesFolder.IsChecked  == true ? ShortcutDestinations.GamesFolder  : 0) |
                (checkboxThcrapFolder.IsChecked == true ? ShortcutDestinations.ThcrapFolder : 0);
            if (config.developer_mode)
            {
                config.shortcuts_type = (ThcrapDll.ShortcutsType)shortcutType.SelectedIndex;
            }
            config.Save();

            if (checkboxDesktop.IsChecked == true)
                CreateShortcuts(configName, games, ThcrapDll.ShortcutsDestination.SHDESTINATION_DESKTOP);
            if (checkboxStartMenu.IsChecked == true)
                CreateShortcuts(configName, games, ThcrapDll.ShortcutsDestination.SHDESTINATION_START_MENU);
            if (checkboxGamesFolder.IsChecked == true)
                CreateShortcuts(configName, games, ThcrapDll.ShortcutsDestination.SHDESTINATION_GAMES_DIRECTORY);
            if (checkboxThcrapFolder.IsChecked == true)
                CreateShortcuts(configName, games, ThcrapDll.ShortcutsDestination.SHDESTINATION_THCRAP_DIR);
        }
    }
}
