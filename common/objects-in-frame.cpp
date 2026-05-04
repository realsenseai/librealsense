// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "objects-in-frame.h"


std::string object_type_to_string( object_type type )
{
    switch( type )
    {
        case object_type::person: return "Person";
        case object_type::face: return "Face";
        default: return "Other";
    }
}
