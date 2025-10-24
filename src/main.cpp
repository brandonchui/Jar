#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "App.h"
#include "graphics/Core.h"
#include "utils/MessageBox.h"

/// The SDL3 callback system was not strictly nessessary here,
/// but since the system supports all platforms (including web),
/// it serves as a good base feature expansion.
extern "C"
{
	/// Gets called one time at start up.
	SDL_AppResult SDL_AppInit(void** appstate, [[maybe_unused]] int32_t argc,
							  [[maybe_unused]] char* argv[])
	{
		if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
		{
			Utils::ErrorMessageBox("SDL Initialization Failed", SDL_GetError());
			return SDL_APP_FAILURE;
		}

		auto* app = new App();
		*appstate = app;

		if (!app->Initialize())
		{
			Utils::ErrorMessageBox("Failed to init application - Exiting");
			delete app;
			return SDL_APP_FAILURE;
		}

		return SDL_APP_CONTINUE;
	}

	/// Called when some SDL event arrives.
	SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
	{
		auto* app = static_cast<App*>(appstate);

		app->ProcessEvent(*event);

		// Check if we should quit
		if (!app->IsRunning())
		{
			return SDL_APP_SUCCESS;
		}

		return SDL_APP_CONTINUE;
	}

	/// The game loop that iterates over and over.
	SDL_AppResult SDL_AppIterate(void* appstate)
	{
		auto* app = static_cast<App*>(appstate);

		if (!app->IsRunning())
		{
			return SDL_APP_SUCCESS;
		}

		// Calculating the delta time and passing it to our application
		// code's Update function. We get the delta between the current
		// tick (1 ms) count subtracted from the last frame's tick count.
		// TODO look into SDL_GetPerformanceFrequency()
		uint64_t currentTicks = SDL_GetTicks();
		float deltaTime = static_cast<float>(currentTicks - app->GetLastTickCount()) / 1000.0F;
		app->GetLastTickCount() = currentTicks;

		app->Update(deltaTime);

		// The graphics context is a global that we need to make sure
		// exists first before calling it to Render.
		if (Graphics::gGraphicsContext != nullptr)
		{
			app->Render(*Graphics::gGraphicsContext);
		}

		return SDL_APP_CONTINUE;
	}

	/// Called once before the app terminates
	void SDL_AppQuit(void* appstate, [[maybe_unused]] SDL_AppResult result)
	{
		auto* app = static_cast<App*>(appstate);

		if (app != nullptr)
		{
			app->Shutdown();
			delete app;
		}

		SDL_Quit();
	}
}
