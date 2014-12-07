precision mediump float;

uniform float iGlobalTime;
uniform sampler2D iChannel0;
const vec3 iResolution = vec3(1024, 768, 1.0);

void main(void)
{
  vec2 uv = gl_FragCoord.xy / iResolution.xy;
  gl_FragColor = vec4(uv,0.5+0.5*sin(iGlobalTime),1.0);
}