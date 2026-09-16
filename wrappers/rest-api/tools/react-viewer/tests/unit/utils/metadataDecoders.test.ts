import { describe, it, expect } from 'vitest'
import { decodeBits, describeMetadata, formatMetadataValue, isDepthMappingDevice, metadataLabel } from '@/utils/metadataDecoders'

describe('metadata decoders', () => {
  it('prints bitmask attributes in hex and everything else as a number', () => {
    expect(formatMetadataValue('safety_hara_events', 255)).toBe('0xff')
    expect(formatMetadataValue('actual_exposure', 8500)).toBe('8500')
  })

  it('spells out enum-like fields', () => {
    expect(formatMetadataValue('safety_operational_mode', 1)).toBe('Standby')
    expect(formatMetadataValue('safety_smcu_debug_info_internal_state', 2)).toBe('RUN_SAFE_STATE')
    expect(formatMetadataValue('safety_operational_mode', 9)).toBe('9')
  })

  it('decodes set bits with their index and names the zero value', () => {
    expect(decodeBits('safety_vision_verdict', 0)).toBe('Depth Visual Safety Verdict: Safe')
    expect(decodeBits('safety_vision_verdict', 0b101)).toBe(
      'Depth Visual Safety Verdict: Not Safe (0), Collision(s) in warning zone (2)')
    expect(decodeBits('embedded_filters', 0b1010)).toBe('Embedded Filter Spatial (1), Holes Filling (3)')
    expect(decodeBits('actual_fps', 30)).toBeUndefined()
  })

  it('describes plain attributes and prefers the bit decoding for masks', () => {
    expect(describeMetadata('ACTUAL_FPS', 30000)).toMatch(/Hardware FPS/)
    expect(describeMetadata('safety_hara_events', 0)).toBe('HaRa events: No HaRa events identified')
    expect(describeMetadata('made_up', 1)).toBeUndefined()
  })

  it('renames manual white balance only for depth-mapping cameras', () => {
    expect(metadataLabel('manual_white_balance')).toBe('Manual White Balance')
    expect(metadataLabel('manual_white_balance', true)).toBe('White Balance')
    expect(isDepthMappingDevice('RealSense D585S')).toBe(true)
    expect(isDepthMappingDevice('RealSense D455')).toBe(false)
  })
})
