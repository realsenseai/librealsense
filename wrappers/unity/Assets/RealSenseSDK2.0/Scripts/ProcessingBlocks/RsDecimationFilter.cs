using Intel.RealSense;
using UnityEngine;

[ProcessingBlockDataAttribute(typeof(DecimationFilter))]
[HelpURL("https://github.com/realsenseai/librealsense/blob/master/doc/post-processing-filters.md#decimation-filter")]
public class RsDecimationFilter : RsProcessingBlock
{
    public Stream _streamFilter = Stream.Depth;
    public Format _formatFilter = Format.Z16;

    /// <summary>
    /// Number of filter iterations
    /// </summary>
    [Range(2, 8)]
    [Tooltip("Number of filter iterations")]
    public int _filterMagnitude = 2;


    private DecimationFilter _pb;
    private IOption filterMag;
    private IOption streamFilter;
    private IOption formatFilter;

    public override Frame Process(Frame frame, FrameSource frameSource)
    {
        if (_pb == null)
        {
            Init();
        }

        UpdateOptions();

        return _pb.Process(frame);
    }

    public void Init()
    {
        _pb = new DecimationFilter();
        filterMag = _pb.Options[Option.FilterMagnitude];
        streamFilter = _pb.Options[Option.StreamFilter];
        formatFilter = _pb.Options[Option.StreamFormatFilter];
    }

    void OnDisable()
    {
        if (_pb != null)
        {
            _pb.Dispose();
            _pb = null;
        }
    }

    public void SetMagnitude(float val)
    {
        _filterMagnitude = (int)val;
    }
    
    private void UpdateOptions()
    {
        filterMag.Value = _filterMagnitude;
        streamFilter.Value = (float)_streamFilter;
        formatFilter.Value = (float)_formatFilter;
    }
}

