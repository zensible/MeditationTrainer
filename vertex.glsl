attribute vec4 vPosition;

uniform float iGlobalTime;
uniform sampler2D iChannel0;
uniform float yOffset;
uniform vec2 iResolution;

void main()
{
  gl_Position = vPosition;
}