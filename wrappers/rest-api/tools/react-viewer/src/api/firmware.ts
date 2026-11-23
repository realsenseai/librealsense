// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

// The online versions list and the firmware images it links, read by the viewer itself as the legacy viewer does
// (common/updates-model.cpp): the server only writes an image it is given, as the SDK does.
import axios from 'axios'

export const VERSIONS_DB_URL = 'https://librealsense.realsenseai.com/Releases/rs_versions_db.json'

interface VersionEntry {
  device_name: string // a pattern, '*' a wildcard
  policy_type: string
  component: string
  version: string
  platform: string
  link: string
}

/** A DB name pattern matches the device's name; the legacy 'Intel ' prefix is ignored on both sides, as newer
 *  SDKs report names without it (is_device_name_equal) */
function nameMatches(pattern: string, name: string) {
  const drop = (s: string) => s.replace(/^Intel /, '')
  const re = drop(pattern).split('*').map((s) => s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')).join('.*')
  return new RegExp(`^${re}$`).test(drop(name))
}

/** The firmware the versions list recommends for a camera by its name, if any. The first match wins: the list
 *  names cameras before the wildcards they would also match. Only entries for any platform count: the image
 *  runs on the camera, not on the machine the viewer runs on */
export async function recommendedFirmware(deviceName: string): Promise<{ version: string; link: string } | undefined> {
  const versions: VersionEntry[] = (await (await fetch(VERSIONS_DB_URL)).json()).versions
  return versions.find((e) => e.component === 'FIRMWARE' && e.policy_type === 'RECOMMENDED'
    && e.platform === '*' && nameMatches(e.device_name, deviceName))
}

/** The recommended image itself, straight from the release host */
export async function downloadFirmware(link: string, onProgress: (progress: number) => void): Promise<Blob> {
  const response = await axios.get<Blob>(link, {
    responseType: 'blob',
    onDownloadProgress: (e) => e.total && onProgress(e.loaded / e.total),
  })
  return response.data
}
