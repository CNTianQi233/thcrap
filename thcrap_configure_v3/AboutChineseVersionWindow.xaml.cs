using System.Diagnostics;
using System.Windows;
using System.Windows.Navigation;

namespace thcrap_configure_v3
{
    public partial class AboutChineseVersionWindow : Window
    {
        public AboutChineseVersionWindow()
        {
            InitializeComponent();
        }

        private void Hyperlink_RequestNavigate(object sender, RequestNavigateEventArgs e)
        {
            Process.Start(new ProcessStartInfo(e.Uri.AbsoluteUri)
            {
                UseShellExecute = true
            });
            e.Handled = true;
        }

        private void CloseButton_Click(object sender, RoutedEventArgs e)
        {
            Close();
        }
    }
}
