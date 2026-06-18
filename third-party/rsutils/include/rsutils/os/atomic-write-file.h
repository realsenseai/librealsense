// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#pragma once

#include <string>


namespace rsutils {
namespace os {


// Writes content to filename atomically via a temp file and rename.
// The temp file is named "<filename>.<pid>.<tid>.<counter>.tmp" (thread-unique); stale copies are benign and safe to delete.
// Returns true on success, false on any failure (open, write, or rename).
//
// On Windows, rename is retried up to max_retries times (sleeping retry_delay_ms ms between
// attempts) when the error is ERROR_SHARING_VIOLATION or ERROR_ACCESS_DENIED.
//
// Known limitations (accepted for this use case):
//   - Power-loss safety: ofstream::close() does not fsync, so file data may still be in the OS
//     page cache at the point of rename. Protects against application crashes, not power loss.
//   - Non-ASCII paths on Windows: MoveFileExA uses the ANSI codepage and may fail for paths
//     containing non-ASCII characters (e.g. non-ASCII username in %APPDATA%).
//
bool atomic_write_file( const std::string & filename,
                        const std::string & content,
                        int max_retries = 5,
                        int retry_delay_ms = 20 );


}  // namespace os
}  // namespace rsutils
