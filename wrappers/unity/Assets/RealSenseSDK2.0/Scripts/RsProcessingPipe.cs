using Intel.RealSense;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using UnityEngine;
using UnityEngine.SceneManagement;
using UnityEngine.UI;

[System.AttributeUsage(System.AttributeTargets.Class, AllowMultiple = false)]
public sealed class ProcessingBlockDataAttribute : System.Attribute
{
    // See the attribute guidelines at
    //  http://go.microsoft.com/fwlink/?LinkId=85236
    public readonly Type blockClass;

    public ProcessingBlockDataAttribute(Type blockClass)
    {
        this.blockClass = blockClass;
    }
}


[Serializable]
public class RsProcessingPipe : RsFrameProvider
{
    public RsFrameProvider Source;
    public RsProcessingProfile profile;
    public override event Action<PipelineProfile> OnStart;
    public override event Action OnStop;
    public override event Action<Frame> OnNewSample;
    private CustomProcessingBlock _block;

    void Awake()
    {
        Source.OnStart += OnSourceStart;
        Source.OnStop += OnSourceStop;

        _block = new CustomProcessingBlock(ProcessFrame);
        _block.Start(OnFrame);
    }

    private void OnSourceStart(PipelineProfile activeProfile)
    {
        Source.OnNewSample += _block.Process;
        ActiveProfile = activeProfile;

        // For the L5** device, disable processing blocks in the PointCloudProcessingBlocks scene, except for Temporaral Filter
        if (ActiveProfile != null)
        {
            string devName = ActiveProfile.Device.Info.GetInfo(CameraInfo.Name);
            if (!string.IsNullOrEmpty(devName) && devName.IndexOf("L5", StringComparison.OrdinalIgnoreCase) >= 0)
            {
                GameObject pbPanel = GameObject.Find("ProcessingBlocks/ScrollRect/Viewport/Content");
                if (pbPanel != null)
                {
                    foreach (Transform child in pbPanel.transform)
                    {
                        if (!child.name.Equals("TemporaFilter"))
                        {
                            child.gameObject.SetActive(false);
                            Toggle toggle = child.transform.Find("Toggle").gameObject.GetComponent<Toggle>();
                            if (toggle != null && toggle.isOn)
                                toggle.isOn = false;
                        }
                    }
                }
            }

        }

        Streaming = true;
        var h = OnStart;
        if (h != null)
            h.Invoke(activeProfile);
    }

    private void OnSourceStop()
    {
        if (!Streaming)
            return;
        if (_block != null)
            Source.OnNewSample -= _block.Process;
        Streaming = false;
        var h = OnStop;
        if (h != null)
            h();
    }

    private void OnFrame(Frame f)
    {
        var onNewSample = OnNewSample;
        if (onNewSample != null)
            onNewSample.Invoke(f);
    }

    private void OnDestroy()
    {
        OnSourceStop();
        if (_block != null)
        {
            _block.Dispose();
            _block = null;
        }
    }

    internal void ProcessFrame(Frame frame, FrameSource src)
    {
        try
        {
            if (!Streaming)
                return;

            Frame f = frame;

            if (profile != null)
            {
                var filters = profile._processingBlocks.AsReadOnly();

                foreach (var pb in filters)
                {
                    if (pb == null || !pb.Enabled)
                        continue;

                    var r = pb.Process(f, src);
                    if (r != f)
                    {
                        // Prevent from disposing the original frame during post-processing
                        if (f != frame)
                        {
                            f.Dispose();
                        }
                        f = r;
                    }
                }
            }

            src.FrameReady(f);

            if (f != frame)
                f.Dispose();
        }
        catch (Exception e)
        {
            Debug.LogException(e);
        }
    }
}
