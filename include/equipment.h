#pragma once

#include <string>

#include "types/race.h"
#include "types/slot.h"

std::string build_equipment_path(int model_id, Race race, Slot slot);