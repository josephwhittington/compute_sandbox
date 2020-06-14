#include "WTime.h"

WTime::WTime()
{
	time = 0;
	lastTime = std::chrono::duration_cast<std::chrono::milliseconds>(m_clock.now().time_since_epoch());
	deltaTime = 0;
	timeScale = 1;
	fps = 0;

	// Initialize the time to 0
	memset(last_ten, 0, sizeof(double) * DT_SMOOTHENING_BUFFER_SIZE);
}

void WTime::Update()
{
	// Time
	std::chrono::milliseconds currenttime = std::chrono::duration_cast<std::chrono::milliseconds>(m_clock.now().time_since_epoch());
	double temp = (double)(currenttime - lastTime).count();
	temp /= 1000;

	// Set the thing
	last_ten[DT_SMOOTHENING_BUFFER_SIZE - 1] = temp;

	// Move the previous frames up
	int fc = 0;
	double sum = temp;
	for (int i = 0; i < DT_SMOOTHENING_BUFFER_SIZE - 1; i++)
	{
		last_ten[i] = last_ten[i + 1];
		// Add to sum
		sum += last_ten[i];
		// Increment number for active frames for smoothening
		if (last_ten[i] > FLT_MIN) fc++;
	}
	// Set the last frame to the recently computed time smoothened
	temp = sum / static_cast<double>(fc);
	deltaTime = temp;

	// Set last time
	lastTime = currenttime;
	time += deltaTime;
	fps = 1.0 / deltaTime;
}

void WTime::ResetTime()
{
	time = 0;
}