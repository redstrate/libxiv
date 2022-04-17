#pragma once

/*
 * Different genders of the same race are considered
 * different races with different ids in the game data,
 * so we keep the same semantics here.
 */
enum class Race {
    HyurMidlanderMale,
    HyurMidlanderFemale,
};

/*
 * This returns the race id. For example, Hyur Midlander Male returns 101.
 */
int get_race_id(Race race);