#include "equipment.h"

#include <fmt/format.h>

std::string build_equipment_path(const int model_id, const Race race, const Slot slot) {
    return fmt::format("chara/equipment/e{model_id:04d}/model/c{race_id:04d}e{model_id:04d}_{slot}.mdl",
                       fmt::arg("model_id", model_id),
                       fmt::arg("race_id", get_race_id(race)),
                       fmt::arg("slot", get_slot_abbreviation(slot)));
}