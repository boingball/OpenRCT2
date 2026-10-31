/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <openrct2/world/tile_element/TileElement.h>
#include <openrct2/GameState.h>
#include <openrct2/world/TilePointerIndex.hpp>
#include <array>
#include <memory>
#include <string>

// A deliberately isolated partial state. Never install it as the active
// game state: objects, rides, entities and other simulation data are absent.
struct PartialS6State
{
    std::unique_ptr<OpenRCT2::GameState_t> state;
    std::unique_ptr<TilePointerIndex<OpenRCT2::TileElement>> tileIndex;
    std::array<uint32_t, 16> sourceTypes{};
    uint32_t surfaces = 0;
    uint32_t paths = 0;
    uint32_t omittedElements = 0;
    std::string scenarioLabel;
};

PartialS6State ImportS6TerrainAndMetadata(const char* path);
uint32_t PartialS6Fingerprint(const PartialS6State& imported);
void WritePartialS6Overview(const PartialS6State& imported, const char* path);
