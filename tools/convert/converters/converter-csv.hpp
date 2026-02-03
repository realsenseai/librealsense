// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2018 RealSense, Inc. All Rights Reserved.

#ifndef __RS_CONVERTER_CONVERTER_CSV_H
#define __RS_CONVERTER_CONVERTER_CSV_H


#include <fstream>
#include <map>
#include <mutex>
#include <array>
#include <condition_variable>
#include "../converter.hpp"


namespace rs2 {
    namespace tools {
        namespace converter {

            struct stringify
            {
                std::ostringstream ss;
                template<class T> stringify& operator << (const T& val) { ss << val; return *this; }
                operator std::string() const { return ss.str(); }
            };

            template <typename T>
            inline bool val_in_range(const T& val, const std::initializer_list<T>& list)
            {
                for (const auto& i : list) {
                    if (val == i) {
                        return true;
                    }
                }
                return false;
            }

            class converter_csv : public converter_base {
            protected:
                struct motion_pose_frame_record {
                    motion_pose_frame_record(rs2_stream stream_type, int stream_index,
                        unsigned long long frame_number,
                        long long frame_ts, long long backend_ts, long long arrival_time,
                        double p1 = 0., double p2 = 0., double p3 = 0.,
                        double p4 = 0., double p5 = 0., double p6 = 0., double p7 = 0.);

                    rs2_stream              _stream_type;
                    int                     _stream_idx;
                    unsigned long long      _frame_number;
                    long long               _frame_ts;          // Device-based timestamp. (msec)
                    long long               _backend_ts;        // Driver-based timestamp. (msec)
                    long long               _arrival_time;      // Host arrival timestamp (msec)
                    std::array<double, 7>   _params;            // The parameters are optional and sensor specific
                    
                    std::string to_string() const;
                };
                std::string get_time_string() const;

                rs2_stream _streamType;
                std::string _filePath;
                std::map<std::pair<rs2_stream, int>, std::vector<motion_pose_frame_record>> _imu_pose_collection;

            public:

                converter_csv(const std::string& filePath, rs2_stream streamType = rs2_stream::RS2_STREAM_ANY);

                void convert(rs2::frame& frame) override;

                void wait() override { }

                void flush() override;
                
                std::string name() const override
                {
                    return "CSV converter";
                }

                void convert_depth(rs2::depth_frame& depthframe);
                void convert_motion_pose(rs2::frame& f);
                void save_motion_pose_data_to_file();
            };
        } // namespace converter
    } // namespace tools
} // namespace rs2



#endif
