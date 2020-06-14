#include "Input.h"

bool Input::ProcessInput(float dt)
{
	SDL_Event e;
	while (SDL_PollEvent(&e))
	{
		if (e.type == SDL_QUIT) return false;

		if (e.type == SDL_KEYDOWN)
		{
			m_keys[e.key.keysym.scancode] = true;
			m_key_hold_time[e.key.keysym.scancode] += dt;
		}
		else if (e.type == SDL_KEYUP)
		{
			m_keys[e.key.keysym.scancode] = false;
			m_key_hold_time[e.key.keysym.scancode] = 0;
		} 
	}

	return true;
}

key_packet Input::IsKeyDown(int key)
{
	key_packet k;
	k.value = m_keys[key];
	k.hold_time = m_key_hold_time[key];

	return k;
}

key_packet Input::IsKeyUp(int key)
{
	key_packet k;
	k.value = !m_keys[key];
	k.hold_time = m_key_hold_time[key];

	return k;
}