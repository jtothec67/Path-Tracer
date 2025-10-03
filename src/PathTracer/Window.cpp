#include "Window.h"

#include "stb_image_write.h"

#include <iostream>
#include <filesystem>

Window::Window(int _width, int _height, const char* _title) : mWidth(_width), mHeight(_height)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cout << "Couldn't create SDL window. Error: " << SDL_GetError() << std::endl;
        throw std::exception("Couldn't create SDL window.");
    }

    // Ask for a modern core context
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 0); // Can use sRGB later if needed

    mWindow = SDL_CreateWindow(_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        mWidth, mHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!mWindow) throw std::runtime_error(SDL_GetError());

    mContext = SDL_GL_CreateContext(mWindow);
    if (!mContext)
    {
        std::cout << "Couldn't create SDL window. Error: " << SDL_GetError() << std::endl;
        throw std::exception("Couldn't create SDL window.");
    }

    SDL_GL_SetSwapInterval(1); // vsync

    GLenum err = glewInit();
    if (err != GLEW_OK) throw std::runtime_error("GLEW init failed");

    InitGL();
    CreateScreenResources();
}

Window::~Window()
{
    if (mProg) glDeleteProgram(mProg);
    if (mVBO)  glDeleteBuffers(1, &mVBO);
    if (mVAO)  glDeleteVertexArrays(1, &mVAO);
    if (mTex)  glDeleteTextures(1, &mTex);
    if (mContext) { SDL_GL_DeleteContext(mContext); mContext = nullptr; }
    if (mWindow) { SDL_DestroyWindow(mWindow); mWindow = nullptr; }
    SDL_Quit();
}

void Window::InitGL()
{
    glViewport(0, 0, mWidth, mHeight);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    const char* vsSrc = R"(
        #version 330 core
        layout(location=0) in vec2 aPos;
        layout(location=1) in vec2 aUV;
        out vec2 vUV;
        void main() {
            vUV = aUV;
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )";
    const char* fsSrc = R"(
        #version 330 core
        in vec2 vUV;
        out vec4 FragColor;
        uniform sampler2D uTex;
        void main() {
            FragColor = texture(uTex, vUV);
        }
    )";

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSrc, nullptr);
    glCompileShader(vs);
    GLint vsok; glGetShaderiv(vs, GL_COMPILE_STATUS, &vsok);
    if (!vsok)
    {
        char log[4096]; glGetShaderInfoLog(vs, sizeof log, nullptr, log);
        throw std::runtime_error(std::string("Vertex shader compile failed: ") + log);
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSrc, nullptr);
    glCompileShader(fs);
    GLint fsok; glGetShaderiv(fs, GL_COMPILE_STATUS, &fsok);
    if (!fsok)
    {
        char log[4096]; glGetShaderInfoLog(fs, sizeof log, nullptr, log);
        throw std::runtime_error(std::string("Fragment shader compile failed: ") + log);
    }

    mProg = glCreateProgram();
    glAttachShader(mProg, vs); glAttachShader(mProg, fs);
    glLinkProgram(mProg);
    GLint prok; glGetProgramiv(mProg, GL_LINK_STATUS, &prok);
    if (!prok)
    {
        char log[4096]; glGetProgramInfoLog(mProg, sizeof log, nullptr, log);
        throw std::runtime_error(std::string("Program link failed: ") + log);
    }
    glDeleteShader(vs); glDeleteShader(fs);
}

void Window::CreateScreenResources()
{
    // Fullscreen triangle (positions, uvs)
    const float verts[] = {
        //   x,    y,    u,   v
        -1.f, -1.f,  0.f, 0.f,
         3.f, -1.f,  2.f, 0.f,
        -1.f,  3.f,  0.f, 2.f,
    };
    glGenVertexArrays(1, &mVAO);
    glGenBuffers(1, &mVBO);
    glBindVertexArray(mVAO);
    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);

    // Screen texture
    glGenTextures(1, &mTex);
    glBindTexture(GL_TEXTURE_2D, mTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, mWidth, mHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Window::Resize(int _width, int _height)
{
    mWidth = _width;
    mHeight = _height;
    glViewport(0, 0, mWidth, mHeight);

    // Reallocate existing texture storage to the new size
    glBindTexture(GL_TEXTURE_2D, mTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, mWidth, mHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Window::DrawScreen(const std::vector<uint8_t>& rgba8)
{
    // Upload (assumes rgba8 is W*H*4 in size)
    glBindTexture(GL_TEXTURE_2D, mTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mWidth, mHeight, GL_RGBA, GL_UNSIGNED_BYTE, rgba8.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    Draw();
}

bool Window::SaveImagePNG(const std::string& _filename, const std::vector<uint8_t>& _rgba8)
{
    // Ensure parent directory exists
    std::error_code ec;
    std::filesystem::path p(_filename);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);

    const int stride = mWidth * 4;

    // Optional vertical flip so the file isn't upside-down
    const uint8_t* dataPtr = _rgba8.data();
    std::vector<uint8_t> flipped;

    flipped.resize(_rgba8.size());
    for (int y = 0; y < mHeight; ++y)
    {
        const uint8_t* src = _rgba8.data() + (size_t)y * stride;
        uint8_t* dst = flipped.data() + (size_t)(mHeight - 1 - y) * stride;
        std::memcpy(dst, src, (size_t)stride);
    }
    dataPtr = flipped.data();

    // Write PNG (comp=4 for RGBA)
    int ok = stbi_write_png(_filename.c_str(), mWidth, mHeight, 4, dataPtr, stride);
    return ok != 0;
}

void Window::Draw()
{
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(mProg);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTex);

    glBindVertexArray(mVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}