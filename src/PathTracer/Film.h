#pragma once

#include <glm/glm.hpp>

#include <vector>

enum class ColourSpace
{
    Linear,
    sRGB
};

enum class ToneMap
{
    None,
    Reinhard
};

class Film
{
public:
    Film();
	Film(int _width, int _height);

    void Resize(int _width, int _height);
    void Reset();

    // Add one sample in linear RGB
	void AddSample(int _x, int _y, const glm::vec3& _linearRGB);

    // Read the current average (linear space). Returns {0,0,0} if no samples yet.
    glm::vec3 AverageAt(int _x, int _y) const;

	// Returns a reference to an internal buffer sized W*H*4. Used for OpenGL texture upload.
    const std::vector<std::uint8_t>& ResolveToRGBA8();

	void SetColourSpace(ColourSpace _colourSpace) { mColourSpace = _colourSpace; mDirty = true; }
	ColourSpace GetColourSpace() const { return mColourSpace; }

	void SetToneMap(ToneMap _toneMap) { mToneMap = _toneMap; mDirty = true; }
	ToneMap GetToneMap() const { return mToneMap; }

    int  Width() const { return mWidth; }
    int  Height() const { return mHeight; }
    int  PixelCount() const { return mWidth * mHeight; }

    const std::vector<glm::vec3>& Accum() const { return mAccum; }
    const std::vector<std::uint32_t>& Samples() const { return mSamples; }

private:
	int mWidth = 0;
	int mHeight = 0;

    std::vector<glm::vec3> mAccum; // Linear sums per pixel
    std::vector<std::uint32_t> mSamples; // Sample counts per pixel
    std::vector<std::uint8_t>  mDisplay8; // Cached RGBA8 output

	ColourSpace mColourSpace = ColourSpace::sRGB;
	ToneMap mToneMap = ToneMap::Reinhard;

	bool mDirty = true; // Accumulation changed since last resolve
};