#include "Film.h"
#include "Window.h"
#include "Sphere.h"
#include "PathTracer.h"
#include "Camera.h"

#include <IMGUI/imgui.h>
#include <IMGUI/imgui_impl_sdl2.h>
#include <IMGUI/imgui_impl_opengl3.h>

#include <iostream>

#undef main
int main()
{
	int winWidth = 800;
	int winHeight = 600;

	Film film(winWidth, winHeight);
	Window window(winWidth, winHeight, "Tracer");

	PathTracer pathTracer;

	Camera camera(glm::ivec2(winWidth, winHeight));

	// Setting up the GUI system
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImGui::StyleColorsDark();

	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	const char* glslVersion = "#version 430";
	ImGui_ImplSDL2_InitForOpenGL(window.GetSDLWindow(), window.GetGLContext());
	ImGui_ImplOpenGL3_Init(glslVersion);

    std::shared_ptr<Sphere> sphere1 = std::make_shared<Sphere>("Sphere 1", glm::vec3(0.0f, 0.0f, -5.0f), 1.0f);
	
	pathTracer.AddRayObject(sphere1);

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
					winWidth = event.window.data1;
					winHeight = event.window.data2;
					film.Resize(winWidth, winHeight);
					window.Resize(winWidth, winHeight);
				}
			}
		}

		winWidth = window.Width();
		winHeight = window.Height();

		// Tracing
		for (int y = 0; y < winHeight; ++y)
		{
			for (int x = 0; x < winWidth; ++x)
			{
				Ray ray = camera.GetRay({ x, y }, { winWidth, winHeight });
				glm::vec3 colour = pathTracer.TraceRay(ray, camera.GetPosition(), 0);
				film.AddSample(x, y, colour);
			}
		}

		{
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplSDL2_NewFrame();
			ImGui::NewFrame();

			ImGui::Begin("Scene Controls");

			if (ImGui::Button("Rest Accumulation"))
			{
				film.Reset();
			}

			for (auto& rayObject : pathTracer.GetRayObjects())
			{
				rayObject->UpdateUI();
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