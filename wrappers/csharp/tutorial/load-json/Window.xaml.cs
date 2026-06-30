using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Windows.Threading;

namespace Intel.RealSense
{
    /// <summary>
    /// Interaction logic for Window.xaml
    /// </summary>
    public partial class CaptureWindow : Window
    {
        private Pipeline  pipeline;
        private Colorizer colorizer;
        private CancellationTokenSource tokenSource = new CancellationTokenSource();
        private ADP_TestLoadSettingsJson testLoadSettingsJson = new ADP_TestLoadSettingsJson();

        static Action<VideoFrame> UpdateImage(Image img)
        {
            var wbmp = img.Source as WriteableBitmap;
            return new Action<VideoFrame>(frame =>
            {
                var rect = new Int32Rect(0, 0, frame.Width, frame.Height);
                wbmp.WritePixels(rect, frame.Data, frame.Stride * frame.Height, frame.Stride);
            });
        }

        public CaptureWindow()
        {
            InitializeComponent();

            try
            {
                Action<VideoFrame> updateDepth;
                Action<VideoFrame> updateColor;

                // The colorizer processing block will be used to visualize the depth frames.
                colorizer = new Colorizer();

                // Create and config the pipeline to strem color and depth frames.
                pipeline = new Pipeline();

                using (var ctx = new Context())
                {

                    var devices = ctx.QueryDevices();
                    var dev = devices[0];

                    Console.WriteLine("\nUsing device 0, an {0}", dev.Info[CameraInfo.Name]);
                    Console.WriteLine("    Serial number: {0}", dev.Info[CameraInfo.SerialNumber]);
                    Console.WriteLine("    Firmware version: {0}", dev.Info[CameraInfo.FirmwareVersion]);

                    var depthSensor = dev.FirstDepthSensor();
                    var colorSensor = dev.FirstColorSensor();

                    var depthProfile = depthSensor.StreamProfiles
                                        .Where(p => p.Stream == Stream.Depth)
                                        .OrderBy(p => p.Framerate)
                                        .Select(p => p.As<VideoStreamProfile>()).First();

                    var colorProfile = colorSensor.StreamProfiles
                                        .Where(p => p.Stream == Stream.Color)
                                        .OrderBy(p => p.Framerate)
                                        .Select(p => p.As<VideoStreamProfile>()).First();

                    if (!testLoadSettingsJson.LoadSettingsJson(dev))
                    {
                        return;
                    }

                    var cfg = new Config();
                    cfg.EnableDevice(dev.Info.GetInfo(CameraInfo.SerialNumber));
                    cfg.EnableStream(Stream.Depth, depthProfile.Width, depthProfile.Height, depthProfile.Format, depthProfile.Framerate);
                    cfg.EnableStream(Stream.Color, colorProfile.Width, colorProfile.Height, colorProfile.Format, colorProfile.Framerate);

                    var pp = pipeline.Start(cfg);                    
                    SetupWindow(pp, out updateDepth, out updateColor);

                    // more device info
                    Console.WriteLine($"--------------------------");
                    foreach (var item in pp.Device.Info.ToArray())
                    {
                        Console.WriteLine($"{item.Key} - {item.Value}");
                    }                    
                    Console.WriteLine($"--------------------------");
                }

                Task.Factory.StartNew(() =>
                {
                    while (!tokenSource.Token.IsCancellationRequested)
                    {
                    // We wait for the next available FrameSet and using it as a releaser object that would track
                    // all newly allocated .NET frames, and ensure deterministic finalization
                    // at the end of scope. 
                    using (var frames = pipeline.WaitForFrames())
                        {
                            var colorFrame = frames.ColorFrame.DisposeWith(frames);
                            var depthFrame = frames.DepthFrame.DisposeWith(frames);

                        // We colorize the depth frame for visualization purposes
                        var colorizedDepth = colorizer.Process<VideoFrame>(depthFrame).DisposeWith(frames);

                        // Render the frames.
                        Dispatcher.Invoke(DispatcherPriority.Render, updateDepth, colorizedDepth);
                            Dispatcher.Invoke(DispatcherPriority.Render, updateColor, colorFrame);

                            Dispatcher.Invoke(new Action(() =>
                            {
                                String depth_dev_sn = depthFrame.Sensor.Info[CameraInfo.SerialNumber];
                                txtTimeStamp.Text = depth_dev_sn + " : " + String.Format("{0,-20:0.00}", depthFrame.Timestamp) + "(" + depthFrame.TimestampDomain.ToString() + ")";
                            }));
                        }
                    }
                }, tokenSource.Token);
                
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message);
                Application.Current.Shutdown();
            }
        }

        private void control_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            tokenSource.Cancel();
        }

        private void SetupWindow(PipelineProfile pipelineProfile, out Action<VideoFrame> depth, out Action<VideoFrame> color)
        {
            try
            {
                using (var p = pipelineProfile.GetStream(Stream.Depth).As<VideoStreamProfile>())
                    imgDepth.Source = new WriteableBitmap(p.Width, p.Height, 96d, 96d, PixelFormats.Rgb24, null);
            }
            catch (Exception ex)
            {
                MessageBox.Show("Unable to find Deapth Stream. " + ex.Message);
            }
            depth = UpdateImage(imgDepth);
            try
            {
                using (var p = pipelineProfile.GetStream(Stream.Color).As<VideoStreamProfile>())
                    imgColor.Source = new WriteableBitmap(p.Width, p.Height, 96d, 96d, PixelFormats.Rgb24, null);                
            }
            catch (Exception ex)
            {
                MessageBox.Show("Unable to find Color Stream. " + ex.Message);
            }
            color = UpdateImage(imgColor);
        }
    }
}
