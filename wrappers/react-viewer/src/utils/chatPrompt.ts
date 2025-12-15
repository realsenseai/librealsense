// System prompt builder and response parser for AI camera configuration chat

import type { DeviceState, SensorInfo, OptionInfo, StreamConfig } from '../api/types'

/**
 * Proposed settings from AI response
 */
export interface ProposedSettings {
  deviceSerial: string
  streamConfigs?: StreamConfig[]
  optionChanges?: Array<{
    sensorId: string
    optionId: string
    value: number | boolean | string
  }>
  explanation?: string
}

/**
 * Build a context-aware system prompt including current device states
 */
export function buildSystemPrompt(deviceStates: Record<string, DeviceState>): string {
  const deviceContext = buildDeviceContext(deviceStates)
  
  return `You are an expert Intel RealSense camera configuration assistant embedded in the RealSense Viewer application.

## Your Role
Help users configure their RealSense cameras using natural language. You can:
1. Explain camera settings and their effects
2. Recommend configurations for specific use cases
3. Propose specific settings changes that the user can apply with one click
4. Generate code snippets in Python or C++ for the RealSense SDK

## Current Device Context
${deviceContext || 'No cameras connected.'}

## Response Format
When proposing settings changes, include them in a JSON block that can be parsed:

\`\`\`settings
{
  "deviceSerial": "device_serial_number",
  "streamConfigs": [
    {
      "sensor_id": "sensor_name",
      "stream_type": "Depth|Color|Infrared|...",
      "format": "Z16|RGB8|...",
      "resolution": { "width": 640, "height": 480 },
      "framerate": 30,
      "enable": true
    }
  ],
  "optionChanges": [
    {
      "sensorId": "sensor_name",
      "optionId": "option_name",
      "value": 50
    }
  ],
  "explanation": "Brief explanation of what these settings do"
}
\`\`\`

## Guidelines
- Be concise but informative
- When users describe use cases (robotics, 3D scanning, etc.), recommend appropriate settings
- Always explain WHY certain settings are recommended
- If a setting is outside valid range, explain the constraint
- For code generation, use modern RealSense SDK 2.0 patterns
- Consider performance vs quality tradeoffs

## Available Stream Types
- Depth: Z16, Y8, Y16 formats
- Color: RGB8, BGR8, YUYV, MJPEG formats  
- Infrared: Y8, Y16 formats
- Gyro/Accel: Motion data (IMU)
- Pose: T265 tracking (if available)

## Common Use Cases
- Robotics: Lower resolution (424x240 or 640x360), higher FPS (60-90), depth only
- 3D Scanning: Higher resolution (1280x720+), lower FPS (15-30), depth + color aligned
- Face Tracking: 640x480 color, 30 FPS, auto-exposure enabled
- Object Detection: 640x480 depth+color, 30 FPS, post-processing filters
`
}

/**
 * Build device context string from current device states
 */
function buildDeviceContext(deviceStates: Record<string, DeviceState>): string {
  const deviceDescriptions: string[] = []
  
  for (const [serial, state] of Object.entries(deviceStates)) {
    const { device, sensors, options, streamConfigs, isStreaming } = state
    
    let desc = `### ${device.name} (Serial: ${serial})
- Status: ${isStreaming ? 'STREAMING' : 'STOPPED'}
- Firmware: ${device.firmware_version || 'Unknown'}
- USB Type: ${device.usb_type || 'Unknown'}

**Sensors:**
${formatSensors(sensors)}

**Current Stream Configuration:**
${formatStreamConfigs(streamConfigs)}

**Available Options:**
${formatOptions(options)}
`
    deviceDescriptions.push(desc)
  }
  
  return deviceDescriptions.join('\n---\n')
}

function formatSensors(sensors: SensorInfo[]): string {
  if (!sensors.length) return '  (none loaded)'
  
  return sensors.map(s => {
    const profiles = s.supported_stream_profiles || []
    const streamTypes = [...new Set(profiles.map(p => p.stream_type))].join(', ')
    return `  - ${s.name}: ${streamTypes || 'No streams'}`
  }).join('\n')
}

function formatStreamConfigs(configs: StreamConfig[]): string {
  const enabled = configs.filter(c => c.enable)
  if (!enabled.length) return '  (no streams enabled)'
  
  return enabled.map(c => 
    `  - ${c.stream_type}: ${c.resolution.width}x${c.resolution.height} @ ${c.framerate}fps (${c.format})`
  ).join('\n')
}

function formatOptions(options: Record<string, OptionInfo[]>): string {
  const formatted: string[] = []
  
  for (const [sensorId, opts] of Object.entries(options)) {
    // Only show key options to keep context manageable
    const keyOptions = opts.filter(o => isKeyOption(o.name))
    if (keyOptions.length) {
      formatted.push(`  ${sensorId}:`)
      keyOptions.forEach(o => {
        const range = o.min_value !== undefined ? ` [${o.min_value}-${o.max_value}]` : ''
        formatted.push(`    - ${o.name}: ${o.current_value}${range}`)
      })
    }
  }
  
  return formatted.length ? formatted.join('\n') : '  (no options loaded)'
}

/**
 * Check if an option is important enough to include in context
 */
function isKeyOption(name: string): boolean {
  const keyOptionPatterns = [
    'exposure', 'gain', 'brightness', 'contrast',
    'laser', 'emitter', 'depth units', 'visual preset',
    'accuracy', 'confidence', 'motion range',
    'auto', 'enable', 'power'
  ]
  const lowerName = name.toLowerCase()
  return keyOptionPatterns.some(p => lowerName.includes(p))
}

/**
 * Parse proposed settings from AI response
 */
export function parseProposedSettings(content: string): ProposedSettings | undefined {
  // Look for settings JSON block
  const settingsMatch = content.match(/```settings\s*([\s\S]*?)```/)
  if (!settingsMatch) return undefined
  
  try {
    const settingsJson = settingsMatch[1].trim()
    const parsed = JSON.parse(settingsJson)
    
    // Validate required fields
    if (!parsed.deviceSerial) return undefined
    
    return {
      deviceSerial: parsed.deviceSerial,
      streamConfigs: parsed.streamConfigs,
      optionChanges: parsed.optionChanges,
      explanation: parsed.explanation,
    }
  } catch (error) {
    console.warn('Failed to parse proposed settings:', error)
    return undefined
  }
}

/**
 * Generate code snippet for current configuration
 */
export function generateCodeSnippet(
  language: 'python' | 'cpp',
  device: { name: string; serial_number: string },
  streamConfigs: StreamConfig[],
  optionChanges?: ProposedSettings['optionChanges']
): string {
  const enabledConfigs = streamConfigs.filter(c => c.enable)
  
  if (language === 'python') {
    return generatePythonSnippet(device, enabledConfigs, optionChanges)
  } else {
    return generateCppSnippet(device, enabledConfigs, optionChanges)
  }
}

function generatePythonSnippet(
  device: { name: string; serial_number: string },
  configs: StreamConfig[],
  optionChanges?: ProposedSettings['optionChanges']
): string {
  const lines = [
    '# RealSense Camera Configuration',
    `# Device: ${device.name} (${device.serial_number})`,
    '',
    'import pyrealsense2 as rs',
    '',
    '# Create pipeline and config',
    'pipeline = rs.pipeline()',
    'config = rs.config()',
    '',
    `# Enable device by serial number`,
    `config.enable_device("${device.serial_number}")`,
    '',
  ]
  
  // Add stream configurations
  if (configs.length) {
    lines.push('# Configure streams')
    for (const c of configs) {
      const streamType = mapStreamType(c.stream_type)
      const format = mapFormat(c.format)
      lines.push(
        `config.enable_stream(rs.stream.${streamType}, ${c.resolution.width}, ${c.resolution.height}, rs.format.${format}, ${c.framerate})`
      )
    }
    lines.push('')
  }
  
  lines.push('# Start streaming')
  lines.push('profile = pipeline.start(config)')
  lines.push('')
  
  // Add option changes
  if (optionChanges?.length) {
    lines.push('# Configure sensor options')
    lines.push('device = profile.get_device()')
    lines.push('')
    for (const opt of optionChanges) {
      lines.push(`# Set ${opt.optionId}`)
      lines.push(`for sensor in device.sensors:`)
      lines.push(`    if sensor.name == "${opt.sensorId}":`)
      lines.push(`        sensor.set_option(rs.option.${snakeCase(opt.optionId)}, ${opt.value})`)
    }
    lines.push('')
  }
  
  lines.push('# Main loop')
  lines.push('try:')
  lines.push('    while True:')
  lines.push('        frames = pipeline.wait_for_frames()')
  
  if (configs.some(c => c.stream_type.toLowerCase() === 'depth')) {
    lines.push('        depth_frame = frames.get_depth_frame()')
  }
  if (configs.some(c => c.stream_type.toLowerCase() === 'color')) {
    lines.push('        color_frame = frames.get_color_frame()')
  }
  
  lines.push('        # Process frames...')
  lines.push('finally:')
  lines.push('    pipeline.stop()')
  
  return lines.join('\n')
}

function generateCppSnippet(
  device: { name: string; serial_number: string },
  configs: StreamConfig[],
  optionChanges?: ProposedSettings['optionChanges']
): string {
  const lines = [
    '// RealSense Camera Configuration',
    `// Device: ${device.name} (${device.serial_number})`,
    '',
    '#include <librealsense2/rs.hpp>',
    '#include <iostream>',
    '',
    'int main() try {',
    '    rs2::pipeline pipe;',
    '    rs2::config cfg;',
    '',
    `    // Enable device by serial number`,
    `    cfg.enable_device("${device.serial_number}");`,
    '',
  ]
  
  // Add stream configurations
  if (configs.length) {
    lines.push('    // Configure streams')
    for (const c of configs) {
      const streamType = mapStreamType(c.stream_type).toUpperCase()
      const format = mapFormat(c.format).toUpperCase()
      lines.push(
        `    cfg.enable_stream(RS2_STREAM_${streamType}, ${c.resolution.width}, ${c.resolution.height}, RS2_FORMAT_${format}, ${c.framerate});`
      )
    }
    lines.push('')
  }
  
  lines.push('    // Start streaming')
  lines.push('    rs2::pipeline_profile profile = pipe.start(cfg);')
  lines.push('')

  // Add option changes
  if (optionChanges?.length) {
    lines.push('    // Configure sensor options')
    lines.push('    rs2::device dev = profile.get_device();')
    lines.push('    for (auto& sensor : dev.query_sensors()) {')
    for (const opt of optionChanges) {
      lines.push(`        // Set ${opt.optionId} on ${opt.sensorId}`)
      lines.push(`        if (sensor.get_info(RS2_CAMERA_INFO_NAME) == std::string("${opt.sensorId}")) {`)
      lines.push(`            sensor.set_option(RS2_OPTION_${snakeCase(opt.optionId).toUpperCase()}, ${opt.value});`)
      lines.push(`        }`)
    }
    lines.push('    }')
    lines.push('')
  }
  
  lines.push('    // Main loop')
  lines.push('    while (true) {')
  lines.push('        rs2::frameset frames = pipe.wait_for_frames();')
  
  if (configs.some(c => c.stream_type.toLowerCase() === 'depth')) {
    lines.push('        rs2::depth_frame depth = frames.get_depth_frame();')
  }
  if (configs.some(c => c.stream_type.toLowerCase() === 'color')) {
    lines.push('        rs2::video_frame color = frames.get_color_frame();')
  }
  
  lines.push('        // Process frames...')
  lines.push('    }')
  lines.push('')
  lines.push('    return EXIT_SUCCESS;')
  lines.push('}')
  lines.push('catch (const rs2::error& e) {')
  lines.push('    std::cerr << "RealSense error: " << e.what() << std::endl;')
  lines.push('    return EXIT_FAILURE;')
  lines.push('}')
  
  return lines.join('\n')
}

function mapStreamType(type: string): string {
  const map: Record<string, string> = {
    'depth': 'depth',
    'color': 'color',
    'infrared': 'infrared',
    'gyro': 'gyro',
    'accel': 'accel',
    'pose': 'pose',
  }
  return map[type.toLowerCase()] || type.toLowerCase()
}

function mapFormat(format: string): string {
  const map: Record<string, string> = {
    'z16': 'z16',
    'rgb8': 'rgb8',
    'bgr8': 'bgr8',
    'y8': 'y8',
    'y16': 'y16',
    'yuyv': 'yuyv',
    'mjpeg': 'mjpeg',
  }
  return map[format.toLowerCase()] || format.toLowerCase()
}

function snakeCase(str: string): string {
  return str.replace(/\s+/g, '_').toLowerCase()
}
