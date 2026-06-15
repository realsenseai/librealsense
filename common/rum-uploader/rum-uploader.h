// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#pragma once

#include <string>
#include <functional>


// Phase-1 RUM uploader. Lives as a standalone module (linked into the viewer for now)
// with a deliberately small public boundary so it can be lifted into a separate
// executable/daemon in a later phase with minimal churn.
namespace rs2 {
namespace rum_uploader {


// Resolve the upload endpoint. RS2_RUM_ENDPOINT (dev/testing override) wins; otherwise
// the hardcoded cloud endpoint is used.
std::string endpoint();

// The last persisted report from the local store (<app-data>/rum/rum.json), or "" if none.
// This is the prior, completed session the boot uploader ships; the SDK writes it on context
// teardown. (The live, in-progress session is available via rs2::rum::get_report().)
std::string saved_report();

// POST the JSON report to the endpoint over HTTP(S). Returns true on HTTP success.
// Returns false (no-op) when HTTP support is not compiled in. TLS verification is left
// at libcurl defaults (enabled) — this is the path that sends data off the machine.
bool upload( std::string const & json_report, std::string const & endpoint );

// Cadence-gated background upload of the saved (prior-session) report. Returns immediately; the
// worker checks consent and that `cadence_hours` have elapsed since `last_upload_unix`, reads the
// on-disk report, and POSTs it. On success, on_uploaded(now_unix) runs on the worker so the caller
// can persist the new last-upload time. Pair with join_saved_upload() at teardown.
void start_saved_upload( int cadence_hours, long long last_upload_unix,
                         std::function< void( long long ) > on_uploaded );

// Join the background upload worker (call at teardown; safe if none was started).
void join_saved_upload();


}  // namespace rum_uploader
}  // namespace rs2
