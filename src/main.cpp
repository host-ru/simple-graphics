#include "core/model.hpp"
#include "core/resource_manager.hpp"

#include "settings.hpp"
#include "utils/GL.hpp"
#include "utils/SDL.hpp"
#include "utils/imgui.hpp"
#include "utils/timer.hpp"
#include <glm/gtx/transform.hpp>
#include <string_view>

auto main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[]) -> int try {
  auto sdl = sdl2::SDL(SDL_INIT_VIDEO);

  // Set up OpenGL attributes
  sdl2::gl_setAttributes(
      {{SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE},
       {SDL_GL_CONTEXT_MAJOR_VERSION, settings::opengl_version.major},
       {SDL_GL_CONTEXT_MINOR_VERSION, settings::opengl_version.minor},
       {SDL_GL_STENCIL_SIZE, settings::stencil_size}});

  // Enable 4x Antialiasing
  sdl2::gl_setAttributes(
      {{SDL_GL_MULTISAMPLEBUFFERS, settings::multisample.buffers},
       {SDL_GL_MULTISAMPLESAMPLES, settings::multisample.samples}});

  // Create window
  auto window_flags = static_cast<unsigned int>(SDL_WINDOW_OPENGL) |
                      static_cast<unsigned int>(SDL_WINDOW_RESIZABLE);
  window_flags |= settings::fullscreen;
  auto window = sdl2::unique_ptr<SDL_Window>(SDL_CreateWindow(
      "Simple graphics engine", settings::window_position.x,
      settings::window_position.y, settings::window_resolution.w,
      settings::window_resolution.h, window_flags));
  auto context = sdl2::SDL_Context(window);

  // Enable GLEW and SDL_image
  glewExperimental = GL_TRUE;
  glewInit();
  auto sdl_image = sdl2::SDL_image(IMG_INIT_PNG);

  // Enable VSync
  if (SDL_GL_SetSwapInterval(1) == -1) {
    std::cerr << "Error enabling VSync!\n";
  };

  // Enable depth test and antialiasing
  gl::enable({GL_DEPTH_TEST, GL_MULTISAMPLE});
  glDepthFunc(GL_LESS);

  // Initialize ImGui
  auto imgui = imgui::Imgui(window, context);

  // Create Vertex Array Object
  // Single VAO for entire application
  auto vao = gl::VertexArrayObject();

  // Create resource manager
  auto resource_manager = ResourceManager();

  // Loading resources
  resource_manager.load_shaders("./src/shaders/shader.vert",
                                "./src/shaders/shader.frag");
  resource_manager.load_model("./resources/AK-47.fbx",
                              "./resources/textures/Ak-47_Albedo.png");
  resource_manager.load_model("./resources/lowpoly_city_triangulated.obj");

  // Setting up the demo scene
  constexpr auto fov = glm::radians(45.0);
  constexpr double z_near = 0.1;
  constexpr double z_far = 100.0;

  constexpr auto camera_position = glm::dvec3(12, 9, 9);
  constexpr auto scene_center = glm::dvec3(0);
  constexpr auto up_direction = glm::dvec3(0, 1, 0);

  constexpr auto identity_matrix = glm::dmat4(1.0);
  constexpr double model1_scale = 10.0;
  constexpr auto model1_scale_matrix =
      glm::dvec3(model1_scale, model1_scale, model1_scale);
  constexpr double model2_scale = 0.001;
  constexpr auto model2_scale_matrix =
      glm::dvec3(model2_scale, model2_scale, model2_scale);

  constexpr auto model2_offset = glm::dvec3(0.0, 4.0, 0.0);

  // Calculate MVP matrix
  glm::dmat4 view_matrix =
      glm::lookAt(camera_position, scene_center, up_direction);
  auto model1_matrix = glm::scale(identity_matrix, model1_scale_matrix);
  auto model2_matrix = glm::scale(identity_matrix, model2_scale_matrix);

  auto &models = resource_manager.get_models();

  auto aspect_ratio = settings::window_resolution.w /
                      static_cast<double>(settings::window_resolution.h);

  // Game loop
  SDL_Event e;
  bool should_quit = false;
  auto t_start = std::chrono::high_resolution_clock::now();

  while (!should_quit) {
    // Check for window events
    if (SDL_PollEvent(&e) != 0) {
      ImGui_ImplSDL2_ProcessEvent(&e);
      switch (e.type) {
      case SDL_QUIT:
        // Handle app quit
        should_quit = true;
        break;
      case SDL_WINDOWEVENT:
        switch (e.window.event) {
        case SDL_WINDOWEVENT_SIZE_CHANGED:
          // Handle window resize
          glViewport(0, 0, e.window.data1, e.window.data2);
          if (!static_cast<bool>(settings::fullscreen)) {
            settings::window_resolution = {e.window.data1, e.window.data2};
          }
          aspect_ratio = e.window.data1 / static_cast<double>(e.window.data2);
          break;
        }
      case SDL_KEYDOWN:
        switch (e.key.keysym.sym) {
        case SDLK_RETURN:
          if (static_cast<bool>(e.key.keysym.mod &
                                (static_cast<unsigned int>(KMOD_LALT) |
                                 static_cast<unsigned int>(KMOD_RALT)))) {
            // Handle fullscreen toggle
            switch (settings::fullscreen) {
            case 0:
              settings::fullscreen = SDL_WINDOW_FULLSCREEN;
              SDL_DisplayMode dm;
              SDL_GetDesktopDisplayMode(0, &dm);
              SDL_SetWindowSize(window.get(), dm.w, dm.h);
              SDL_SetWindowFullscreen(window.get(), settings::fullscreen);
              break;
            case SDL_WINDOW_FULLSCREEN:
            case SDL_WINDOW_FULLSCREEN_DESKTOP:
              settings::fullscreen = 0;
              SDL_SetWindowFullscreen(window.get(), settings::fullscreen);
              SDL_SetWindowSize(window.get(), settings::window_resolution.w,
                                settings::window_resolution.h);
              break;
            default:
              break;
            }
          }
          break;
        }
        break;
      }
    }

    // Clear the screen to black
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(static_cast<unsigned int>(GL_COLOR_BUFFER_BIT) |
            static_cast<unsigned int>(GL_DEPTH_BUFFER_BIT));

    // Calculate new model mvp matrices
    glm::dmat4 projection_matrix =
        glm::perspective(fov, aspect_ratio, z_near, z_far);
    glm::dmat4 mvp_matrix1 = projection_matrix * view_matrix * model1_matrix;
    glm::dmat4 mvp_matrix2 = projection_matrix *
                             glm::translate(view_matrix, model2_offset) *
                             model2_matrix;

    auto t_now = std::chrono::high_resolution_clock::now();
    auto time =
        std::chrono::duration_cast<std::chrono::microseconds>(t_now - t_start);

    constexpr double time_delta = 0.001 * 0.001 * 0.1;
    constexpr double pi_rad = 180.0;
    constexpr auto axis = glm::dvec3(0.0, 1.0, 0.0);

    auto rotation_time = time.count() * time_delta;
    auto rotated_mvp1 =
        glm::rotate(mvp_matrix1, rotation_time * glm::radians(pi_rad), axis);
    models.at(0).set_mvp_matrix(rotated_mvp1);
    models.at(1).set_mvp_matrix(mvp_matrix2);

    // Render all the models
    resource_manager.render_all();

    // Start the Dear ImGui frame
    imgui.create_frame(window);

    // Imgui Code
    ImGui::ShowDemoWindow();

    // Render ImGui
    imgui.render();

    // Swap window
    SDL_GL_SwapWindow(window.get());
  }
  return 0;
} catch (const std::exception &e) {
  std::cerr << e.what() << '\n';
  return 1;
}
