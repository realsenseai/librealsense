import { describe, it, expect, vi } from 'vitest'
import { socketService } from '@/api/socket'

type FakeSocket = { connected: boolean; emit: ReturnType<typeof vi.fn> }

function withSocket(fake: FakeSocket) {
  // The service keeps its socket private; tests reach it the way the class does.
  ;(socketService as unknown as { socket: FakeSocket }).socket = fake
}

describe('socketService.request', () => {
  it('resolves with the server acknowledgement', async () => {
    const emit = vi.fn((_event: string, _data: unknown, ack: (r: unknown) => void) => ack({ depth: 1.5 }))
    withSocket({ connected: true, emit })

    await expect(socketService.request('depth_at_pixel', { x: 1 })).resolves.toEqual({ depth: 1.5 })
    expect(emit).toHaveBeenCalledWith('depth_at_pixel', { x: 1 }, expect.any(Function))
  })

  it('rejects at once when the socket is down, so callers fall back to REST', async () => {
    withSocket({ connected: false, emit: vi.fn() })
    await expect(socketService.request('depth_at_pixel', {})).rejects.toThrow('socket not connected')
  })

  it('rejects when no acknowledgement arrives in time', async () => {
    withSocket({ connected: true, emit: vi.fn() })
    await expect(socketService.request('depth_at_pixel', {}, 10)).rejects.toThrow('no reply')
  })
})
