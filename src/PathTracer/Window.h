#pragma once

#include <SDL2/sdl.h>
#include <GL/glew.h>

#include <vector>

class Window
{
public:
    Window(int _width, int _height, const char* _title = "Tracer");
    ~Window();

    void Resize(int _width, int _height); // Reallocate GL texture and viewport
    void DrawScreen(const std::vector<uint8_t>& _rgba8); // Upload and draw

    int  Width()  const { return mWidth; }
    int  Height() const { return mHeight; }

    SDL_Window* GetSDLWindow()  const { return mWindow; }
    SDL_GLContext GetGLContext()  const { return mContext; }

private:
    void InitGL();
    void CreateScreenResources();
    void Draw(); // Draws the texture to the screen

    int mWidth = 0;
    int mHeight = 0;
    SDL_Window* mWindow = nullptr;
    SDL_GLContext mContext = nullptr;

    GLuint mTex = 0; // Screen texture
    GLuint mVAO = 0; // Fullscreen triangle
    GLuint mVBO = 0;
    GLuint mProg = 0; // Simple textured shader
};