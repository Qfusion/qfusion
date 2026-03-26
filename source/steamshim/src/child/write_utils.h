#pragma once
#include "../os.h"
#include "../steamshim_types.h"

template <typename T>
static inline void prepared_rpc_packet( const steam_rpc_shim_common_s *req, T *response )
{
	response->sync = req->sync;
	response->cmd = req->cmd;
}

static inline void write_packet( PipeType fd, const void *response, uint32_t size )
{
	writePipe( fd, &size, sizeof( uint32_t ) );
	writePipe( fd, response, size );
}


