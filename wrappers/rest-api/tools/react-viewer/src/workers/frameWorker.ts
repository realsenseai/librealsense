// Reads one stream's WebSocket off the main thread. Each message is
// [uint32 LE header length][header JSON][frame bytes][depth only: color table].
const decoder = new TextDecoder()

self.onmessage = ({ data: { url } }: MessageEvent<{ url: string }>) => {
  const ws = new WebSocket(url)
  ws.binaryType = 'arraybuffer'
  postMessage({ state: 'connecting' })
  ws.onopen = () => postMessage({ state: 'open' })
  ws.onclose = () => postMessage({ state: 'closed' })
  ws.onmessage = ({ data }: MessageEvent<ArrayBuffer>) => {
    const headerLength = new DataView(data).getUint32(0, true)
    const header = JSON.parse(decoder.decode(new Uint8Array(data, 4, headerLength)))
    const start = 4 + headerLength
    const end = data.byteLength - (header.color_table_size ?? 0)
    const pixels = data.slice(start, end) // a copy: Uint16Array needs an even offset
    const colorTable = header.color_table_size ? new Uint8Array(data, end) : undefined
    postMessage({ header, pixels, colorTable }, { transfer: [pixels, data] })
  }
}
