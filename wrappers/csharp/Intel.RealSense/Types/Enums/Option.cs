// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.

namespace Intel.RealSense
{
    /// <summary>
    /// Defines general configuration controls.
    /// <para>
    /// These can generally be mapped to camera UVC controls, and unless stated otherwise, can be set/queried at any time.
    /// </para>
    /// </summary>
    public enum Option
    {
        /// <summary>Enable / disable color backlight compensation</summary>
        BacklightCompensation = 0,

        /// <summary>Color image brightness</summary>
        Brightness = 1,

        /// <summary>Color image contrast</summary>
        Contrast = 2,

        /// <summary>Controls exposure time of color camera. Setting any value will disable auto exposure</summary>
        Exposure = 3,

        /// <summary>Color image gain</summary>
        Gain = 4,

        /// <summary>Color image gamma setting</summary>
        Gamma = 5,

        /// <summary>Color image hue</summary>
        Hue = 6,

        /// <summary>Color image saturation setting</summary>
        Saturation = 7,

        /// <summary>Color image sharpness setting</summary>
        Sharpness = 8,

        /// <summary>Controls white balance of color image. Setting any value will disable auto white balance</summary>
        WhiteBalance = 9,

        /// <summary>Enable / disable color image auto-exposure</summary>
        EnableAutoExposure = 10,

        /// <summary>Enable / disable color image auto-white-balance</summary>
        EnableAutoWhiteBalance = 11,

        /// <summary>Provide access to several recommend sets of option presets for the depth camera</summary>
        VisualPreset = 12,

        /// <summary>Power of the F200 / SR300 projector, with 0 meaning projector off</summary>
        LaserPower = 13,

        /// <summary>Set the number of patterns projected per frame. The higher the accuracy value the more patterns projected. Increasing the number of patterns help to achieve better accuracy. Note that this control is affecting the Depth FPS</summary>
        Accuracy = 14,

        /// <summary>Motion vs. Range trade-off, with lower values allowing for better motion sensitivity and higher values allowing for better depth range</summary>
        MotionRange = 15,

        /// <summary>Set the filter to apply to each depth frame. Each one of the filter is optimized per the application requirements</summary>
        FilterOption = 16,

        /// <summary>The confidence level threshold used by the Depth algorithm pipe to set whether a pixel will get a valid range or will be marked with invalid range</summary>
        ConfidenceThreshold = 17,

        /// <summary>Laser Emitter enabled</summary>
        EmitterEnabled = 18,

        /// <summary>Number of frames the user is allowed to keep per stream. Trying to hold-on to more frames will cause frame-drops.</summary>
        FramesQueueSize = 19,

        /// <summary>Total number of detected frame drops from all streams</summary>
        TotalFrameDrops = 20,

        /// <summary>Auto-Exposure modes: Static, Anti-Flicker and Hybrid</summary>
        AutoExposureMode = 21,

        /// <summary>Power Line Frequency control for anti-flickering Off/50Hz/60Hz/Auto</summary>
        PowerLineFrequency = 22,

        /// <summary>Current Asic Temperature</summary>
        AsicTemperature = 23,

        /// <summary>disable error handling</summary>
        ErrorPollingEnabled = 24,

        /// <summary>Current Projector Temperature</summary>
        ProjectorTemperature = 25,

        /// <summary>Enable / disable trigger to be outputed from the camera to any external device on every depth frame</summary>
        OutputTriggerEnabled = 26,

        /// <summary>Current Motion-Module Temperature</summary>
        MotionModuleTemperature = 27,

        /// <summary>Number of meters represented by a single depth unit</summary>
        DepthUnits = 28,

        /// <summary>Enable/Disable automatic correction of the motion data</summary>
        EnableMotionCorrection = 29,

        /// <summary>Allows sensor to dynamically ajust the frame rate depending on lighting conditions</summary>
        AutoExposurePriority = 30,

        /// <summary>Color scheme for data visualization</summary>
        ColorScheme = 31,

        /// <summary>Perform histogram equalization post-processing on the depth data</summary>
        HistogramEqualizationEnabled = 32,

        /// <summary>Minimal distance to the target</summary>
        MinDistance = 33,

        /// <summary>Maximum distance to the target</summary>
        MaxDistance = 34,

        /// <summary>Texture mapping stream unique ID</summary>
        TextureSource = 35,

        /// <summary>The 2D-filter effect. The specific interpretation is given within the context of the filter</summary>
        FilterMagnitude = 36,

        /// <summary>2D-filter parameter controls the weight/radius for smoothing.</summary>
        FilterSmoothAlpha = 37,

        /// <summary>2D-filter range/validity threshold</summary>
        FilterSmoothDelta = 38,

        /// <summary>Enhance depth data post-processing with holes filling where appropriate</summary>
        HolesFill = 39,

        /// <summary>The distance in mm between the first and the second imagers in stereo-based depth cameras</summary>
        StereoBaseline = 40,

        /// <summary>Allows dynamically ajust the converge step value of the target exposure in Auto-Exposure algorithm</summary>
        AutoExposureConvergeStep = 41,

        /// <summary>Impose Inter-camera HW synchronization mode. Applicable for D400/Rolling Shutter SKUs</summary>
        InterCamSyncMode = 42,

        /// <summary>Select a stream to process</summary>
        StreamFilter = 43,

        /// <summary>Select a stream format to process</summary>
        StreamFormatFilter = 44,

        /// <summary>Select a stream index to process</summary>
        StreamIndexFilter = 45,

        /// <summary>When supported, this option make the camera to switch the emitter state every frame. 0 for disabled, 1 for enabled</summary>
        EmitterOnOff = 46,

        /// <summary>Deprecated!!! - Zero order point x</summary>
        ZeroOrderPointX = 47,

        /// <summary>Deprecated!!! - Zero order point y</summary>
        ZeroOrderPointY = 48,

        /// <summary>LLD temperature</summary>
        LLDTemperature = 49,

        /// <summary>MC temperature</summary>
        MCTemperature = 50,

        /// <summary>MA temperature</summary>
        MATemperature = 51,

        /// <summary>Hardware stream configuration</summary>
        HardwarePreset = 52,

        /// <summary>Disable global time</summary>
        GlobalTimeEnabled = 53,

        /// <summary>APD temperature</summary>
        APDTemperature = 54,

        /// <summary>Enable an internal map</summary>
        EnableMapping = 55,

        /// <summary>Enable appearance based relocalization</summary>
        EnableRelocalization = 56,

        /// <summary>Enable position jumping</summary>
        EnablePoseJumping = 57,

        /// <summary>Enable dynamic calibration</summary>
        EnableDynamicCalibration = 58,

        /// <summary>Offset from sensor to depth origin in millimetrers</summary>
        DepthOffset = 59,

        /// <summary>Power of the LED (light emitting diode), with 0 meaning LED off</summary>
        LedPower = 60,

        /// <summary>Deprecated!!! - Toggle Zero-Order mode</summary>
        ZeroOrderEnabled = 61,

        /// <summary>Preserve previous map when starting</summary>
        EnableMapPreservation = 62,

        /// <summary>Enable/disable sensor shutdown when a free-fall is detected (on by default)</summary>
        FreeFallDetectionEnabled = 63,

        /// <summary>Changes the exposure time of Avalanche Photo Diode in the receiver</summary>
        APDExposureTime = 64,

        /// <summary>Changes the amount of sharpening in the post-processed image</summary>
        PostProcessingSharpening = 65,

        /// <summary>Changes the amount of sharpening in the pre-processed image</summary>
        PreProcessingSharpening = 66,

        /// <summary>Control edges and background noise</summary>
        NoiseFilterLevel = 67,

        /// <summary>Enable\disable pixel invalidation</summary>
        InvalidationBypass = 68,

        /// <summary>Deprecated - Use digital gain option, Change the depth ambient light see rs2_ambient_light for values</summary>
        AmbientLightEnvLevel = 69,

        /// <summary>Change the depth digital gain see rs2_digital_gain for values</summary>
        DigitalGain = 69,

        /// <summary>Deprecated - The resolution mode: see rs2_sensor_mode for values</summary>
        SensorMode = 70,

        /// <summary>Enable Laser On constantly (GS SKU Only)</summary>
        EmitterAlwaysOn = 71,

        /// <summary>Depth Thermal Compensation for selected D400 SKUs</summary>
        ThermalCompensation = 72,

        /// <summary>Camera Accuracy Health</summary>
        TriggerCameraAccuracyHealth = 73,

        /// <summary>Reset Camera Accuracy Health</summary>
        ResetCameraAccuracyHealth = 74,

        /// <summary>Host Performance</summary>
        HostPerformance = 75,

        /// <summary>HDR Enabled (ON = 1, OFF = 0) - for D400 SKUs</summary>
        HdrEnabled = 76,

        /// <summary>Subpreset sequence Name - for D400 SKUs</summary>
        SequenceName = 77,

        /// <summary>Subpreset sequence size - for D400 SKUs</summary>
        SequenceSize = 78,

        /// <summary>Subpreset sequence id - for D400 SKUs</summary>
        SequenceId = 79,

        /// <summary>Humidity temperature [Deg Celsius]</summary>
        HumidityTemperature = 80,

        /// <summary>Turn on/off the maximum usable range who calculates the maximum range of the camera given the amount of ambient light in the scene </summary>
        EnableMaxUsableRange = 81,

        /// <summary>Turn on/off the alternate IR, When enabling alternate IR, the IR image is holding the amplitude of the depth correlation. </summary>
        AlternateIR = 82,

        /// <summary>Noise estimation on the IR image</summary>
        NoiseEstimation = 83,

        /// <summary>Enables data collection for calculating IR pixel reflectivity</summary>
        EnableIrReflectivity = 84,

        /// <summary>Auto exposure limit - for D400 SKUs</summary>
        auto_exposure_limit = 85,

        /// <summary>auto gain limit - for D400 SKUs</summary>
        auto_gain_limit = 86,

        /// <summary>Enable automatic receiver sensitivity</summary>
        auto_rx_sensitivity = 87,

        /// <summary>Change transmitter frequency, increasing effective range over sharpness</summary>
        transmitter_frequency = 88,

        /// <summary>Enables vertical binning which increases the maximal sensed distance</summary>
        vertical_binning = 89,

        /// <summary>Control the receiver sensitivity to incoming light, both projected and ambient</summary>
        receiver_sensitivity = 90,

        /// <summary>Enable firmware depth-to-color alignment for object detection distance</summary>
        AlignDepth = 109
    }
}
