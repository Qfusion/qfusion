#pragma once

#include "../steamshim_types.h"
#include "steam/steamtypes.h"

struct steam_async_complete_s {
	SteamAPICall_t api_call;
	struct steam_rpc_shim_common_s rpc;
};

void steam_async_push_rpc_shim(SteamAPICall_t call, steam_rpc_shim_common_s* common);
bool steam_async_pop_rpc_shim(SteamAPICall_t call, struct steam_rpc_shim_common_s* result); 
