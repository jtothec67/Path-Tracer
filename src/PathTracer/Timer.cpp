#include "Timer.h"

#include <iostream>

Timer::Timer()
{
	Start();
}

Timer::~Timer()
{
	if (mRunning)
	{
		std::cout << "That took: " << GetElapsedSeconds() << " seconds" << std::endl;
	}
}

void Timer::Start()
{
	mStart = std::chrono::steady_clock::now();
	mRunning = true;
}

float Timer::Stop()
{
	if (!mRunning)
	{
		return 0.016f;
	}

	mEnd = std::chrono::steady_clock::now();

	std::chrono::duration<float> duration = mEnd - mStart;

	mStart = mEnd;

	mRunning = false;

	return duration.count();
}

void Timer::Reset()
{
	Stop();
	Start();
}

float Timer::GetElapsedSeconds()
{
	if (!mRunning)
	{
		return 0.0f;
	}

	mTempTimeStamp = std::chrono::steady_clock::now();

	std::chrono::duration<float> duration = mTempTimeStamp - mStart;

	return duration.count();
}

float Timer::GetElapsedMilliseconds()
{
	if (!mRunning)
	{
		return 0.0f;
	}

	mTempTimeStamp = std::chrono::steady_clock::now();

	std::chrono::duration<float, std::milli> duration = mTempTimeStamp - mStart;

	return duration.count();
}