import pyray as rl
from openpilot.system.ui.lib.application import gui_app


# Define fragment shader code
# fragment_shader = """
# #version 330

# // Input vertex attributes (from vertex shader)
# in vec2 fragTexCoord;
# in vec4 fragColor;

# // Output fragment color
# out vec4 finalColor;

# // Uniform inputs
# uniform float time;

# void main()
# {
#     // Create color bands based on position
#     vec4 color = vec4(0.0);

#     // Gradient from left to right (red to blue)
#     color.r = 1.0 - fragTexCoord.x;
#     color.b = fragTexCoord.x;

#     // Add green pulsating effect based on time
#     color.g = abs(sin(time * 2.0)) * 0.5;

#     // Add grid pattern
#     if (mod(fragTexCoord.x * 20.0, 1.0) < 0.05 || mod(fragTexCoord.y * 20.0, 1.0) < 0.05) {
#         color = vec4(1.0, 1.0, 1.0, 1.0) * abs(sin(time));
#     }

#     // Add a moving circle
#     float circle_x = 0.5 + 0.4 * cos(time);
#     float circle_y = 0.5 + 0.4 * sin(time);
#     float dist = distance(fragTexCoord, vec2(circle_x, circle_y));
#     if (dist < 0.1) {
#         color = vec4(1.0, 1.0, 0.0, 1.0);  // Yellow circle
#     }

#     // Full opacity
#     color.a = 1.0;

#     finalColor = color;
# }
# """

fragment_shader = """
#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;

// Output fragment color
out vec4 finalColor;

void main()
{
    // Display texture coordinates as colors for debugging
    finalColor = vec4(fragTexCoord.x, fragTexCoord.y, 0.0, 1.0);

    // Add grid lines to visualize coordinate space
    if (mod(fragTexCoord.x * 10.0, 1.0) < 0.05 || mod(fragTexCoord.y * 10.0, 1.0) < 0.05) {
        finalColor = vec4(1.0, 1.0, 1.0, 1.0);  // White grid
    }

    // Original split logic
    if (fragTexCoord.x < 0.5) {
        // This should make the left half blue
        finalColor = vec4(0.0, 0.0, 1.0, 1.0);
    } else {
        // This should make the right half red
        finalColor = vec4(1.0, 0.0, 0.0, 1.0);
    }
}
"""
# Default vertex shader (no modifications needed)
vertex_shader = """
#version 330

// Input vertex attributes
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;

// Output vertex attributes (to fragment shader)
out vec2 fragTexCoord;
out vec4 fragColor;

// Uniform inputs
uniform mat4 mvp;

void main()
{
    // Pass texture coordinates and color to fragment shader
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;

    // Calculate final vertex position
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
"""


def shader_render(shader, rect, time):
    # Create a white texture
    white_img = rl.gen_image_color(2, 2, rl.WHITE)
    white_texture = rl.load_texture_from_image(white_img)

    # Begin shader mode
    rl.begin_shader_mode(shader)

    # Draw the texture with proper texture coordinates
    source_rect = rl.Rectangle(0, 0, 2, 2)  # Full texture
    dest_rect = rect

    # This function correctly maps texture coords from source to dest
    rl.draw_texture_pro(
        white_texture,
        source_rect,
        dest_rect,
        rl.Vector2(0, 0),  # No offset
        0.0,               # No rotation
        rl.WHITE           # No tint
    )

    # End shader mode
    rl.end_shader_mode()

    # Clean up
    rl.unload_texture(white_texture)
    rl.unload_image(white_img)

if __name__ == "__main__":
  gui_app.init_window("Shader Test")

  # Load custom shader
  shader = rl.load_shader_from_memory(vertex_shader, fragment_shader)

  # Initialize time
  time = 0.0

  for _ in gui_app.render():
    # Update time for animation
    time += rl.get_frame_time() * 1.0  # Double the speed

    # Clear background
    rl.clear_background(rl.BLACK)

    # Render with our shader
    # Create a 500x500 rectangle centered in the window
    window_width = rl.get_screen_width()
    window_height = rl.get_screen_height()
    rect_size = 500
    rect = rl.Rectangle(
      (window_width - rect_size) / 2,  # x position centered
      (window_height - rect_size) / 2,  # y position centered
      rect_size,
      rect_size
    )
    # shader_render(shader, rect, time)
    shader_render(shader, rl.Rectangle(0, 0, gui_app.width, gui_app.height), time)

    # Add some text
    rl.draw_text("Custom Shader Demo", 10, 10, 20, rl.WHITE)
    rl.draw_text("Press ESC to exit", 10, 40, 15, rl.GRAY)
    rl.draw_text(f"Time: {time:.2f}", 10, 70, 15, rl.WHITE)

  # Unload shader before closing
  rl.unload_shader(shader)
