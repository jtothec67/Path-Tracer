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

void Film::AddSample(int _x, int _y, const glm::vec3& _linearRGB)
{
    const int p = _y * mWidth + _x;
    mAccum[p] += _linearRGB;
    mSamples[p] += 1u;
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

        if (toneMap == ToneMap::Reinhard)
            // Simple Reinhard tone mapping
            c = c / (glm::vec3(1.0f) + c);

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

const std::vector<std::uint8_t>& Film::ResolveToRGBA8_sRGB()
{
    if (!mDirty) return mDisplay8;

    const int n = PixelCount();
    if ((int)mDisplay8.size() != n * 4) mDisplay8.resize(n * 4);

    for (int p = 0; p < n; ++p)
    {
        const uint32_t s = mSamples[p];
        glm::vec3 c = s ? (mAccum[p] / float(s)) : glm::vec3(0.0f); // linear RGB

        if (toneMap == ToneMap::Reinhard)
            // Simple Reinhard tone mapping
            c = c / (glm::vec3(1.0f) + c);

        // sRGB encoding
        glm::vec3 e;
        // R
        {
            float u = std::max(0.0f, c.r);
            e.r = (u <= 0.0031308f) ? (12.92f * u) : (1.055f * std::pow(u, 1.0f / 2.4f) - 0.055f);
        }
        // G
        {
            float u = std::max(0.0f, c.g);
            e.g = (u <= 0.0031308f) ? (12.92f * u) : (1.055f * std::pow(u, 1.0f / 2.4f) - 0.055f);
        }
        // B
        {
            float u = std::max(0.0f, c.b);
            e.b = (u <= 0.0031308f) ? (12.92f * u) : (1.055f * std::pow(u, 1.0f / 2.4f) - 0.055f);
        }

        // Clamp to displayable range after encoding
        e = glm::clamp(e, glm::vec3(0.0f), glm::vec3(1.0f));

        mDisplay8[4 * p + 0] = static_cast<std::uint8_t>(std::lround(e.r * 255.0f));
        mDisplay8[4 * p + 1] = static_cast<std::uint8_t>(std::lround(e.g * 255.0f));
        mDisplay8[4 * p + 2] = static_cast<std::uint8_t>(std::lround(e.b * 255.0f));
        mDisplay8[4 * p + 3] = 255u;
    }

    mDirty = false;
    return mDisplay8;
}