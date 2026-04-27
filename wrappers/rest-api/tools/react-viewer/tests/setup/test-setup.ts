import '@testing-library/jest-dom'
import { cleanup } from '@testing-library/react'
import { afterEach, beforeAll, afterAll, vi } from 'vitest'

// Import MSW server - this sets up the lifecycle hooks automatically
import '../mocks/server'

// Cleanup after each test
afterEach(() => {
  cleanup()
})

// Mock Socket.IO before any imports
vi.mock('socket.io-client', () => ({
  io: vi.fn(() => ({
    on: vi.fn(),
    off: vi.fn(),
    emit: vi.fn(),
    connected: true,
    disconnect: vi.fn(),
    removeAllListeners: vi.fn(),
  })),
}))

// Mock WebRTC APIs that aren't available in jsdom
beforeAll(() => {
  // Mock RTCPeerConnection
  global.RTCPeerConnection = vi.fn().mockImplementation(() => ({
    createOffer: vi.fn().mockResolvedValue({ type: 'offer', sdp: 'mock-sdp' }),
    createAnswer: vi.fn().mockResolvedValue({ type: 'answer', sdp: 'mock-sdp' }),
    setLocalDescription: vi.fn().mockResolvedValue(undefined),
    setRemoteDescription: vi.fn().mockResolvedValue(undefined),
    addIceCandidate: vi.fn().mockResolvedValue(undefined),
    addTransceiver: vi.fn(),
    addTrack: vi.fn(),
    close: vi.fn(),
    ontrack: null,
    onicecandidate: null,
    oniceconnectionstatechange: null,
    onconnectionstatechange: null,
    ondatachannel: null,
    connectionState: 'new',
    iceConnectionState: 'new',
    localDescription: null,
    remoteDescription: null,
  })) as any

  // Mock RTCIceCandidate
  global.RTCIceCandidate = vi.fn().mockImplementation((init) => init) as any

  // Mock MediaStream
  global.MediaStream = vi.fn().mockImplementation(() => ({
    getTracks: vi.fn(() => []),
    addTrack: vi.fn(),
    removeTrack: vi.fn(),
    getVideoTracks: vi.fn(() => []),
    getAudioTracks: vi.fn(() => []),
  })) as any

  // Mock HTMLMediaElement play/pause
  if (typeof HTMLMediaElement !== 'undefined') {
    Object.defineProperty(HTMLMediaElement.prototype, 'play', {
      configurable: true,
      value: vi.fn().mockResolvedValue(undefined),
    })

    Object.defineProperty(HTMLMediaElement.prototype, 'pause', {
      configurable: true,
      value: vi.fn(),
    })
  }

  // Mock IntersectionObserver
  global.IntersectionObserver = vi.fn().mockImplementation(() => ({
    observe: vi.fn(),
    unobserve: vi.fn(),
    disconnect: vi.fn(),
  })) as any

  // Mock ResizeObserver
  global.ResizeObserver = vi.fn().mockImplementation(() => ({
    observe: vi.fn(),
    unobserve: vi.fn(),
    disconnect: vi.fn(),
  })) as any
})

afterAll(() => {
  vi.restoreAllMocks()
})
