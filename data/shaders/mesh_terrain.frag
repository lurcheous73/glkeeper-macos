
#define DIFFUSE_TEXTURE tex_0

uniform sampler2D DIFFUSE_TEXTURE;

// passed from vertex shader
in vec2 Texcoord;
in vec4 FragColor;
in vec3 InPos;
in float Explored;

// result
out vec4 FinalColor;

// entry point
void main() 
{
    if (Explored < 0.5f)
    {
        // Shroud hides the terrain, but DK2 still lets the Keeper mark
        // diggable rock inside it.  Keep the per-tile selection/tag overlay
        // visible on top of black instead of swallowing it with the fog.
        FinalColor = vec4(FragColor.rgb, 1.0f);
        return;
    }
	vec4 texelColor = texture(DIFFUSE_TEXTURE, Texcoord);
    if (texelColor.a < 0.185f)
        discard; // translucency

    // temporary color correction 
    // >>
    FinalColor = texelColor * (smoothstep(-2.0f, 2.0f, InPos.y));
    FinalColor += FragColor; // addtitive
    // <<
}