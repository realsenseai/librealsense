/** GPU unprojection of a z16 depth image, a port of the SDK's pointcloud-gl shaders
 * (src/gl/pc-shader.cpp): occlusion invalidation, per-vertex normals, texture mapping through
 * the depth->texture extrinsics, and the three-light diffuse term. */

export const DISTORTION_NONE = 0
export const DISTORTION_BROWN = 1 // brown_conrady, modified_brown_conrady, inverse_brown_conrady all project alike

// Raw GLSL ES 3.00 (RawShaderMaterial): three.js adds only the #version line.
export const vertexShader = /* glsl */ `
precision highp float;
precision highp int;
precision highp usampler2D;

in vec3 position;          // pixel coordinates (u, v, 0)
uniform mat4 modelViewMatrix;
uniform mat4 projectionMatrix;

uniform usampler2D depthTex;
uniform ivec2 imageSize;
uniform float units;
uniform vec4 depthK;      // fx, fy, ppx, ppy of the depth stream
uniform int hasTexture;
uniform vec4 texK;        // fx, fy, ppx, ppy of the texture stream
uniform vec2 texSize;
uniform float texCoeffs[5];
uniform int texModel;
uniform mat3 extRot;      // depth -> texture
uniform vec3 extTrans;
uniform float minDeltaZ;
uniform int occlusion;
uniform float pointSize;

out float vInvalid;
out vec2 vUv;
out vec3 vNormal;
out vec3 vPos;

vec3 unproject(ivec2 px) {
  float d = float(texelFetch(depthTex, px, 0).r) * units;
  return vec3((float(px.x) - depthK.z) / depthK.x * d, (float(px.y) - depthK.w) / depthK.y * d, d);
}

vec2 project(vec3 p) {
  float x = p.x / p.z;
  float y = p.y / p.z;
  if (texModel == 1) {
    float r2 = x * x + y * y;
    float f = 1.0 + texCoeffs[0] * r2 + texCoeffs[1] * r2 * r2 + texCoeffs[4] * r2 * r2 * r2;
    float xf = x * f;
    float yf = y * f;
    float dx = xf + 2.0 * texCoeffs[2] * x * y + texCoeffs[3] * (r2 + 2.0 * x * x);
    float dy = yf + 2.0 * texCoeffs[3] * x * y + texCoeffs[2] * (r2 + 2.0 * y * y);
    x = dx;
    y = dy;
  }
  return vec2(x * texK.x + texK.z, y * texK.y + texK.w);
}

void main() {
  ivec2 px = ivec2(position.xy);
  vec3 p = unproject(px);
  ivec2 l = ivec2(max(px.x - 1, 0), px.y);
  ivec2 r = ivec2(min(px.x + 1, imageSize.x - 1), px.y);
  ivec2 t = ivec2(px.x, max(px.y - 1, 0));
  ivec2 b = ivec2(px.x, min(px.y + 1, imageSize.y - 1));
  vec3 pl = unproject(l);
  vec3 pr = unproject(r);
  vec3 pt = unproject(t);
  vec3 pb = unproject(b);

  vec3 axis1 = normalize(mix(pr - p, p - pl, 0.5));
  vec3 axis2 = normalize(mix(pt - p, p - pb, 0.5));
  vNormal = cross(axis1, axis2);

  vInvalid = 0.0;
  if (abs(p.z) < 0.01) vInvalid = 1.0;
  if (occlusion == 1) {
    if (abs(pl.z - p.z) > minDeltaZ * p.z) vInvalid = 1.0;
    if (abs(pr.z - p.z) > minDeltaZ * p.z) vInvalid = 1.0;
    if (abs(pt.z - p.z) > minDeltaZ * p.z) vInvalid = 1.0;
    if (abs(pb.z - p.z) > minDeltaZ * p.z) vInvalid = 1.0;
  }

  vUv = vec2(-1.0);
  if (hasTexture == 1) {
    vec3 q = extRot * p + extTrans;
    vUv = project(q) / texSize;
    if (vUv.x < 0.0 || vUv.y < 0.0 || vUv.x >= 1.0 || vUv.y >= 1.0) vInvalid = 1.0;
  }

  vPos = p;
  // The SDK looks down +Z with Y down; three.js looks down -Z with Y up.
  vec3 scene = vec3(p.x, -p.y, -p.z);
  if (vInvalid > 0.0) {
    gl_Position = vec4(1.0, 1.0, 1.0, 0.0);
  } else {
    gl_Position = projectionMatrix * modelViewMatrix * vec4(scene, 1.0);
  }
  gl_PointSize = pointSize;
}
`

export const fragmentShader = /* glsl */ `
precision highp float;
precision highp int;

in float vInvalid;
in vec2 vUv;
in vec3 vNormal;
in vec3 vPos;

uniform sampler2D colorTex;
uniform int hasTexture;
uniform int shaded;
uniform vec2 depthRange;

layout(location = 0) out vec4 outColor;

// The 2D view's "jet" legend: blue (near) -> cyan -> yellow -> red -> dark red (far)
vec3 jet(float t) {
  t = clamp(t, 0.0, 1.0);
  vec3 c0 = vec3(0.0, 0.0, 1.0);
  vec3 c1 = vec3(0.0, 1.0, 1.0);
  vec3 c2 = vec3(1.0, 1.0, 0.0);
  vec3 c3 = vec3(1.0, 0.0, 0.0);
  vec3 c4 = vec3(50.0 / 255.0, 0.0, 0.0);
  float s = t * 4.0;
  if (s < 1.0) return mix(c0, c1, s);
  if (s < 2.0) return mix(c1, c2, s - 1.0);
  if (s < 3.0) return mix(c2, c3, s - 2.0);
  return mix(c3, c4, s - 3.0);
}

void main() {
  if (vInvalid > 0.0) discard;
  vec3 color = hasTexture == 1
    ? texture(colorTex, vec2(vUv.x, 1.0 - vUv.y)).rgb
    : jet((vPos.z - depthRange.x) / (depthRange.y - depthRange.x));
  if (shaded == 1) {
    vec3 light0 = vec3(0.0, 1.0, 0.0);
    vec3 light1 = vec3(0.5, -1.0, 0.0);
    vec3 light2 = vec3(-0.5, -1.0, 0.0);
    float d0 = max(dot(vNormal, light0 - vPos), 0.0);
    float d1 = max(dot(vNormal, light1 - vPos), 0.0);
    float d2 = max(dot(vNormal, light2 - vPos), 0.0);
    float diffuse = 0.6 + d0 * 0.2 + d1 * 0.2 + d2 * 0.2;
    color *= clamp(diffuse, 0.0, 1.0);
  }
  outColor = vec4(color, 1.0);
}
`
