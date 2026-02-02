using System;
using System.Threading;
using UnityEngine;
using Intel.RealSense;
using System.Collections;
using System.Linq;

/// <summary>
/// Manages streaming using a RealSense Device
/// </summary>
[HelpURL("https://github.com/realsenseai/librealsense/tree/master/wrappers/unity")]
public class RsDevice : RsFrameProvider
{
    /// <summary>
    /// The parallelism mode of the module
    /// </summary>
    public enum ProcessMode
    {
        Multithread,
        UnityThread,
    }

    // public static RsDevice Instance { get; private set; }

    /// <summary>
    /// Threading mode of operation, Multithread or UnityThread
    /// </summary>
    [Tooltip("Threading mode of operation, Multithreads or Unitythread")]
    public ProcessMode processMode;

    // public bool Streaming { get; private set; }

    /// <summary>
    /// Notifies upon streaming start
    /// </summary>
    public override event Action<PipelineProfile> OnStart;

    /// <summary>
    /// Notifies when streaming has stopped
    /// </summary>
    public override event Action OnStop;

    /// <summary>
    /// Fired when a new frame is available
    /// </summary>
    public override event Action<Frame> OnNewSample;

    /// <summary>
    /// User configuration
    /// </summary>
    public RsConfiguration DeviceConfiguration = new RsConfiguration
    {
        mode = RsConfiguration.Mode.Live,
        RequestedSerialNumber = string.Empty,
        Profiles = new RsVideoStreamRequest[] {
            new RsVideoStreamRequest {Stream = Stream.Depth, StreamIndex = -1, Width = 640, Height = 480, Format = Format.Z16 , Framerate = 30 },
            new RsVideoStreamRequest {Stream = Stream.Infrared, StreamIndex = -1, Width = 640, Height = 480, Format = Format.Y8 , Framerate = 30 },
            new RsVideoStreamRequest {Stream = Stream.Color, StreamIndex = -1, Width = 640, Height = 480, Format = Format.Rgb8 , Framerate = 30 }
        }
    };

    private Thread worker;
    private readonly AutoResetEvent stopEvent = new AutoResetEvent(false);
    private Pipeline m_pipeline;

    void OnEnable()
    {
        m_pipeline = new Pipeline();

        using (var cfg = DeviceConfiguration.ToPipelineConfig())
            ActiveProfile = m_pipeline.Start(cfg);

        DeviceConfiguration.Profiles = ActiveProfile.Streams.Select(RsVideoStreamRequest.FromProfile).ToArray();

        if (processMode == ProcessMode.Multithread)
        {
            stopEvent.Reset();
            worker = new Thread(WaitForFrames);
            worker.IsBackground = true;
            worker.Start();
        }

        StartCoroutine(WaitAndStart());
    }

    IEnumerator WaitAndStart()
    {
        yield return new WaitForEndOfFrame();
        Streaming = true;
        if (OnStart != null)
            OnStart(ActiveProfile);
    }

    void OnDisable()
    {
        OnNewSample = null;
        // OnNewSampleSet = null;

        if (worker != null)
        {
            stopEvent.Set();
            worker.Join();
        }

        if (Streaming && OnStop != null)
            OnStop();

        if (ActiveProfile != null)
        {
            ActiveProfile.Dispose();
            ActiveProfile = null;
        }

        if (m_pipeline != null)
        {
            // if (Streaming)
            // m_pipeline.Stop();
            m_pipeline.Dispose();
            m_pipeline = null;
        }

        Streaming = false;
    }

    void OnDestroy()
    {
        // OnStart = null;
        OnStop = null;

        if (ActiveProfile != null)
        {
            ActiveProfile.Dispose();
            ActiveProfile = null;
        }

        if (m_pipeline != null)
        {
            m_pipeline.Dispose();
            m_pipeline = null;
        }
    }

    private void RaiseSampleEvent(Frame frame)
    {
        var onNewSample = OnNewSample;
        if (onNewSample != null)
        {
            onNewSample(frame);
        }
    }

    /// <summary>
    /// Worker Thread for multithreaded operations
    /// </summary>
    private void WaitForFrames()
    {
        while (!stopEvent.WaitOne(0))
        {
            using (var frames = m_pipeline.WaitForFrames())
                RaiseSampleEvent(frames);
        }
    }

    void Update()
    {
        if (!Streaming)
            return;

        if (processMode != ProcessMode.UnityThread)
            return;

        FrameSet frames;
        if (m_pipeline.PollForFrames(out frames))
        {
            using (frames)
                RaiseSampleEvent(frames);
        }
    }

}
