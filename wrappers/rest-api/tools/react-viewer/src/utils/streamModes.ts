import type { SensorConfig, StreamConfig, SupportedStreamProfile } from '../api/types'

/** The exact mode a stream will be started with, given the sensor-level selection. */
export function effectiveMode(config: StreamConfig, sensorConfig?: SensorConfig) {
  const perStream = !sensorConfig || sensorConfig.isMotionSensor
  return {
    width: perStream || sensorConfig.perStreamResolution ? config.resolution.width : sensorConfig.resolution.width,
    height: perStream || sensorConfig.perStreamResolution ? config.resolution.height : sensorConfig.resolution.height,
    fps: perStream || sensorConfig.perStreamFps ? config.framerate : sensorConfig.framerate,
    format: config.format,
  }
}

/**
 * Whether the SDK lists the exact mode; the legacy viewer refuses to start on
 * "Selected value is not supported". Profiles without a mode list cannot be checked.
 */
export function isModeSupported(profile: SupportedStreamProfile | undefined, mode: ReturnType<typeof effectiveMode>): boolean {
  if (!profile?.modes || profile.modes.length === 0) return true
  return profile.modes.some(([w, h, fps, fmt]) =>
    w === mode.width && h === mode.height && fps === mode.fps && fmt.toLowerCase() === mode.format.toLowerCase())
}

/** Names of the enabled streams whose selected mode the sensor does not list. */
export function unsupportedStreams(streams: StreamConfig[], profiles: SupportedStreamProfile[], sensorConfig?: SensorConfig): string[] {
  return streams
    .filter((c) => c.enable)
    .filter((c) => !isModeSupported(
      profiles.find((p) => p.stream_type.toLowerCase() === c.stream_type.toLowerCase()), effectiveMode(c, sensorConfig)))
    .map((c) => c.stream_type)
}
