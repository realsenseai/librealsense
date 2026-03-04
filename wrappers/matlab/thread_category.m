classdef thread_category < int64
    enumeration
        usb_io            (0)
        video_capture     (1)
        sensor_io         (2)
        frame_processing  (3)
        device_monitoring (4)
        dispatch          (5)
        network           (6)
        utility           (7)
        count             (8)
    end
end
