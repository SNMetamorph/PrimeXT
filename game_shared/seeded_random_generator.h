/*
seeded_random_generator.h - stateless random numbers generator, used within weapon predicting
Copyright (C) 2024 SNMetamorph

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#pragma once
#include <stdint.h>

class CSeededRandomGenerator
{
public:
	int32_t GetInteger(uint32_t seed, int32_t min, int32_t max) const;
	float GetFloat(uint32_t seed, float min, float max) const;

private:
	uint32_t ExecuteRound(uint32_t &seed) const;

	static const uint32_t m_seedTable[256];
};
