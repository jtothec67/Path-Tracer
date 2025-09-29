#include "Film.h"
#include "Window.h"
#include "Sphere.h"
#include "Box.h"
#include "PathTracer.h"
#include "Camera.h"
#include "Timer.h"
#include "ThreadPool.h"

#include <IMGUI/imgui.h>
#include <IMGUI/imgui_impl_sdl2.h>
#include <IMGUI/imgui_impl_opengl3.h>

#include <iostream>

void TracePixels(int _fromy, int _toy, glm::ivec2 _winSize, std::shared_ptr<Camera> _camera, std::shared_ptr<PathTracer> _pathTracer, std::shared_ptr<Film> _film, int _depth, bool _albedoOnly)
{
	for (int y = _fromy; y <= _toy && y < _winSize.y; ++y)
	{
		for (int x = 0; x < _winSize.x; ++x)
		{
			Ray ray = _camera->GetRay({ x, y }, _winSize);
			glm::vec3 colour = _pathTracer->TraceRay(ray, _depth, _albedoOnly);
			_film->AddSample(x, y, colour);
		}
	}
}

void RayTraceParallel(ThreadPool& threadPool, int _numTasks, glm::ivec2 _winSize, std::shared_ptr<Camera> _camera, std::shared_ptr<PathTracer> _pathTracer, std::shared_ptr<Film> _film, int _depth, bool _albedoOnly)
{
	// Calculate the number of rows each thread should process
	int rowsPerThread = std::ceil(_winSize.y / static_cast<float>(_numTasks));

	// Enqueue tasks
	for (int i = 0; i < _numTasks; ++i)
	{
		int startY = i * rowsPerThread; // Starting row for this task
		int endY = std::min(startY + rowsPerThread, _winSize.y); // Ending row for this task

		// Enqueue the task to trace pixels for the assigned rows
		threadPool.EnqueueTask([=] { TracePixels(startY, endY - 1, _winSize, _camera, _pathTracer, _film, _depth, _albedoOnly); });
	}

	// Wait for all tasks to complete
	threadPool.WaitForCompletion();
}

#undef main
int main()
{
	int winWidth = 800;
	int winHeight = 600;

	std::shared_ptr<Film> film = std::make_shared<Film>(winWidth, winHeight);
	Window window(winWidth, winHeight, "Tracer");

	std::shared_ptr<PathTracer> pathTracer = std::make_shared<PathTracer>();

	std::shared_ptr<Camera> camera = std::make_shared<Camera>(glm::ivec2(winWidth, winHeight));
	camera->SetPosition(glm::vec3(-0.281922, 0, -5.05391));

	// Setting up the GUI system
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImGui::StyleColorsDark();

	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	const char* glslVersion = "#version 430";
	ImGui_ImplSDL2_InitForOpenGL(window.GetSDLWindow(), window.GetGLContext());
	ImGui_ImplOpenGL3_Init(glslVersion);

	//// --- Planet (ground) ---
	//// Radius is huge; center.y = -R so the surface sits at y = 0.
	//std::shared_ptr<Sphere> planet = std::make_shared<Sphere>("Planet", glm::vec3(0.0f, -1000.0f, -6.0f));
	//planet->SetRadius(999.f);

	//Material planetMat;
	//planetMat.albedo = glm::vec3(0.60f, 0.80f, 1.00f);   // light blue
	//planet->SetMaterial(planetMat);

	//pathTracer->AddRayObject(planet);

	//// --- Pink sphere sitting on the planet ---
	//// Put it at y = radius so it rests on the y=0 surface.
	//std::shared_ptr<Sphere> pink = std::make_shared<Sphere>("Pink Sphere", glm::vec3(0.0f, -0.5f, -3.5f));
	//pink->SetRadius(0.5f);

	//Material pinkMat;
	//pinkMat.albedo = glm::vec3(1.0f, 0.2f, 0.6f);        // pink
	//pink->SetMaterial(pinkMat);

	//pathTracer->AddRayObject(pink);

	//// --- Box next to the pink sphere ---
	//std::shared_ptr<Box> box = std::make_shared<Box>("Box");
	//box->SetPosition(glm::vec3(1.0f, 0.0f, -4.0f));
	//box->SetSize(glm::vec3(1.0f, 1.0f, 1.0f));
	//Material boxMat;
	//boxMat.albedo = glm::vec3(0.8f, 0.6f, 0.2f);       // yellowy
	//box->SetMaterial(boxMat);
	//pathTracer->AddRayObject(box);

	//// --- Sun (emissive) ---
	//std::shared_ptr<Sphere> sun = std::make_shared<Sphere>("Sun", glm::vec3(-4.0f, 4.0f, -6.f));
	//sun->SetRadius(1.0f);

	//Material sunMat;
	//sunMat.emissionColour = glm::vec3(1.0f, 1.0f, 1.0f);
	//sunMat.emissionStrength = 60.0f;                     // tweak to taste
	//sun->SetMaterial(sunMat);

	//pathTracer->AddRayObject(sun);

	// Room bounds in world space (open on the camera side)
	const float roomMinX = -1.0f;
	const float roomMaxX = 1.0f;
	const float roomMinY = 0.0f;
	const float roomMaxY = 2.0f;
	const float roomNearZ = -3.0f;  // opening toward camera
	const float roomFarZ = -7.0f;  // back wall

	const float wallT = 0.01f;      // wall thickness

	Material white; white.albedo = glm::vec3(0.73f, 0.73f, 0.73f);
	Material red;   red.albedo = glm::vec3(0.75f, 0.15f, 0.15f);
	Material green; green.albedo = glm::vec3(0.15f, 0.75f, 0.15f);

	// Floor
	{
		auto floorB = std::make_shared<Box>("Floor");
		floorB->SetPosition(glm::vec3(0,-1,-7));
		floorB->SetSize(glm::vec3(roomMaxX - roomMinX, wallT, roomNearZ - roomFarZ));
		floorB->SetMaterial(white);
		pathTracer->AddRayObject(floorB);
	}

	// Ceiling
	{
		auto ceilB = std::make_shared<Box>("Ceiling");
		ceilB->SetPosition(glm::vec3(-0.3, 0.99, -7));
		ceilB->SetSize(glm::vec3(roomMaxX - roomMinX, wallT, roomNearZ - roomFarZ));
		ceilB->SetMaterial(white);
		pathTracer->AddRayObject(ceilB);
	}

	// Back wall
	{
		auto backB = std::make_shared<Box>("BackWall");
		backB->SetPosition(glm::vec3(-0.2, 0, -7));
		backB->SetSize(glm::vec3(roomMaxX - roomMinX, roomMaxY - roomMinY, wallT));
		backB->SetMaterial(white);
		pathTracer->AddRayObject(backB);
	}

	// Front wall
	{
		auto frontB = std::make_shared<Box>("FrontWall");
		frontB->SetPosition(glm::vec3(-0.2, 0, -5));
		frontB->SetSize(glm::vec3(roomMaxX - roomMinX, roomMaxY - roomMinY, wallT));
		frontB->SetMaterial(white);
		pathTracer->AddRayObject(frontB);
	}

	// Left wall (red)
	{
		auto leftB = std::make_shared<Box>("LeftWall");
		leftB->SetPosition(glm::vec3(-1, 0, -7));
		leftB->SetSize(glm::vec3(wallT, roomMaxY - roomMinY, roomNearZ - roomFarZ));
		leftB->SetMaterial(red);
		pathTracer->AddRayObject(leftB);
	}

	// Right wall (green)
	{
		auto rightB = std::make_shared<Box>("RightWall");
		rightB->SetPosition(glm::vec3(0.59, 0, -7));
		rightB->SetSize(glm::vec3(wallT, roomMaxY - roomMinY, roomNearZ - roomFarZ));
		rightB->SetMaterial(green);
		pathTracer->AddRayObject(rightB);
	}

	// Rectangular emissive ceiling light (centered, inset from ceiling)
	{
		const glm::vec3 lightSize(1.2f, wallT, 1.8f);
		const glm::vec3 lightPos(-0.2, 0.989, -6);

		Material lightMat;
		lightMat.emissionColour = glm::vec3(1.0f);
		lightMat.emissionStrength = 3.5f;       // tweak as needed

		auto lightB = std::make_shared<Box>("CeilingLight");
		lightB->SetPosition(lightPos);
		lightB->SetSize(lightSize);
		lightB->SetMaterial(lightMat);
		pathTracer->AddRayObject(lightB);
	}

	// Sphere
	{
		auto sphere = std::make_shared<Sphere>("Sphere", glm::vec3(-0.2f, -0.3f, -7.0f));
		sphere->SetRadius(0.535f);
		Material sphereMat;
		sphereMat.albedo = glm::vec3(0.6343, 0.5316, 0.1109);
		sphereMat.roughness = 0.0f;
		sphere->SetMaterial(sphereMat);
		pathTracer->AddRayObject(sphere);
	}


	int numThreads = 32;
	int numTasks = 128;
	ThreadPool threadPool(numThreads);

	int rayDepth = 10;

	Timer timer;
	float msPerFrame = 0.0f;

	Timer accumulationTimer;
	int frameCounter = 0;

	bool albedoOnly = true;

	bool sRGB = true;

	SDL_Event event;

	bool running = true;
	while (running)
	{
		timer.Start();

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
					film->Resize(winWidth, winHeight);
					film->Reset();
					accumulationTimer.Reset();
					frameCounter = 0;
					window.Resize(winWidth, winHeight);
				}
			}
			if (event.type == SDL_KEYDOWN)
			{
				switch (event.key.keysym.sym)
				{
					// Camera movement
				case SDLK_w:
					camera->SetPosition(camera->GetPosition() + camera->GetForward() * msPerFrame / 1000.f);
					break;
				case SDLK_s:
					camera->SetPosition(camera->GetPosition() - camera->GetForward() * msPerFrame / 1000.f);
					break;
				case SDLK_a:
					camera->SetPosition(camera->GetPosition() + camera->GetRight() * msPerFrame / 1000.f);
					break;
				case SDLK_d:
					camera->SetPosition(camera->GetPosition() - camera->GetRight() * msPerFrame / 1000.f);
					break;
				case SDLK_q:
					camera->SetPosition(camera->GetPosition() - glm::vec3(0, 1, 0));
					break;
				case SDLK_e:
					camera->SetPosition(camera->GetPosition() + glm::vec3(0, 1, 0));
					break;
				case SDLK_UP:
					camera->SetRotation(glm::vec3(camera->GetRotation().x - 1, camera->GetRotation().y, camera->GetRotation().z));
					break;
				case SDLK_DOWN:
					camera->SetRotation(glm::vec3(camera->GetRotation().x + 1, camera->GetRotation().y, camera->GetRotation().z));
					break;
				case SDLK_LEFT:
					camera->SetRotation(glm::vec3(camera->GetRotation().x, camera->GetRotation().y - 1, camera->GetRotation().z));
					break;
				case SDLK_RIGHT:
					camera->SetRotation(glm::vec3(camera->GetRotation().x, camera->GetRotation().y + 1, camera->GetRotation().z));
					break;
				}
			}
		}

		winWidth = window.Width();
		winHeight = window.Height();

		{
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplSDL2_NewFrame();
			ImGui::NewFrame();

			ImGui::Begin("Scene Controls");

            ImGui::Text("%.3f ms", msPerFrame);

			ImGui::Text("%.0f seconds", accumulationTimer.GetElapsedSeconds());
			ImGui::Text("%i frames", frameCounter);

            if (ImGui::Checkbox("Albedo Only", &albedoOnly))
            {
                // If it was just disabled, reset film once for accumulation
                if (!albedoOnly)
                {
                    film->Reset();
					accumulationTimer.Reset();
					frameCounter = 0;
                }
            }
			if (albedoOnly)
			{
				// Use as a debug so don't want accumulation
				film->Reset();
				accumulationTimer.Reset();
				frameCounter = 0;
			}

			ImGui::Checkbox("sRGB output ", &sRGB);

			// Grab current value as int
			int current = static_cast<int>(film->GetToneMap());
			int before = current;

			ImGui::TextUnformatted("Tone mapping");
			// One radio per enum value
			ImGui::RadioButton("None", &current, static_cast<int>(ToneMap::None));
			ImGui::SameLine();
			ImGui::RadioButton("Reinhard", &current, static_cast<int>(ToneMap::Reinhard));

			// If user changed selection, push it back as enum
			if (current != before)
				film->SetToneMap(static_cast<ToneMap>(current));

			if (ImGui::Button("Rest Accumulation"))
			{
				film->Reset();
				accumulationTimer.Reset();
				frameCounter = 0;
			}

			ImGui::SliderInt("Ray Depth", &rayDepth, 1, 10);

			if(ImGui::SliderInt("Number of threads", &numThreads, 1, 128))
			{
				threadPool.Shutdown();
				threadPool.InitialiseThreads(numThreads);
			}

			ImGui::SliderInt("Number of tasks", &numTasks, 1, 128);


			for (auto& rayObject : pathTracer->GetRayObjects())
			{
				rayObject->UpdateUI();
			}

			ImGui::End();
		}

		frameCounter++;
		RayTraceParallel(threadPool, numTasks, glm::ivec2(winWidth, winHeight), camera, pathTracer, film, rayDepth, albedoOnly);

		auto RGBA = sRGB ? film->ResolveToRGBA8_sRGB() : film->ResolveToRGBA8();

		window.DrawScreen(RGBA);

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

		msPerFrame = timer.GetElapsedMilliseconds();
	}

	// Shut down GUI system
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
}