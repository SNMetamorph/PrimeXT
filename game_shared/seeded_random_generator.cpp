/*
seeded_random_generator.cpp - stateless random numbers generator, used within weapon predicting
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

#include "seeded_random_generator.h"
#include <cstring>

const uint32_t CSeededRandomGenerator::m_seedTable[256] =
{
	28985, 27138, 26457, 9451, 17764, 10909, 28790, 8716, 6361, 4853, 17798, 21977, 19643, 20662, 10834, 20103,
	27067, 28634, 18623, 25849, 8576, 26234, 23887, 18228, 32587, 4836, 3306, 1811, 3035, 24559, 18399, 315,
	26766, 907, 24102, 12370, 9674, 2972, 10472, 16492, 22683, 11529, 27968, 30406, 13213, 2319, 23620, 16823,
	10013, 23772, 21567, 1251, 19579, 20313, 18241, 30130, 8402, 20807, 27354, 7169, 21211, 17293, 5410, 19223,
	10255, 22480, 27388, 9946, 15628, 24389, 17308, 2370, 9530, 31683, 25927, 23567, 11694, 26397, 32602, 15031,
	18255, 17582, 1422, 28835, 23607, 12597, 20602, 10138, 5212, 1252, 10074, 23166, 19823, 31667, 5902, 24630,
	18948, 14330, 14950, 8939, 23540, 21311, 22428, 22391, 3583, 29004, 30498, 18714, 4278, 2437, 22430, 3439,
	28313, 23161, 25396, 13471, 19324, 15287, 2563, 18901, 13103, 16867, 9714, 14322, 15197, 26889, 19372, 26241,
	31925, 14640, 11497, 8941, 10056, 6451, 28656, 10737, 13874, 17356, 8281, 25937, 1661, 4850, 7448, 12744,
	21826, 5477, 10167, 16705, 26897, 8839, 30947, 27978, 27283, 24685, 32298, 3525, 12398, 28726, 9475, 10208,
	617, 13467, 22287, 2376, 6097, 26312, 2974, 9114, 21787, 28010, 4725, 15387, 3274, 10762, 31695, 17320,
	18324, 12441, 16801, 27376, 22464, 7500, 5666, 18144, 15314, 31914, 31627, 6495, 5226, 31203, 2331, 4668,
	12650, 18275, 351, 7268, 31319, 30119, 7600, 2905, 13826, 11343, 13053, 15583, 30055, 31093, 5067, 761,
	9685, 11070, 21369, 27155, 3663, 26542, 20169, 12161, 15411, 30401, 7580, 31784, 8985, 29367, 20989, 14203,
	29694, 21167, 10337, 1706, 28578, 887, 3373, 19477, 14382, 675, 7033, 15111, 26138, 12252, 30996, 21409,
	25678, 18555, 13256, 23316, 22407, 16727, 991, 9236, 5373, 29402, 6117, 15241, 27715, 19291, 19888, 19847
};

int32_t CSeededRandomGenerator::GetInteger(uint32_t seed, int32_t min, int32_t max) const
{
	int32_t range = max - min;
	if (range <= 0) {
		return min;
	}
	else 
	{
		uint32_t initialSeed = m_seedTable[static_cast<int32_t>(seed + min + max) & 0xFF];
		uint32_t value = ExecuteRound(initialSeed);
		return min + (value % range);
	}
}

float CSeededRandomGenerator::GetFloat(uint32_t seed, float min, float max) const
{
	int32_t a, b;
	std::memcpy(&a, &min, sizeof(a));
	std::memcpy(&b, &max, sizeof(b));

	float range = max - min;
	if (range <= 0.0f) {
		return min;
	}
	else
	{
		uint32_t initialSeed = m_seedTable[static_cast<int32_t>(seed + a + b) & 0xFF];
		for (int32_t i = 0; i < 2; i++) {
			ExecuteRound(initialSeed);
		}
		uint32_t value = ExecuteRound(initialSeed) & 0xFFFF;
		float fraction = value / 65536.0f;
		return min + (range * fraction);
	}
}

uint32_t CSeededRandomGenerator::ExecuteRound(uint32_t &seed) const
{
	seed *= 69069;
	seed += m_seedTable[seed & 0xFF];
	seed += 1;
	return seed & 0x0FFFFFFF;
}
