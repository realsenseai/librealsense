// License: Apache 2.0. Copyright (C) 2026 RealSense, Inc.
// Used only when the FW tree is unavailable: rule content is C1-owned and is not
// part of the I1 export, so FW admission assertions are skipped in that mode.
#include "IntentProtocol.h"
namespace rsintent {
Error validate(const Model &, uint16_t) { return Error(); }
} // namespace rsintent
