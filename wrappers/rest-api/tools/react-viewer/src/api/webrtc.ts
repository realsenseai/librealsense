import { apiClient } from './client'
import type { ICECandidate } from './types'

/**
 * Sessions this page holds. A reload or a closed tab never unmounts the components, so the
 * server would keep every peer connection encoding frames until it notices the silence -
 * minutes later, and by then a long-lived server is crawling. Tell it on the way out.
 */
const liveSessions = new Set<string>()

if (typeof window !== 'undefined') {
  window.addEventListener('pagehide', () => {
    for (const sessionId of liveSessions) {
      // keepalive: the request must outlive the document
      void fetch(apiClient.webrtcSessionUrl(sessionId), { method: 'DELETE', keepalive: true }).catch(() => undefined)
    }
    liveSessions.clear()
  })
}

export class WebRTCHandler {
  private peerConnection: RTCPeerConnection | null = null
  private sessionId: string | null = null
  private deviceId: string
  private streamTypes: string[]
  private onTrack: (event: RTCTrackEvent) => void
  private onConnectionStateChange: (state: RTCPeerConnectionState) => void
  private iceCandidateQueue: RTCIceCandidate[] = []
  private closed = false

  constructor(
    deviceId: string,
    streamTypes: string[],
    onTrack: (event: RTCTrackEvent) => void,
    onConnectionStateChange: (state: RTCPeerConnectionState) => void
  ) {
    this.deviceId = deviceId
    this.streamTypes = streamTypes
    this.onTrack = onTrack
    this.onConnectionStateChange = onConnectionStateChange
  }

  async connect(): Promise<void> {
    // Create peer connection. Kept in a local so the negotiation sequence
    // below survives disconnect() nulling this.peerConnection mid-await.
    const pc = new RTCPeerConnection({
      iceServers: [
        { urls: 'stun:stun.l.google.com:19302' },
        { urls: 'stun:stun1.l.google.com:19302' },
      ],
    })
    this.peerConnection = pc

    // Set up event handlers
    pc.ontrack = this.onTrack

    pc.onconnectionstatechange = () => {
      if (this.peerConnection) {
        this.onConnectionStateChange(this.peerConnection.connectionState)
      }
    }

    pc.onicecandidate = async (event) => {
      if (event.candidate && this.sessionId) {
        try {
          await apiClient.addICECandidate(this.sessionId, {
            candidate: event.candidate.candidate,
            sdpMid: event.candidate.sdpMid || '',
            sdpMLineIndex: event.candidate.sdpMLineIndex || 0,
          })
        } catch (error) {
          console.error('Error sending ICE candidate:', error)
        }
      }
    }

    pc.oniceconnectionstatechange = () => {
      if (import.meta.env.DEV) console.log('ICE connection state:', pc.iceConnectionState)
    }

    // Add transceivers for receiving streams
    this.streamTypes.forEach((streamType) => {
      pc.addTransceiver('video', {
        direction: 'recvonly',
        streams: [new MediaStream()],
      })
      if (import.meta.env.DEV) console.log(`Added transceiver for ${streamType}`)
    })

    try {
      // Get offer from server
      const serverOffer = await apiClient.createWebRTCOffer({
        device_id: this.deviceId,
        stream_types: this.streamTypes,
      })

      this.sessionId = serverOffer.session_id
      liveSessions.add(this.sessionId)

      // disconnect() may have run while the offer was in flight (React
      // StrictMode remounts every effect in dev). Without this the session id
      // is only learned after the handler is already dead, so the server keeps
      // an orphan peer connection encoding frames until its 1-hour sweep.
      if (this.closed) {
        this.releaseSession()
        return
      }

      // Set remote description (server's offer)
      await pc.setRemoteDescription({
        type: serverOffer.type as RTCSdpType,
        sdp: serverOffer.sdp,
      })

      // Create and send answer
      const answer = await pc.createAnswer()
      await pc.setLocalDescription(answer)

      await apiClient.sendWebRTCAnswer(this.sessionId, answer)

      // Process any queued ICE candidates
      for (const candidate of this.iceCandidateQueue) {
        await pc.addIceCandidate(candidate)
      }
      this.iceCandidateQueue = []

      // Poll for server ICE candidates
      this.pollICECandidates()
    } catch (error) {
      console.error('WebRTC connection error:', error)
      throw error
    }
  }

  private async pollICECandidates(): Promise<void> {
    if (!this.sessionId || !this.peerConnection) return

    try {
      const candidates = await apiClient.getICECandidates(this.sessionId)
      for (const candidate of candidates) {
        await this.addRemoteICECandidate(candidate)
      }
    } catch (error) {
      console.error('Error polling ICE candidates:', error)
    }

    // Continue polling if connection is still being established
    if (
      this.peerConnection?.iceConnectionState === 'checking' ||
      this.peerConnection?.iceConnectionState === 'new'
    ) {
      setTimeout(() => this.pollICECandidates(), 500)
    }
  }

  async addRemoteICECandidate(candidate: ICECandidate): Promise<void> {
    const iceCandidate = new RTCIceCandidate({
      candidate: candidate.candidate,
      sdpMid: candidate.sdpMid,
      sdpMLineIndex: candidate.sdpMLineIndex,
    })

    if (this.peerConnection?.remoteDescription) {
      await this.peerConnection.addIceCandidate(iceCandidate)
    } else {
      this.iceCandidateQueue.push(iceCandidate)
    }
  }

  disconnect(): void {
    this.closed = true

    if (this.peerConnection) {
      this.peerConnection.close()
      this.peerConnection = null
    }

    this.releaseSession()
  }

  private releaseSession(): void {
    if (this.sessionId) {
      liveSessions.delete(this.sessionId)
      apiClient.closeWebRTCSession(this.sessionId).catch(console.error)
      this.sessionId = null
    }
  }

  get connectionState(): RTCPeerConnectionState | null {
    return this.peerConnection?.connectionState ?? null
  }
}
