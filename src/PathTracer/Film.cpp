#include "Film.h"

Film::Film()
	: mWidth(0)
	, mHeight(0)
{
}

Film::Film(int _width, int _height)
	: mWidth(0)
	, mHeight(0)
{
	Resize(_width, _height);
}


void Film::Resize(int _width, int _height)
{
    mWidth = glm::max(0, _width);
    mHeight = glm::max(0, _height);
    const int n = PixelCount();
    mAccum.assign(n, glm::vec3(0.0f));
    mSamples.assign(n, 0u);
    mDisplay8.assign(std::max(1, n) * 4, 0u);
    mDirty = true;
}

void Film::Reset()
{
    std::fill(mAccum.begin(), mAccum.end(), glm::vec3(0.0f));
    std::fill(mSamples.begin(), mSamples.end(), 0u);
    mDirty = true;
}

glm::vec3 Film::AverageAt(int _x, int _y) const
{
    const int p = _y * mWidth + _x;
    const uint32_t s = mSamples[p];
    return s ? (mAccum[p] / float(s)) : glm::vec3(0.0f);
}

const std::vector<std::uint8_t>& Film::ResolveToRGBA8()
{
    if (!mDirty) return mDisplay8;

    const int n = PixelCount();
    if ((int)mDisplay8.size() != n * 4) mDisplay8.resize(n * 4);

    for (int p = 0; p < n; ++p)
    {
        const uint32_t s = mSamples[p];
        glm::vec3 c = s ? (mAccum[p] / float(s)) : glm::vec3(0.0f);

        // Clamp linear 0..1
        c = glm::clamp(c, glm::vec3(0.0f), glm::vec3(1.0f));

        mDisplay8[4 * p + 0] = static_cast<std::uint8_t>(std::lround(c.r * 255.0f));
        mDisplay8[4 * p + 1] = static_cast<std::uint8_t>(std::lround(c.g * 255.0f));
        mDisplay8[4 * p + 2] = static_cast<std::uint8_t>(std::lround(c.b * 255.0f));
        mDisplay8[4 * p + 3] = 255u;
    }

    mDirty = false;
    return mDisplay8;
}