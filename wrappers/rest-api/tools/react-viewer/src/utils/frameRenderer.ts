import type { Frame } from '../api/frames'

// Draws SDK frames as they are, with WebGL2. Depth is colored on the GPU by indexing its color table
// (256x256 texels, one per depth value) with each pixel's value.
const VERTEX = `#version 300 es
out vec2 uv;
void main() {
  vec2 p = vec2(gl_VertexID & 1, gl_VertexID >> 1);
  uv = vec2(p.x, 1.0 - p.y);
  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}`

const FRAGMENT = `#version 300 es
precision highp float;
precision highp usampler2D;
uniform int mode; // 0 RGB(A) or gray 8 bit, 1 gray 16 bit, 2 depth, 3 YUYV, 4 BGR(A)
uniform sampler2D image;
uniform usampler2D values;
uniform sampler2D colorTable;
in vec2 uv;
out vec4 color;
void main() {
  if (mode == 2) {
    uint d = texture(values, uv).r;
    color = vec4(texelFetch(colorTable, ivec2(d & 255u, d >> 8), 0).rgb, 1.0);
  } else if (mode == 1) {
    color = vec4(vec3(float(texture(values, uv).r) / 65535.0), 1.0);
  } else if (mode == 3) {
    // Each pixel is [Y, U] or [Y, V]: a pair shares its U and V (src/gl/yuy2rgb-gl.cpp)
    ivec2 p = ivec2(uv * vec2(textureSize(image, 0)));
    int even = p.x & ~1;
    float y = texelFetch(image, p, 0).r, u = texelFetch(image, ivec2(even, p.y), 0).g - 0.5;
    float v = texelFetch(image, ivec2(even + 1, p.y), 0).g - 0.5;
    color = vec4(y + 1.402 * v, y - 0.34414 * u - 0.71414 * v, y + 1.772 * u, 1.0); // the canvas clamps to 0-1
  } else {
    vec4 c = texture(image, uv);
    color = vec4(mode == 4 ? c.bgr : c.rgb, 1.0);
  }
}`

interface Layout { mode: number; internal: number; format: number; type: number }

function layoutOf(gl: WebGL2RenderingContext, format: string): Layout | null {
  switch (format) {
    case 'rgb8': return { mode: 0, internal: gl.RGB8, format: gl.RGB, type: gl.UNSIGNED_BYTE }
    case 'bgr8': return { mode: 4, internal: gl.RGB8, format: gl.RGB, type: gl.UNSIGNED_BYTE }
    case 'rgba8': return { mode: 0, internal: gl.RGBA8, format: gl.RGBA, type: gl.UNSIGNED_BYTE }
    case 'bgra8': return { mode: 4, internal: gl.RGBA8, format: gl.RGBA, type: gl.UNSIGNED_BYTE }
    case 'y8': return { mode: 0, internal: gl.LUMINANCE, format: gl.LUMINANCE, type: gl.UNSIGNED_BYTE }
    // UYVY like legacy: each [U or V, Y] read as one 16-bit gray, Y the high byte (luminance only)
    case 'y16': case 'uyvy': return { mode: 1, internal: gl.R16UI, format: gl.RED_INTEGER, type: gl.UNSIGNED_SHORT }
    case 'z16': return { mode: 2, internal: gl.R16UI, format: gl.RED_INTEGER, type: gl.UNSIGNED_SHORT }
    case 'yuyv': return { mode: 3, internal: gl.RG8, format: gl.RG, type: gl.UNSIGNED_BYTE }
    default: return null
  }
}

export class FrameRenderer {
  private gl: WebGL2RenderingContext
  private program: WebGLProgram
  private textures: { image: WebGLTexture; values: WebGLTexture; colorTable: WebGLTexture }

  constructor(private canvas: HTMLCanvasElement) {
    const gl = canvas.getContext('webgl2')
    if (!gl) throw new Error('WebGL2 is not available')
    this.gl = gl
    this.program = gl.createProgram()!
    for (const [type, source] of [[gl.VERTEX_SHADER, VERTEX], [gl.FRAGMENT_SHADER, FRAGMENT]] as const) {
      const shader = gl.createShader(type)!
      gl.shaderSource(shader, source)
      gl.compileShader(shader)
      if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) throw new Error(gl.getShaderInfoLog(shader) ?? 'shader')
      gl.attachShader(this.program, shader)
    }
    gl.linkProgram(this.program)
    gl.useProgram(this.program)
    const texture = (unit: number, name: string) => {
      const t = gl.createTexture()!
      gl.activeTexture(gl.TEXTURE0 + unit)
      gl.bindTexture(gl.TEXTURE_2D, t)
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.NEAREST)  // integer textures can't filter
      gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.NEAREST)
      gl.uniform1i(gl.getUniformLocation(this.program, name), unit)
      return t
    }
    this.textures = { image: texture(0, 'image'), values: texture(1, 'values'), colorTable: texture(2, 'colorTable') }
  }

  /** Frees the WebGL context now: a page may only hold a few at once. */
  dispose() {
    this.gl.getExtension('WEBGL_lose_context')?.loseContext()
  }

  /** Draws the frame; false if its format can't be drawn. */
  draw({ header, pixels, colorTable }: Frame): boolean {
    const gl = this.gl
    const layout = layoutOf(gl, header.format)
    const { width = 0, height = 0, stride = 0, bpp = 1 } = header // every format drawn here is video
    if (!layout || (layout.mode === 2 && !colorTable)) return false
    if (this.canvas.width !== width || this.canvas.height !== height) {
      this.canvas.width = width
      this.canvas.height = height
    }
    gl.viewport(0, 0, width, height)
    gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1)
    gl.pixelStorei(gl.UNPACK_ROW_LENGTH, stride / bpp)
    const integer = layout.type === gl.UNSIGNED_SHORT
    gl.activeTexture(gl.TEXTURE0 + (integer ? 1 : 0))
    gl.bindTexture(gl.TEXTURE_2D, integer ? this.textures.values : this.textures.image)
    const data = integer ? new Uint16Array(pixels) : new Uint8Array(pixels)
    gl.texImage2D(gl.TEXTURE_2D, 0, layout.internal, width, height, 0, layout.format, layout.type, data)
    if (layout.mode === 2) {
      gl.pixelStorei(gl.UNPACK_ROW_LENGTH, 0)
      gl.activeTexture(gl.TEXTURE2)
      gl.bindTexture(gl.TEXTURE_2D, this.textures.colorTable)
      gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGB8, 256, 256, 0, gl.RGB, gl.UNSIGNED_BYTE, colorTable!)
    }
    gl.uniform1i(gl.getUniformLocation(this.program, 'mode'), layout.mode)
    gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4)
    return true
  }
}
