using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;

namespace Intel.RealSense
{
    class Program
    {
        static void Main(string[] args)
        {
            using (var ctx = new Context())
            {
                var devices = ctx.QueryDevices();

                Console.WriteLine("There are {0} connected RealSense devices.", devices.Count);
                if (devices.Count == 0) return;
                var dev = devices[0];

                Console.WriteLine("\nUsing device 0, an {0}", dev.Info[CameraInfo.Name]);
                Console.WriteLine("    Serial number: {0}", dev.Info[CameraInfo.SerialNumber]);
                Console.WriteLine("    Firmware version: {0}", dev.Info[CameraInfo.FirmwareVersion]);

                var depthSensor = dev.FirstDepthSensor();

                var sp = depthSensor.StreamProfiles
                    .Where(p => p.Stream == Stream.Depth)
                    .OrderBy(p => p.Framerate)
                    .Select(p => p.As<VideoStreamProfile>())
                    .FirstOrDefault();

                if (sp ==null)
                {
                    Console.WriteLine("No default profile found for sensor");
                    return;
                }

                depthSensor.Open(sp);


                int one_meter = (int)(1f / depthSensor.DepthScale);
                ushort[] depth = new ushort[sp.Width * sp.Height];
                char[] buffer = new char[(sp.Width/ 10 + 1) * (sp.Height/ 20)];
                int[] coverage = new int[sp.Width / 10]; 

                depthSensor.Start(f =>
                {
                    using (var vf = f.As<VideoFrame>())
                        vf.CopyTo(depth);
                    
                    int b = 0;
                    for (int y = 0; y < sp.Height; ++y)
                    {
                        for (int x = 0; x < sp.Width; ++x)
                        {
                            ushort d = depth[x + y * sp.Width];
                            if (d > 0 && d < one_meter)
                            {
                                var ind = x / 10;
                                ++coverage[ind >= coverage.Length ? coverage.Length-1 : ind];
                            }
                        }

                        if (y % 20 == 19)
                        {
                            for (int i = 0; i < coverage.Length ; i++)
                            {
                                int c = coverage[i];
                                var depthChars = " .:nhBXWW";
                                buffer[b++] = depthChars[c/25 >= depthChars.Length ? depthChars.Length-1: c/25];
                                coverage[i] = 0;
                            }
                            buffer[b++] = '\n';
                        }
                    }

                    Console.SetCursorPosition(0, 0);
                    Console.WriteLine();
                    Console.Write(buffer);
                });

                AutoResetEvent stop = new AutoResetEvent(false);
                Console.CancelKeyPress += (s, e) =>
                {
                    e.Cancel = true;
                    stop.Set();
                };
                stop.WaitOne();

                depthSensor.Stop();
                depthSensor.Close();
            }
        }
    }
}
