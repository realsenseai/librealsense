// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2018 RealSense, Inc. All Rights Reserved.

#ifndef __RS_CONVERTER_CONVERTER_H
#define __RS_CONVERTER_CONVERTER_H

#include <map>
#include <unordered_set>
#include <thread>
#include <string>
#include <sstream>
#include <algorithm>

#include "librealsense2/rs.hpp"


namespace rs2 {
    namespace tools {
        namespace converter {

            void metadata_to_txtfile(const rs2::frame& frm, const std::string& filename);

            typedef unsigned long long frame_number_t;

            class converter_base {
            protected:
                std::thread _worker;
                std::vector<std::thread> _subWorkers;
                std::map<std::pair<rs2_stream, int>, std::unordered_set<frame_number_t>> _framesMap;

            protected:
                bool frames_map_get_and_set(rs2_stream streamType, int index, frame_number_t frameNumber);
                void wait_sub_workers();

                template <typename F> void start_worker(const F& f)
                {
                    _worker = std::thread(f);
                }

                template <typename F> void add_sub_worker(const F& f)
                {
                    _subWorkers.emplace_back(f);
                }

            public:
                virtual void convert(rs2::frame& frame) = 0;
                virtual std::string name() const = 0;

                virtual std::string get_statistics();

                virtual void wait();

                virtual void flush();
            };

        }
    }
}


#endif
