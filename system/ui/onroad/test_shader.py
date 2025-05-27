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

// Uniform inputs
uniform float time;

void main()
{
    if (fragTexCoord.x < 0.1) {
      finalColor = vec4(0.0, 0.0, 1.0, 1.0); // Black for out of bounds
      return;
    }
    // Create color bands based on position
    vec4 color = vec4(0.0);

    // Gradient from left to right (red to blue)
    color.r = 1.0 - fragTexCoord.x;
    color.b = fragTexCoord.x;

    // Add green pulsating effect based on time
    // color.g = abs(sin(time * 2.0)) * 0.5;

    // Add grid pattern
    if (mod(fragTexCoord.x * 20.0, 1.0) < 0.05 || mod(fragTexCoord.y * 20.0, 1.0) < 0.05) {
        color = vec4(1.0, 1.0, 1.0, 1.0) * abs(sin(time));
    }

    // Add a moving circle
    float circle_x = 0.5 + 0.4 * cos(time);
    float circle_y = 0.5 + 0.4 * sin(time);
    float dist = distance(fragTexCoord, vec2(circle_x, circle_y));
    if (dist < 0.1) {
        color = vec4(1.0, 1.0, 0.0, 1.0);  // Yellow circle
    }

    // Full opacity
    color.a = 1.0;

    finalColor = color;
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
  # Get the location of the uniform
  time_loc = rl.get_shader_location(shader, "time")
  assert(time_loc >= 0), "Shader uniform 'time' not found"

  # Set the time uniform value
  time_value = rl.ffi.new("float[]", [time])
  rl.set_shader_value(shader, time_loc, time_value, rl.SHADER_UNIFORM_FLOAT)  # SHADER_UNIFORM_FLOAT is 0

  # Draw with shader
  rl.begin_shader_mode(shader)
  rl.draw_rectangle(int(rect.x), int(rect.y), int(rect.width), int(rect.height), rl.WHITE)
  rl.end_shader_mode()


if __name__ == "__main__":
  gui_app.init_window("Shader Test")

  # Load custom shader
  shader = rl.load_shader_from_memory(vertex_shader, fragment_shader)

  # Initialize time
  time = 0.0

  for _ in gui_app.render():
    # Update time for animation
    time += rl.get_frame_time()

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

  # Unload shader before closing
  rl.unload_shader(shader)
