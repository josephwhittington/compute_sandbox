#pragma once

#include "SDL.h"

struct key_packet
{
	bool value;
	float hold_time = 0;
};

class Input
{
public:
	Input() = default;
	~Input() = default;

	bool ProcessInput(float dt);

	// Key getters
	key_packet IsKeyDown(int key);
	key_packet IsKeyUp(int key);

private:
	bool m_keys[1024];
	float m_key_hold_time[1024] = { 0.0f };
};