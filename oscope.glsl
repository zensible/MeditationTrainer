precision mediump float;

uniform float iGlobalTime;
uniform sampler2D iChannel0;
uniform float yOffset;
const vec3 iResolution = vec3(256, 256, 1.0);

void main(void)
{
  vec2 uv = gl_FragCoord.xy / iResolution.xy;
  float horGrid = 10.0 * max(max(mod(uv.x, 0.1), mod(-uv.x, 0.1)), max(mod(uv.y, 0.1), mod(-uv.y, 0.1)));
    
  horGrid = pow(horGrid, 100.0);
  horGrid = mix(0.0, 0.1, horGrid);
  vec4 background = vec4(horGrid, horGrid, horGrid, 1.0);
    
  float mag = 1.0 - distance(uv.y, 0.5 + yOffset * sin(uv.x * 5.0 + iGlobalTime * 2.0));
  mag = pow(mag, 200.0);
  background += vec4(mag*uv.x, mag*uv.y, mag*sin(iGlobalTime / 2.0), 1.0);
    
  gl_FragColor = background;
}