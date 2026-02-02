# C# Cookbook

This document contains recipes for using the SDK in C#.

Following samples are ports of the C++ [API-How-To](https://github.com/realsenseai/librealsense/wiki/API-How-To) as well as some C# specific features & gotchas.

> You can use the [dotnet-script](https://github.com/filipw/dotnet-script) tool to run these snippets, with 2 prerequisites:
>
>  1. you have both the managed `Intel.RealSense.dll` and unmanaged `realsense2` library in your path.
>  2. add this at the top of a snippet to add a reference and import the library:
>
>       ```cs
>       #r "Intel.RealSense.dll"
>       using Intel.RealSense;
>       ```

## Table of Content

* [Get first RealSense device](#get-first-realsense-device)
* [Start Streaming with Default Configuration](#start-streaming-with-default-configuration)
* [Start Streaming Left and Right Infrared Imagers](#start-streaming-left-and-right-infrared-imagers)
* [Wait for Coherent Set of Frames](#wait-for-coherent-set-of-frames)
* [Poll for Frames](#poll-for-frames)
* [Do Processing on a Background-Thread](#do-processing-on-a-background-thread)
* [Get and Apply Depth-to-Color Extrinsics](#get-and-apply-depth-to-color-extrinsics)
* [Get Disparity Baseline](#get-disparity-baseline)
* [Get Video Stream Intrinsics](#get-video-stream-intrinsics)
* [Get Field of View](#get-field-of-view)
* [Get Depth Units](#get-depth-units)
* [Controlling the Laser](#controlling-the-laser)
* [Casting](#casting)
* [Unsafe Direct Access](#unsafe-direct-access)
* [Generics support](#generics-support)
* [LINQ Carefully](#linq-carefully)
* [Recording](#recording)
* [Playback](#playback)

### Get first RealSense device

```cs
Context ctx = new Context();
var list = ctx.QueryDevices(); // Get a snapshot of currently connected devices
if (list.Count == 0)
    throw new Exception("No device detected. Is it plugged in?");
Device dev = list[0];
```

### Start Streaming with Default Configuration

```cs
var pipe = new Pipeline();
pipe.Start();
```

### Start Streaming Left and Right Infrared Imagers

```cs
var cfg = new Config();
cfg.EnableStream(Stream.Infrared, 1);
cfg.EnableStream(Stream.Infrared, 2);
var pipe = new Pipeline();
pipe.Start(cfg);
```

### Wait for Coherent Set of Frames

```cs
var pipe = new Pipeline();
pipe.Start();
FrameSet frames = pipe.WaitForFrames();
Frame frame = frames.FirstOrDefault(Stream.Depth);
```

### Poll for Frames

```cs
var pipe = new Pipeline();
pipe.Start();

const int CAPACITY = 5; // allow max latency of 5 frames
var queue = new FrameQueue(CAPACITY);
Task.Run(() =>
{
    while (true)
    {
        DepthFrame frame;
        if (queue.PollForFrame(out frame))
        {
            using (frame)
            {
                // Do processing on the frame
            }
        }
    }
});

while (true)
{
    using (var frames = pipe.WaitForFrames())
    using (var depth = frames.DepthFrame)
        queue.Enqueue(depth);
}
```

### Get and Apply Depth-to-Color Extrinsics

```cs
// Use your favorite math type here (Numerics.Vector3, UnityEngine.Vector3...)
using Vector = Intel.RealSense.Math.Vector;

var pipe = new Pipeline();
PipelineProfile selection = pipe.Start();
var depth_stream = selection.GetStream(Stream.Depth);
var color_stream = selection.GetStream(Stream.Color);
Extrinsics e = depth_stream.GetExtrinsicsTo(color_stream);
// e.rotation is a column-major rotation matrix
// e.translation is xyz translation in meters

// Transform point extension method.
static Vector Transform(this Extrinsics extrin, Vector from_point)
{
    Vector to_point;
    to_point.x = extrin.rotation[0] * from_point.x + extrin.rotation[3] * from_point.y + extrin.rotation[6] * from_point.z + extrin.translation[0];
    to_point.y = extrin.rotation[1] * from_point.x + extrin.rotation[4] * from_point.y + extrin.rotation[7] * from_point.z + extrin.translation[1];
    to_point.z = extrin.rotation[2] * from_point.x + extrin.rotation[5] * from_point.y + extrin.rotation[8] * from_point.z + extrin.translation[2];
    return to_point;
}

// Apply extrinsics to the origin
var p = e.Transform(new Vector());
Console.WriteLine($"({p.x}, {p.y}, {p.z})");
```

### Get Disparity Baseline

```cs
Config cfg = new Config();
cfg.EnableStream(Stream.Infrared, 1);
cfg.EnableStream(Stream.Infrared, 2);
var pipe = new Pipeline();
PipelineProfile selection = pipe.Start();
var ir1_stream = selection.GetStream(Stream.Infrared, 0);
var ir2_stream = selection.GetStream(Stream.Infrared, 1);
Extrinsics e = ir1_stream.GetExtrinsicsTo(ir2_stream);
var baseline = e.translation[0];
```

### Get Video Stream Intrinsics

```cs
var pipe = new Pipeline();
PipelineProfile selection = pipe.Start();
var depth_stream = selection.GetStream<VideoStreamProfile>(Stream.Depth);
Intrinsics i = depth_stream.GetIntrinsics();
```

### Get Field of View

```cs
var pipe = new Pipeline();
PipelineProfile selection = pipe.Start();
var depth_stream = selection.GetStream<VideoStreamProfile>(Stream.Depth);
Intrinsics i = depth_stream.GetIntrinsics();
float[] fov = i.FOV; // float[2] - horizontal and vertical field of view in degrees
```

### Get Depth Units

```cs
var pipe = new Pipeline();
PipelineProfile selection = pipe.Start();

Sensor sensor = selection.Device.Sensors[0];
float scale = sensor.DepthScale;
```

### Controlling the Laser

```cs
var pipe = new Pipeline();
PipelineProfile selection = pipe.Start();
var selected_device = selection.Device;
var depth_sensor = selected_device.Sensors[0];

if (depth_sensor.Options.Supports(Option.EmitterEnabled))
{
    depth_sensor.Options[Option.EmitterEnabled].Value = 1f; // Enable emitter
    depth_sensor.Options[Option.EmitterEnabled].Value = 0f; // Disable emitter
}
if (depth_sensor.Options.Supports(Option.LaserPower))
{
    var laserPower = depth_sensor.Options[Option.LaserPower];
    laserPower.Value = laserPower.Max; // Set max power
    laserPower.Value = 0f;             // Disable laser
}
```

### Casting

```cs
void Is<T>(T obj, Type type,
        [System.Runtime.CompilerServices.CallerFilePath] string sourceFilePath = "",
        [System.Runtime.CompilerServices.CallerLineNumber] int sourceLineNumber = 0)
        where T : IDisposable
{
    using (obj)
        Console.WriteLine($"{sourceFilePath}:{sourceLineNumber} {typeof(T) == type}");
}

using (var pipe = new Pipeline())
using (var pp = pipe.Start())
using (var fs = pipe.WaitForFrames())
{
    // Non generic methods return the base type, StreamProfile in this case
    Is(pp.GetStream(Stream.Depth), typeof(VideoStreamProfile)); // False

    // As<T> creates a new object, both need to be disposed
    using (var p = pp.GetStream(Stream.Depth))
        Is(p.As<VideoStreamProfile>(), typeof(VideoStreamProfile)); // True

    // Cast<T> is just a shorthand for the above
    Is(pp.GetStream(Stream.Depth).Cast<VideoStreamProfile>(), typeof(VideoStreamProfile)); // True

    // Generic methods are preferable, in this case it creates the correct type upfront
    Is(pp.GetStream<VideoStreamProfile>(Stream.Depth), typeof(VideoStreamProfile)); // True

    // Again, non generic methods return the base type, Frame in this case
    Is(fs.FirstOrDefault(Stream.Depth), typeof(VideoFrame)); // False

    // Be sure to dispose of both the frame and the strongly-typed clone
    using (var f = fs.FirstOrDefault(Stream.Depth))
        Is(f.As<VideoFrame>(), typeof(VideoFrame)); // True

    // Cast<T> is basically a shorthand for the above
    Is(fs.FirstOrDefault(Stream.Depth).Cast<VideoFrame>(), typeof(VideoFrame)); // True

    // FirstOrDefault<T> is same as above
    Is(fs.FirstOrDefault<VideoFrame>(Stream.Depth), typeof(VideoFrame)); // True

    // Non generic properties return the base type, StreamProfile in this case
    Is(fs.Profile, typeof(VideoStreamProfile)); // False

    // As<T> creates a new object, both need to be disposed
    using(var p = fs.Profile)
        Is(p.As<VideoStreamProfile>(), typeof(VideoStreamProfile)); // True

    // Cast<T> is basically a shorthand for the above
    Is(fs.Profile.Cast<VideoStreamProfile>(), typeof(VideoStreamProfile)); // True

    // Generic methods are preferable, in this case it creates the correct type upfront
    Is(fs.GetProfile<VideoStreamProfile>(), typeof(VideoStreamProfile)); // True
}
```

### Unsafe Direct Access

```cs
var pipe = new Pipeline();
pipe.Start();

using (var frames = pipe.WaitForFrames())
using (var depth = frames.DepthFrame)
{
    // unsafe code block to avoid marshaling frame data
    unsafe {
        ushort* depth_data = (ushort*)depth.Data.ToPointer();
        for(int i=0; i<depth.Width*depth.Height; ++i) {
            ushort d = depth_data[i];
        }
    }
}
```

### Generics support

```cs
var pipe = new Pipeline();
var pc = new PointCloud();

pipe.Start();

using (var frames = pipe.WaitForFrames())
using (var depth = frames.DepthFrame)
using (var pc_frame = pc.Process(depth))
using (var points = pc_frame.As<Points>())
{
    // CopyVertices is extensible, any of these will do:
    var vertices = new float[points.Count * 3];
    // var vertices = new Intel.RealSense.Math.Vertex[points.Count];
    // var vertices = new UnityEngine.Vector3[points.Count];
    // var vertices = new System.Numerics.Vector3[points.Count]; // SIMD
    // var vertices = new GlmSharp.vec3[points.Count];
    //  var vertices = new byte[points.Count * 3 * sizeof(float)];
    points.CopyVertices(vertices);
}
```

### LINQ Carefully

```cs
var pipe = new Pipeline();
pipe.Start(frame =>
{
    using (var fs = frame.As<FrameSet>())
    {
        // DON'T: Filtered frames will not be disposed
        // var frames = fs.Where(f => f.Is(Extension.DepthFrame)).ToList();

        // DO: All frames in the collection will be disposed along with the frameset
        var frames = fs.Where(f => f.DisposeWith(fs).Is(Extension.DepthFrame)).ToList();

        frames.ForEach(f =>
        {
            using (var p = f.Profile)
            using (var vf = f.As<VideoFrame>())
                Console.WriteLine($"#{f.Number} {p.Stream} {p.Format} {vf.Width}x{vf.Height}@{p.Framerate}");
        });
    }
});

await Task.Delay(4000);
```

### Recording

```cs
// Extension method for a fluent API
static Config RecordToFile(this Config cfg, string file) { cfg.EnableRecordToFile(file); return cfg; }

int i;

// Record to file using pipeline, the returned device is a recorder
using (var pipe = new Pipeline())
using (var cfg = new Config().RecordToFile($"ros{++i}.bag"))
using (var pp = pipe.Start(cfg))
using (var dev = pp.Device)
using (var recorder = RecordDevice.FromDevice(dev))
{
    Task.Delay(1000).Wait();
    Console.WriteLine(recorder.FileName);
}

class WaitAndDispose : IDisposable
{
    public WaitAndDispose(int millisecodsDelay, RecordDevice dev) => Task.Delay(millisecodsDelay)
        .ContinueWith(_ => Console.WriteLine(dev.FileName))
        .ContinueWith(_ => dev.Dispose())
        .Wait();
    public void Dispose() { }
}

// Record to files twice, from same device, manually creating recorders
using (var pipe = new Pipeline())
using (var pp = pipe.Start())
using (var dev = pp.Device)
using (new WaitAndDispose(1000, new RecordDevice(dev, $"ros{++i}.bag")))
using (new WaitAndDispose(1000, new RecordDevice(dev, $"ros{++i}.bag")))
{ }
```

### Playback

```cs
// Extension method for a fluent API
static Config FromFile(this Config cfg, string file) { cfg.EnableDeviceFromFile(file, repeat: false); return cfg; }
// Extension method to convert playback position (nanoseconds) to TimeSpan
static TimeSpan ToTimeSpan(this ulong ns) => TimeSpan.FromMilliseconds(ns * 1e-6);

// Playback from file using pipeline
using (var pipe = new Pipeline())
using (var cfg = new Config().FromFile(@"ros1.bag"))
using (var pp = pipe.Start(cfg))
using (var dev = pp.Device)
using (var playback = PlaybackDevice.FromDevice(dev))
{
    Console.WriteLine(playback.FileName);
    playback.Realtime = false;
    var t = Task.Run(async () =>
    {
        var start = DateTime.Now;
        void Print() => Console.WriteLine($"Real: {DateTime.Now - start} Status: {playback.Status,-7} Playback: {playback.Position.ToTimeSpan()}/{playback.Duration.ToTimeSpan()}");
        while (playback.Status != PlaybackStatus.Stopped)
        {
            Print();
            if (pipe.PollForFrames(out FrameSet frames))
                using (frames)
                {
                    foreach (var f in frames)
                        using (f)
                        using (var p = f.Profile)
                            Console.WriteLine($"    {p.Stream} {p.Format,4} #{f.Number} {f.TimestampDomain} {f.Timestamp:F2}");
                }
            await Task.Delay(50);
        }
        Print();
    });
    await Task.Delay(500);
    playback.Pause();
    await Task.Delay(2000);
    playback.Resume();
    await t;
}

// Manually creating a playback device, configure it's first sensor and stream
using (var ctx = new Context())
using (var playback = ctx.AddDevice(@"ros1.bag"))
using (var sensor = playback.Sensors[0])
{
    Console.WriteLine(playback.FileName);
    playback.Realtime = false;

    var start = DateTime.Now;
    void Print() => Console.WriteLine($"Real: {DateTime.Now - start} Status: {playback.Status,-7} Playback: {playback.Position.ToTimeSpan()}/{playback.Duration.ToTimeSpan()}");

    sensor.Open(sensor.StreamProfiles.ToArray());
    sensor.Start(f =>
    {
        Print();
        using (var p = f.Profile)
            Console.WriteLine($"    {p.Stream} {p.Format,4} #{f.Number} {f.TimestampDomain} {f.Timestamp:F2}");
        Task.Delay(100).Wait();
    });

    SpinWait.SpinUntil(() => playback.Status == PlaybackStatus.Stopped);
    Print();
}
```
