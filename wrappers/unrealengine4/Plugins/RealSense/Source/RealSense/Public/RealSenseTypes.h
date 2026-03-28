#pragma once

#include "RealSenseTypes.generated.h"

namespace rs2 {
    class config;
    class device;
    class pipeline;
    class frameset;
    class frame;
    class align;
    class pointcloud;
    class points;
}

// typedef enum rs2_stream
UENUM(Blueprintable)
enum class ERealSenseStreamType : uint8
{
    STREAM_ANY,
    STREAM_DEPTH                            , /**< Native stream of depth data produced by RealSense device */
    STREAM_COLOR                            , /**< Native stream of color data captured by RealSense device */
    STREAM_INFRARED                         , /**< Native stream of infrared data captured by RealSense device */
};

// typedef enum rs2_format
UENUM(Blueprintable)
enum class ERealSenseFormatType : uint8
{
    FORMAT_ANY             , /**< When passed to enable stream, librealsense will try to provide best suited format */
    FORMAT_Z16             , /**< 16-bit linear depth values. The depth is meters is equal to depth scale * pixel value. */
    FORMAT_DISPARITY16     , /**< 16-bit linear disparity values. The depth in meters is equal to depth scale / pixel value. */
    FORMAT_XYZ32F          , /**< 32-bit floating point 3D coordinates. */
    FORMAT_YUYV            , /**< Standard YUV pixel format as described in https://en.wikipedia.org/wiki/YUV */
    FORMAT_RGB8            , /**< 8-bit red, green and blue channels */
    FORMAT_BGR8            , /**< 8-bit blue, green, and red channels -- suitable for OpenCV */
    FORMAT_RGBA8           , /**< 8-bit red, green and blue channels + constant alpha channel equal to FF */
    FORMAT_BGRA8           , /**< 8-bit blue, green, and red channels + constant alpha channel equal to FF */
    FORMAT_Y8              , /**< 8-bit per-pixel grayscale image */
    FORMAT_Y16             , /**< 16-bit per-pixel grayscale image */
    FORMAT_RAW10           , /**< Four 10-bit luminance values encoded into a 5-byte macropixel */
    FORMAT_RAW16           , /**< 16-bit raw image */
    FORMAT_RAW8            , /**< 8-bit raw image */
    FORMAT_UYVY            , /**< Similar to the standard YUYV pixel format, but packed in a different order */
    FORMAT_MOTION_RAW      , /**< Raw data from the motion sensor */
    FORMAT_MOTION_XYZ32F   , /**< Motion data packed as 3 32-bit float values, for X, Y, and Z axis */
    FORMAT_GPIO_RAW        , /**< Raw data from the external sensors hooked to one of the GPIO's */
    FORMAT_6DOF            , /**< Pose data packed as floats array, containing translation vector, rotation quaternion and prediction velocities and accelerations vectors */
    FORMAT_DISPARITY32     , /**< 32-bit float-point disparity values. Depth->Disparity conversion : Disparity = Baseline*FocalLength/Depth */
};

// typedef enum rs2_option
UENUM(Blueprintable)
enum class ERealSenseOptionType : uint8
{
    BACKLIGHT_COMPENSATION                     , /**< Enable / disable color backlight compensation*/
    BRIGHTNESS                                 , /**< Color image brightness*/
    CONTRAST                                   , /**< Color image contrast*/
    EXPOSURE                                   , /**< Controls exposure time of color camera. Setting any value will disable auto exposure*/
    GAIN                                       , /**< Color image gain*/
    GAMMA                                      , /**< Color image gamma setting*/
    HUE                                        , /**< Color image hue*/
    SATURATION                                 , /**< Color image saturation setting*/
    SHARPNESS                                  , /**< Color image sharpness setting*/
    WHITE_BALANCE                              , /**< Controls white balance of color image. Setting any value will disable auto white balance*/
    ENABLE_AUTO_EXPOSURE                       , /**< Enable / disable color image auto-exposure*/
    ENABLE_AUTO_WHITE_BALANCE                  , /**< Enable / disable color image auto-white-balance*/
    VISUAL_PRESET                              , /**< Provide access to several recommend sets of option presets for the depth camera */
    LASER_POWER                                , /**< Power of the F200 / SR300 projector, with 0 meaning projector off*/
    ACCURACY                                   , /**< Set the number of patterns projected per frame. The higher the accuracy value the more patterns projected. Increasing the number of patterns help to achieve better accuracy. Note that this control is affecting the Depth FPS */
    MOTION_RANGE                               , /**< Motion vs. Range trade-off, with lower values allowing for better motion sensitivity and higher values allowing for better depth range*/
    FILTER_OPTION                              , /**< Set the filter to apply to each depth frame. Each one of the filter is optimized per the application requirements*/
    CONFIDENCE_THRESHOLD                       , /**< The confidence level threshold used by the Depth algorithm pipe to set whether a pixel will get a valid range or will be marked with invalid range*/
    EMITTER_ENABLED                            , /**< Laser Emitter enabled */
    FRAMES_QUEUE_SIZE                          , /**< Number of frames the user is allowed to keep per stream. Trying to hold-on to more frames will cause frame-drops.*/
    TOTAL_FRAME_DROPS                          , /**< Total number of detected frame drops from all streams */
    AUTO_EXPOSURE_MODE                         , /**< Auto-Exposure modes: Static, Anti-Flicker and Hybrid */
    POWER_LINE_FREQUENCY                       , /**< Power Line Frequency control for anti-flickering Off/50Hz/60Hz/Auto */
    ASIC_TEMPERATURE                           , /**< Current Asic Temperature */
    ERROR_POLLING_ENABLED                      , /**< disable error handling */
    PROJECTOR_TEMPERATURE                      , /**< Current Projector Temperature */
    OUTPUT_TRIGGER_ENABLED                     , /**< Enable / disable trigger to be outputed from the camera to any external device on every depth frame */
    MOTION_MODULE_TEMPERATURE                  , /**< Current Motion-Module Temperature */
    DEPTH_UNITS                                , /**< Number of meters represented by a single depth unit */
    ENABLE_MOTION_CORRECTION                   , /**< Enable/Disable automatic correction of the motion data */
    AUTO_EXPOSURE_PRIORITY                     , /**< Allows sensor to dynamically ajust the frame rate depending on lighting conditions */
    COLOR_SCHEME                               , /**< Color scheme for data visualization */
    HISTOGRAM_EQUALIZATION_ENABLED             , /**< Perform histogram equalization post-processing on the depth data */
    MIN_DISTANCE                               , /**< Minimal distance to the target */
    MAX_DISTANCE                               , /**< Maximum distance to the target */
    TEXTURE_SOURCE                             , /**< Texture mapping stream unique ID */
    FILTER_MAGNITUDE                           , /**< The 2D-filter effect. The specific interpretation is given within the context of the filter */
    FILTER_SMOOTH_ALPHA                        , /**< 2D-filter parameter controls the weight/radius for smoothing.*/
    FILTER_SMOOTH_DELTA                        , /**< 2D-filter range/validity threshold*/
    HOLES_FILL                                 , /**< Enhance depth data post-processing with holes filling where appropriate*/
    STEREO_BASELINE                            , /**< The distance in mm between the first and the second imagers in stereo-based depth cameras*/
    AUTO_EXPOSURE_CONVERGE_STEP                , /**< Allows dynamically ajust the converge step value of the target exposure in Auto-Exposure algorithm*/
    INTER_CAM_SYNC_MODE                        , /**< Impose Inter-camera HW synchronization mode. Applicable for D400/L500/Rolling Shutter SKUs */
    STREAM_FILTER                              , /**< Select a stream to process */
    STREAM_FORMAT_FILTER                       , /**< Select a stream format to process */
    STREAM_INDEX_FILTER                        , /**< Select a stream index to process */
    EMITTER_ON_OFF                             , /**< When supported, this option make the camera to switch the emitter state every frame. 0 for disabled, 1 for enabled */
    ZERO_ORDER_POINT_X                         , /**< Zero order point x*/
    ZERO_ORDER_POINT_Y                         , /**< Zero order point y*/
    LLD_TEMPERATURE                            , /**< LLD temperature*/
    MC_TEMPERATURE                             , /**< MC temperature*/
    MA_TEMPERATURE                             , /**< MA temperature*/
    HARDWARE_PRESET                            , /**< Hardware stream configuration */
    GLOBAL_TIME_ENABLED                        , /**< disable global time  */
    APD_TEMPERATURE                            , /**< APD temperature*/
    ENABLE_MAPPING                             , /**< Enable an internal map */
    ENABLE_RELOCALIZATION                      , /**< Enable appearance based relocalization */
    ENABLE_POSE_JUMPING                        , /**< Enable position jumping */
    ENABLE_DYNAMIC_CALIBRATION                 , /**< Enable dynamic calibration */
    DEPTH_OFFSET                               , /**< Offset from sensor to depth origin in millimetrers */
    LED_POWER                                  , /**< Power of the LED (light emitting diode), with 0 meaning LED off */
    ZERO_ORDER_ENABLED                         , /**< Deprecated!! -  Zero-order mode */
    ENABLE_MAP_PRESERVATION                    , /**< Preserve map from the previous run */
    FREEFALL_DETECTION_ENABLED                 , /**< Enable/disable sensor shutdown when a free-fall is detected (on by default) */
    AVALANCHE_PHOTO_DIODE                      , /**< Changes the exposure time of Avalanche Photo Diode in the receiver */
    POST_PROCESSING_SHARPENING                 , /**< Changes the amount of sharpening in the post-processed image */
    PRE_PROCESSING_SHARPENING                  , /**< Changes the amount of sharpening in the pre-processed image */
    NOISE_FILTERING                            , /**< Control edges and background noise */
    INVALIDATION_BYPASS                        , /**< Enable\disable pixel invalidation */
    AMBIENT_LIGHT                              , /**< Change the depth ambient light see rs2_ambient_light for values */
    DIGITAL_GAIN = AMBIENT_LIGHT               , /**< Change the depth digital gain see rs2_digital_gain for values */
    SENSOR_MODE                                , /**< Deprecated!! - The resolution mode: see rs2_sensor_mode for values */
    EMITTER_ALWAYS_ON                          , /**< Enable Laser On constantly (GS SKU Only) */
    THERMAL_COMPENSATION                       , /**< Depth Thermal Compensation for selected D400 SKUs */
    TRIGGER_CAMERA_ACCURACY_HEALTH             , /**< DEPRECATED! */
    RESET_CAMERA_ACCURACY_HEALTH               , /**< DEPRECATED! */
    HOST_PERFORMANCE                           , /**< Set host performance mode to optimize device settings so host can keep up with workload, for example, USB transaction granularity, setting option to low performance host leads to larger USB transaction size and reduced number of transactions which improves performance and stability if host is relatively weak as compared to workload */
    HDR_ENABLED                                , /**< Enable / disable HDR */
    SEQUENCE_NAME                              , /**< HDR Sequence name */
    SEQUENCE_SIZE                              , /**< HDR Sequence size */
    SEQUENCE_ID                                , /**< HDR Sequence ID - 0 is not HDR; sequence ID for HDR configuration starts from 1 */
    HUMIDITY_TEMPERATURE                       , /**< Humidity temperature [Deg Celsius] */
    ENABLE_MAX_USABLE_RANGE                    , /**< Turn on/off the maximum usable range who calculates the maximum range of the camera given the amount of ambient light in the scene */
    ALTERNATE_IR                               , /**< Turn on/off the alternate IR, When enabling alternate IR, the IR image is holding the amplitude of the depth correlation. */
    NOISE_ESTIMATION                           , /**< Noise estimation - indicates the noise on the IR image */
    ENABLE_IR_REFLECTIVITY                     , /**< Enables data collection for calculating IR pixel reflectivity */
    AUTO_EXPOSURE_LIMIT                        , /**< Set and get auto exposure limit in microseconds. Default is 0 which means full exposure range. If the requested exposure limit is greater than frame time, it will be set to frame time at runtime. Setting will not take effect until next streaming session. */
    AUTO_GAIN_LIMIT                            , /**< Set and get auto gain limits ranging from 16 to 248. Default is 0 which means full gain. If the requested gain limit is less than 16, it will be set to 16. If the requested gain limit is greater than 248, it will be set to 248. Setting will not take effect until next streaming session. */
    AUTO_RX_SENSITIVITY                        , /**< Set and get auto receiver sensitivity.*/
    TRANSMITTER_FREQUENCY                      , /**< Change transmitter frequency, increasing effective range over sharpness. */
    VERTICAL_BINNING                           , /**< Enables vertical binning which increases the maximal sensed distance. */
    RECEIVER_SENSITIVITY                       , /**< Control the receiver sensitivity to incoming light, both projected and ambient. */
    EMITTER_FREQUENCY                          , /**< Select emitter (laser) frequency:  0 --> 57[KHZ], 1 --> 97[KHZ] */
    DEPTH_AUTO_EXPOSURE_MODE                   , /**< Select depth sensor auto exposure mode:  0 --> REGULAR 1 --> ACCELERATED */
    SAFETY_PRESET_ACTIVE_INDEX                 , /**< Set / Get current active safety preset index**/
    SAFETY_MODE                                , /**< Safety camera operation mode see rs2_safety_mode for values. */
    RGB_TNR_ENABLED                            , /**< RGB Temporal Noise Reduction enabling ON (1) / OFF (0)*/
};

UENUM(Blueprintable)
enum class ERealSensePipelineMode : uint8
{
    CaptureOnly,
    RecordFile,
    PlaybackFile,
};

UENUM(Blueprintable)
enum class ERealSenseDepthColormap : uint8
{
    Jet,
    Classic,
    WhiteToBlack,
    BlackToWhite,
    Bio,
    Cold,
    Warm,
    Quantized,
    Pattern,
};

// typedef enum rs2_thread_category
UENUM(Blueprintable)
enum class ERealSenseThreadCategory : uint8
{
    USB_IO                                     , /**< USB I/O threads */
    VIDEO_CAPTURE                              , /**< Video capture threads */
    SENSOR_IO                                  , /**< Sensor I/O threads */
    FRAME_PROCESSING                           , /**< Frame processing threads */
    DEVICE_MONITORING                          , /**< Device monitoring threads */
    DISPATCH                                   , /**< Dispatch threads */
    NETWORK                                    , /**< Network threads */
    UTILITY                                    , /**< Utility threads */
};

USTRUCT(BlueprintType)
struct FRealSenseStreamProfile
{
    GENERATED_BODY()

    UPROPERTY(Category="RealSense", BlueprintReadWrite, EditAnywhere)
    ERealSenseStreamType StreamType = ERealSenseStreamType::STREAM_ANY;

    UPROPERTY(Category="RealSense", BlueprintReadWrite, EditAnywhere)
    ERealSenseFormatType Format = ERealSenseFormatType::FORMAT_ANY;

    UPROPERTY(Category="RealSense", BlueprintReadWrite, EditAnywhere)
    int32 Width = 640;

    UPROPERTY(Category="RealSense", BlueprintReadWrite, EditAnywhere)
    int32 Height = 480;

    UPROPERTY(Category="RealSense", BlueprintReadWrite, EditAnywhere)
    int32 Rate = 30;
};

USTRUCT(BlueprintType)
struct FRealSenseStreamMode
{
    GENERATED_BODY()

    UPROPERTY(Category="RealSense", BlueprintReadWrite, EditAnywhere)
    int32 Width = 640;

    UPROPERTY(Category="RealSense", BlueprintReadWrite, EditAnywhere)
    int32 Height = 480;

    UPROPERTY(Category="RealSense", BlueprintReadWrite, EditAnywhere)
    int32 Rate = 30;
};

USTRUCT(BlueprintType)
struct FRealSenseOptionRange
{
    GENERATED_BODY()

    UPROPERTY(Category="RealSense", BlueprintReadOnly, VisibleAnywhere)
    float Min;

    UPROPERTY(Category="RealSense", BlueprintReadOnly, VisibleAnywhere)
    float Max;

    UPROPERTY(Category="RealSense", BlueprintReadOnly, VisibleAnywhere)
    float Step;

    UPROPERTY(Category="RealSense", BlueprintReadOnly, VisibleAnywhere)
    float Default;
};
