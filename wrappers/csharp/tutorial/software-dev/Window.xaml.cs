using System;
using System.Collections.Generic;
using System.Diagnostics;
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
        private Pipeline pipeline;
        private Colorizer colorizer;
        private CancellationTokenSource tokenSource = new CancellationTokenSource();

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

                pipeline = new Pipeline();
                colorizer = new Colorizer();

                var depthWidth = 640; 
                var depthHeight = 480;
                var depthFrames = 30;
                var depthFormat = Format.Z16;

                var colorWidth = 640;
                var colorHeight = 480;
                var colorFrames = 30;                
                using (var ctx = new Context())
                {
                    var devices = ctx.QueryDevices();
                    var dev = devices[0];

                    Console.WriteLine("\nUsing device 0, an {0}", dev.Info[CameraInfo.Name]);
                    Console.WriteLine("    Serial number: {0}", dev.Info[CameraInfo.SerialNumber]);
                    Console.WriteLine("    Firmware version: {0}", dev.Info[CameraInfo.FirmwareVersion]);

                    var depthSensor = dev.FirstDepthSensor();
                    var colorSensor = dev.FirstColorSensor();
                   
                    var depthProfiles = depthSensor.StreamProfiles
                                        .Where(p => p.Stream == Stream.Depth)
                                        .OrderBy(p => p.Framerate)
                                        .Select(p => p.As<VideoStreamProfile>());
                    VideoStreamProfile colorProfile = null;
                    
                    // select color profile to have frameset equal or closer to depth frameset to syncer work smooth
                    foreach (var depthProfile in depthProfiles)
                    {
                        depthWidth = depthProfile.Width;
                        depthHeight = depthProfile.Height;
                        depthFrames = depthProfile.Framerate;
                        depthFormat = depthProfile.Format;
                        colorProfile = colorSensor.StreamProfiles
                                            .Where(p => p.Stream == Stream.Color)
                                            .OrderByDescending(p => p.Framerate)
                                            .Select(p => p.As<VideoStreamProfile>())
                                            .FirstOrDefault(p => p.Framerate == depthFrames);
                        if (colorProfile != null)
                        {
                            colorWidth = colorProfile.Width;
                            colorHeight = colorProfile.Height;
                            colorFrames = colorProfile.Framerate;
                            break;
                        }
                    }
                    if (colorProfile == null)
                    {
                        // if no profile with the same framerate found, takes the first
                        colorProfile = colorSensor.StreamProfiles
                                        .Where(p => p.Stream == Stream.Color)
                                        .OrderByDescending(p => p.Framerate)
                                        .Select(p => p.As<VideoStreamProfile>()).FirstOrDefault();
                        if (colorProfile == null)
                        {
                            throw new InvalidOperationException($"Error while finding appropriate depth and color profiles");
                        }
                        colorWidth = colorProfile.Width;
                        colorHeight = colorProfile.Height;
                        colorFrames = colorProfile.Framerate;

                    }

                }

                var cfg = new Config();
                cfg.EnableStream(Stream.Depth, depthWidth, depthHeight, depthFormat, depthFrames);
                cfg.EnableStream(Stream.Color, colorWidth, colorHeight, Format.Rgb8, colorFrames);

                var profile = pipeline.Start(cfg);

                SetupWindow(profile, out updateDepth, out updateColor);

                // Setup the SW device and sensors
                var software_dev = new SoftwareDevice();
                var depth_sensor = software_dev.AddSensor("Depth");
                var depth_profile = depth_sensor.AddVideoStream(new SoftwareVideoStream
                {
                    type = Stream.Depth,
                    index = 0,
                    uid = 100,
                    width = depthWidth,
                    height = depthHeight,
                    fps = depthFrames,
                    bpp = 2,
                    format = depthFormat,
                    intrinsics = profile.GetStream(Stream.Depth).As<VideoStreamProfile>().GetIntrinsics()
                });                
                depth_sensor.AddReadOnlyOption(Option.DepthUnits, 1.0f / 5000);

                var color_sensor = software_dev.AddSensor("Color");
                var color_profile = color_sensor.AddVideoStream(new SoftwareVideoStream
                {
                    type = Stream.Color,
                    index = 0,
                    uid = 101,
                    width = colorWidth,
                    height = colorHeight,
                    fps = colorFrames,
                    bpp = 3,
                    format = Format.Rgb8, 
                    intrinsics = profile.GetStream(Stream.Color).As<VideoStreamProfile>().GetIntrinsics()
                });

                // Note about the Syncer: If actual FPS is significantly different from reported FPS in AddVideoStream
                // this can confuse the syncer and prevent it from producing synchronized pairs
                software_dev.SetMatcher(Matchers.Default);

                var sync = new Syncer();

                // The raw depth->metric units translation scale is required for Colorizer to work
                var realDepthSensor = profile.Device.FirstDepthSensor();
                depth_sensor.AddReadOnlyOption(Option.DepthUnits, realDepthSensor.DepthScale);

                depth_sensor.Open(depth_profile);
                color_sensor.Open(color_profile);

                // Push the SW device frames to the syncer
                depth_sensor.Start(sync.SubmitFrame);
                color_sensor.Start(sync.SubmitFrame);

                var token = tokenSource.Token;

                ushort[] depthData = null;
                byte[] colorData = null;

                var t = Task.Factory.StartNew(() =>
                {
                    while (!token.IsCancellationRequested)
                    {
                        // We use the frames that are captured from live camera as the input data for the SW device
                        using (var frames = pipeline.WaitForFrames())
                        {
                            var depthFrame = frames.DepthFrame.DisposeWith(frames);
                            var colorFrame = frames.ColorFrame.DisposeWith(frames);

                            depthData = depthData ?? new ushort[depthFrame.Width * depthFrame.Height];
                            depthFrame.CopyTo(depthData);
                            // Construct SW depth frame for SW depth sensor and initialize Depth Unit
                            depth_sensor.AddVideoFrame(depthData, depthFrame.Stride, depthFrame.BitsPerPixel / 8, depthFrame.Timestamp,
                                depthFrame.TimestampDomain, (int)depthFrame.Number, depth_profile, realDepthSensor.DepthScale);

                            colorData = colorData ?? new byte[colorFrame.Stride * colorFrame.Height];
                            colorFrame.CopyTo(colorData);
                            // Construct SW color frame for SW color sensor
                            color_sensor.AddVideoFrame(colorData, colorFrame.Stride, colorFrame.BitsPerPixel / 8, colorFrame.Timestamp,
                                colorFrame.TimestampDomain, (int)colorFrame.Number, color_profile);
                        }

                        // Dispaly the frames that come from the SW device after synchronization
                        using (var new_frames = sync.WaitForFrames())
                        {
                            if (new_frames.Count == 2)
                            {
                                var colorFrame = new_frames.ColorFrame.DisposeWith(new_frames);
                                var depthFrame = new_frames.DepthFrame.DisposeWith(new_frames);

                                var colorizedDepth = colorizer.Process<VideoFrame>(depthFrame).DisposeWith(new_frames);                                
                                // Render the frames.
                                Dispatcher.Invoke(DispatcherPriority.Render, updateDepth, colorizedDepth);
                                Dispatcher.Invoke(DispatcherPriority.Render, updateColor, colorFrame);
                            }
                        }
                    }
                }, token);
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
            using (var p = pipelineProfile.GetStream(Stream.Depth).As<VideoStreamProfile>())
                imgDepth.Source = new WriteableBitmap(p.Width, p.Height, 96d, 96d, PixelFormats.Rgb24, null);
            depth = UpdateImage(imgDepth);

            using (var p = pipelineProfile.GetStream(Stream.Color).As<VideoStreamProfile>())
                imgColor.Source = new WriteableBitmap(p.Width, p.Height, 96d, 96d, PixelFormats.Rgb24, null);
            color = UpdateImage(imgColor);
        }
    }
}
