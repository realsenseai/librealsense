using Intel.RealSense;
using UnityEngine;

[ProcessingBlockDataAttribute(typeof(ThresholdFilter))]
[HelpURL("https://github.com/realsenseai/librealsense/blob/master/doc/post-processing-filters.md#hole-filling-filter")]
public class RsThresholdFilter : RsProcessingBlock
{
    /// <summary>
    /// Min range in meters
    /// </summary>
    [Range(0f, 16f)]
    public float MinDistance = 0.1f;

    /// <summary>
    /// Max range in meters
    /// </summary>
    [Range(0f, 16f)]
    public float MaxDistance = 4f;

    private ThresholdFilter _pb;

    private IOption minOption;
    private IOption maxOption;

    public void Init()
    {
        _pb = new ThresholdFilter();
        minOption = _pb.Options[Option.MinDistance];
        maxOption = _pb.Options[Option.MaxDistance];
    }

    void OnDisable()
    {
        if (_pb != null)
            _pb.Dispose();
    }

    public override Frame Process(Frame frame, FrameSource frameSource)
    {
        if (_pb == null)
        {
            Init();
        }

        UpdateOptions();

        return _pb.Process(frame);
    }

    public void SetMinDistance(float v) {
        MinDistance = v;
    }
    public void SetMaxDistance(float v) {
        MaxDistance = v;
    }

    private void UpdateOptions()
    {
        minOption.Value = MinDistance;
        maxOption.Value = MaxDistance;
    }
}
