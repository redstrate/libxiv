#include "types/race.h"
#include "types/slot.h"

std::string_view get_slot_abbreviation(Slot slot) {
    switch(slot) {
        case Slot::Head:
            return "met";
        case Slot::Hands:
            return "glv";
        case Slot::Legs:
            return "dwn";
        case Slot::Feet:
            return "sho";
        case Slot::Body:
            return "top";
        case Slot::Earring:
            return "ear";
        case Slot::Neck:
            return "nek";
        case Slot::Rings:
            return "rir";
        case Slot::Wrists:
            return "wrs";
    }
}

std::optional<Slot> get_slot_from_id(const int id) {
    switch(id) {
        case 3:
            return Slot::Head;
        case 5:
            return Slot::Hands;
        case 7:
            return Slot::Legs;
        case 8:
            return Slot::Feet;
        case 4:
            return Slot::Body;
        case 9:
            return Slot::Earring;
        case 10:
            return Slot::Neck;
        case 12:
            return Slot::Rings;
        case 11:
            return Slot::Wrists;
        default:
            return {};
    }
}

int get_race_id(Race race) {
    switch(race) {
        case Race::HyurMidlanderMale:
            return 101;
        case Race::HyurMidlanderFemale:
            return 201;
    }
}