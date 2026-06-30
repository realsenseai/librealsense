import { apiClient } from './client'
import type { ICECandidate } from './types'

export class WebRTCHandler {
  private peerConnection: RTCPeerConnection | null = null
  private sessionId: string | null = null
  private deviceId: string
  private streamTypes: string[]
  private onTrack: (event: RTCTrackEvent) => void
  private onConnectionStateChange: (state: RTCPeerConnectionState) => void
  private iceCandidateQueue: RTCIceCandidate[] = []

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
    // Create peer connection
    this.peerConnection = new RTCPeerConnection({
      iceServers: [
        { urls: 'stun:stun.l.google.com:19302' },
        { urls: 'stun:stun1.l.google.com:19302' },
      ],
    })

    // Set up event handlers
    this.peerConnection.ontrack = this.onTrack

    this.peerConnection.onconnectionstatechange = () => {
      if (this.peerConnection) {
        this.onConnectionStateChange(this.peerConnection.connectionState)
      }
    }

    this.peerConnection.onicecandidate = async (event) => {
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

    this.peerConnection.oniceconnectionstatechange = () => {
      if (import.meta.env.DEV) console.log('ICE connection state:', this.peerConnection?.iceConnectionState)
    }

    // Add transceivers for receiving streams
    this.streamTypes.forEach((streamType) => {
      this.peerConnection?.addTransceiver('video', {
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

      // Set remote description (server's offer)
      await this.peerConnection.setRemoteDescription({
        type: serverOffer.type as RTCSdpType,
        sdp: serverOffer.sdp,
      })

      // Create and send answer
      const answer = await this.peerConnection.createAnswer()
      await this.peerConnection.setLocalDescription(answer)

      await apiClient.sendWebRTCAnswer(this.sessionId, answer)

      // Process any queued ICE candidates
      for (const candidate of this.iceCandidateQueue) {
        await this.peerConnection.addIceCandidate(candidate)
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
    if (this.peerConnection) {
      this.peerConnection.close()
      this.peerConnection = null
    }

    if (this.sessionId) {
      apiClient.closeWebRTCSession(this.sessionId).catch(console.error)
      this.sessionId = null
    }
  }

  get connectionState(): RTCPeerConnectionState | null {
    return this.peerConnection?.connectionState ?? null
  }
}
