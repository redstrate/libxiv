#pragma once

#include <string_view>
#include <optional>

enum class Slot {
    Head,
    Hands,
    Legs,
    Feet,
    Body,
    Earring,
    Neck,
    Rings,
    Wrists
};

/*
 * This gets the slot abbreviation used in model paths. For example, Head returns "met".
 */
std::string_view get_slot_abbreviation(Slot slot);

std::optional<Slot> get_slot_from_id(int id);