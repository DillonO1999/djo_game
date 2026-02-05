#version 330
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec3 fragPosition; // Add this in your VS as well

uniform sampler2D texture0; // Grass
uniform sampler2D texture1; // Rock

out vec4 finalColor;

void main() {
    // 1. Calculate Triplanar Blending Weights
    vec3 blending = abs(fragNormal);
    blending /= (blending.x + blending.y + blending.z);

    // 2. Sample textures from 3 directions (using world position as UVs)
    float scale = 0.2; // Adjust for texture tiling size
    vec4 xTex = texture(texture1, fragPosition.zy * scale);
    vec4 yTex = texture(texture0, fragPosition.xz * scale);
    vec4 zTex = texture(texture1, fragPosition.xy * scale);

    // 3. Blend them together for the base color
    vec4 baseColor = xTex * blending.x + yTex * blending.y + zTex * blending.z;

    // 4. Slope-based tinting (still use the Y normal to force rock on cliffs)
    float slope = fragNormal.y;
    float blend = clamp((slope - 0.6) / (0.8 - 0.6), 0.0, 1.0);
    
    // Mix the triplanar result with a bias toward rock on steep slopes
    finalColor = mix(xTex * 0.5 + zTex * 0.5, baseColor, blend);
}