#include "Film.h"
#include "Window.h"

#include <IMGUI/imgui.h>
#include <IMGUI/imgui_impl_sdl2.h>
#include <IMGUI/imgui_impl_opengl3.h>

#include <iostream>

#undef main
int main()
{
	int _width = 800;
	int _height = 600;

	Film film(_width, _height);
	Window window(_width, _height, "Tracer");

	// Setting up the GUI system
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImGui::StyleColorsDark();

	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	const char* glslVersion = "#version 430";
	ImGui_ImplSDL2_InitForOpenGL(window.GetSDLWindow(), window.GetGLContext());
	ImGui_ImplOpenGL3_Init(glslVersion);

	bool running = true;
	int i = 0;
	while (running)
	{
		std::cout << i++ << std::endl;
		SDL_Event event;
		if (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.type == SDL_QUIT)
			{
				running = false;
			}
			else if (event.type == SDL_WINDOWEVENT)
			{
				// User clicked the X on a window
				if (event.window.event == SDL_WINDOWEVENT_CLOSE)
				{
					running = false;
				}
				else if (event.window.event == SDL_WINDOWEVENT_RESIZED)
				{
					_width = event.window.data1;
					_height = event.window.data2;
					film.Resize(_width, _height);
					window.Resize(_width, _height);
				}
			}
		}

		{
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplSDL2_NewFrame();
			ImGui::NewFrame();

			ImGui::Begin("Scene Controls");

			if (ImGui::Button("Test button"))
			{
				std::cout << "Test button pressed" << std::endl;
			}

			ImGui::End();
		}

		window.DrawScreen(film.ResolveToRGBA8());

		// Draws the GUI
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// Free floating ImGui window
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
			SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
		}

		SDL_GL_SwapWindow(window.GetSDLWindow());
	}

	// Shut down GUI system
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
}