// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

#include <iostream>
#include "test.h"

namespace test {


std::string context;
bool debug = false;
std::string serial;
debug_logger log;


log_message::~log_message()
{
    std::cout << '-' << char(_type) << "- " << _line.str() << std::endl;
}

}
