#pragma once

#include <chrono>
#include <GWCA/Constants/Constants.h>

namespace ZaishenCycles
{
	using namespace std::chrono;
	uint32_t GetZiashenMission(system_clock::time_point = system_clock::now());
	uint32_t GetZiashenBounty(system_clock::time_point = system_clock::now());
}

