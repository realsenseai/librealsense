import { describe, it, expect, beforeEach } from 'vitest'
import { http, HttpResponse } from 'msw'
import { server } from '../../mocks/server'

/**
 * A reload never unmounts the viewer's components, so the only chance to release the camera
 * sessions is the page-unload event; a server left holding them keeps encoding frames for
 * every tab that ever streamed.
 */
describe('WebRTC sessions and the page going away', () => {
  beforeEach(() => {
    document.body.innerHTML = ''
  })

  it('closes the sessions this page opened when the page hides', async () => {
    const closed: string[] = []
    server.use(
      http.post('/api/v1/webrtc/offer', () =>
        HttpResponse.json({ session_id: 'session-1', sdp: 'v=0', type: 'offer' })),
      http.post('/api/v1/webrtc/answer', () => HttpResponse.json({ success: true })),
      http.get('/api/v1/webrtc/sessions/:id/ice-candidates', () => HttpResponse.json([])),
      http.delete('/api/v1/webrtc/sessions/:id', ({ params }) => {
        closed.push(params.id as string)
        return HttpResponse.json({ success: true })
      }),
    )

    const { WebRTCHandler } = await import('@/api/webrtc')
    const handler = new WebRTCHandler('device1', ['depth'], () => undefined, () => undefined)
    await handler.connect().catch(() => undefined) // jsdom has no real RTCPeerConnection

    window.dispatchEvent(new Event('pagehide'))
    await new Promise((resolve) => setTimeout(resolve, 50))

    expect(closed).toContain('session-1')
  })
})
