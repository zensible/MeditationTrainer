precision mediump float;

vec3 COLOR1 = vec3(0.0, 0.0, 0.3);
vec3 COLOR2 = vec3(0.5, 0.0, 0.0);
float BLOCK_WIDTH = 0.01;

uniform float yOffset;
uniform float iGlobalTime;
uniform sampler2D iChannel0;
uniform vec2 iResolution;

void main(void)
{
  vec2 uv = gl_FragCoord.xy / iResolution.xy;
  
  // To create the BG pattern
  vec3 final_color = vec3(1.0);
  vec3 bg_color = vec3(0.0);
  vec3 wave_color = vec3(0.0);

  bg_color = vec3(0.0, 0.0, 0.0);
  
  // To create the waves
  float wave_width = 0.01;
  uv  = -1.0 + 2.0 * uv;
  uv.y += 0.1;
  for(float i = 0.0; i < 10.0; i++) {
    
    uv.y += (0.07 * sin(uv.x + i/7.0 + iGlobalTime ));
    wave_width = abs(1.0 / (150.0 * uv.y));
    wave_color += vec3(wave_width * 1.9, wave_width, wave_width * 1.5);
  }
  
  final_color = bg_color + wave_color;
  
  gl_FragColor = vec4(final_color, 1.0);
}