#include "app/App.hpp"
#include "util/Config.hpp"
#include "util/Theme.hpp"
#include "util/Font.hpp"

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <SDL.h>
#ifdef _WIN32
#include <windows.h>  // for SetProcessDPIAware
#endif
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL_opengles2.h>
#else
#include <SDL_opengl.h>
#endif

#include <cstdio>
#include <string>

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

#ifdef _WIN32
    SetProcessDPIAware();
#endif

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

    // OpenGL 3.0 core profile
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_Window* window = SDL_CreateWindow(
        "Pasgen",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1100, 700,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext gl_ctx = SDL_GL_CreateContext(window);
    if (!gl_ctx) {
        std::fprintf(stderr, "SDL_GL_CreateContext error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_MakeCurrent(window, gl_ctx);
    SDL_GL_SetSwapInterval(1); // vsync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // disable ini layout persistence

    // Bundled fonts live in a "fonts" directory next to the executable
    // (copied there at build time / installed there by `make install`).
    std::string fonts_dir;
    if (char* base = SDL_GetBasePath()) {
        fonts_dir = std::string(base) + "fonts/";
        SDL_free(base);
    }

    // Font (persisted in config; changeable from Settings > Preferences)
    pasgen::apply_font(pasgen::Config::instance().get_font_id(),
                        pasgen::Config::instance().get_font_size(),
                        fonts_dir);

    // Theme (persisted in config; changeable from Settings > Preferences)
    pasgen::apply_theme(pasgen::theme_from_string(pasgen::Config::instance().get_theme()));

    const char* glsl_version = "#version 130";
    ImGui_ImplSDL2_InitForOpenGL(window, gl_ctx);
    ImGui_ImplOpenGL3_Init(glsl_version);

    pasgen::App app;

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) app.request_quit();
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window))
                app.request_quit();
        }

        if (app.wants_quit()) running = false;

        // Rebuild the font atlas if the user changed the font in Preferences.
        // Must happen outside a NewFrame/Render pair, and the old GPU texture
        // has to be torn down before the atlas is repacked and re-uploaded.
        if (app.consume_font_dirty()) {
            ImGui_ImplOpenGL3_DestroyFontsTexture();
            pasgen::apply_font(app.font_id(), app.font_size(), fonts_dir);
            ImGui_ImplOpenGL3_CreateFontsTexture();
        }

        // Update window title to reflect db state
        {
            std::string title = "Pasgen";
            if (app.has_database()) {
                std::string name = app.db_name();
                if (!name.empty()) title += " — " + name;
                if (app.is_dirty())  title += " *";
            }
            SDL_SetWindowTitle(window, title.c_str());
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        int display_w, display_h;
        SDL_GetWindowSize(window, &display_w, &display_h);
        app.render(display_w, display_h);

        ImGui::Render();
        glViewport(0, 0, display_w, display_h);
        // Matches the active theme so resizing never flashes a foreign color.
        ImVec4 clear = app.background_color();
        glClearColor(clear.x, clear.y, clear.z, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
