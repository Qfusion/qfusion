#include "rpc_async_handles.h"
#include <vector>

static std::vector<steam_async_complete_s> handles;
void steam_async_push_rpc_shim(SteamAPICall_t call, steam_rpc_shim_common_s* common)
{
	steam_async_complete_s res;
	res.api_call = call;
	res.rpc = *common;
	handles.push_back( res );
}

bool steam_async_pop_rpc_shim( SteamAPICall_t call, struct steam_rpc_shim_common_s *result )
{
	assert( result );
	for( size_t i = 0; i < handles.size(); ++i ) {
		if( handles[i].api_call == call ) {
			*result = handles[i].rpc;
			if( i != handles.size() - 1 ) {
				handles[i] = std::move( handles.back() );
			}
			handles.pop_back();
			return true;
		}
	}
	return false;
}
