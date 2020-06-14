#pragma once
#include <chrono>

#define DT_SMOOTHENING_BUFFER_SIZE 30

class WTime
{
private:
	std::chrono::high_resolution_clock m_clock;
	std::chrono::milliseconds lastTime;

	double last_ten[DT_SMOOTHENING_BUFFER_SIZE];
public:
	double deltaTime;
	double timeScale;
	double fps;
	double time;

	WTime();
	void Update();
	void ResetTime();
};