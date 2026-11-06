// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

export const isDesktopApp = '__TAURI__' in window

// Desktop app pages are served by the app itself; the bundled backend is always local.
export const BACKEND_ORIGIN = isDesktopApp ? 'http://localhost:8000' : window.location.origin
