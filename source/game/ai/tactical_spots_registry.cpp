#include "tactical_spots_registry.h"
#include "bot.h"

TacticalSpotsRegistry TacticalSpotsRegistry::instance;

#define NAV_FILE_VERSION 10
#define NAV_FILE_EXTENSION "nav"
#define NAV_FILE_FOLDER "navigation"

bool TacticalSpotsRegistry::Init( const char *mapname ) {
	if( instance.IsLoaded() ) {
		G_Printf( "TacticalSpotsRegistry::Init(): The instance has been already initialized\n" );
		abort();
	}

	return instance.Load( mapname );
}

struct FileCloseGuard {
	int fp;
	FileCloseGuard( int fp_ ) : fp( fp_ ) {}

	~FileCloseGuard() {
		if( fp >= 0 ) {
			trap_FS_FCloseFile( fp );
		}
	}
};

struct FileRemoveGuard {
	const char *filename;
	FileRemoveGuard( const char *filename_ ) : filename( filename_ ) {};

	~FileRemoveGuard() {
		if( filename ) {
			trap_FS_RemoveFile( filename );
		}
	}

	void CancelPendingRemoval() { filename = nullptr; }
};

struct ScopedMessagePrinter {
	char buffer[256];

	ScopedMessagePrinter( const char *format, ... ) {
		va_list va;
		va_start( va, format );
		Q_vsnprintfz( buffer, 256, format, va );
		va_end( va );
	}

	~ScopedMessagePrinter() {
		if( *buffer ) {
			G_Printf( buffer );
		}
	}

	void CancelPendingMessage() { buffer[0] = 0; }
};

// Nav nodes used for old bots navigation
struct nav_node_s {
	vec3_t origin;
	int32_t flags;
	int32_t area;
};

bool TacticalSpotsRegistry::LoadRawNavFileData( const char *mapname ) {
	char filename[MAX_QPATH];
	Q_snprintfz( filename, sizeof( filename ), "%s/%s.%s", NAV_FILE_FOLDER, mapname, NAV_FILE_EXTENSION );

	constexpr const char *function = "TacticalSpotsRegistry::Load()";
	ScopedMessagePrinter messagePrinter( S_COLOR_RED "%s: Can't load file %s\n", function, filename );

	int fp;
	int fileSize = trap_FS_FOpenFile( filename, &fp, FS_READ );
	if( fileSize <= 0 ) {
		G_Printf( S_COLOR_RED "%s: Cannot open file %s", function, filename );
		return false;
	}

	FileCloseGuard fileCloseGuard( fp );

	// TODO: Old nav files never were arch/endian-aware.
	// We assume they are written on a little-endian arch hardware compiled with 32-bit int.

	int32_t version;
	if( trap_FS_Read( &version, 4, fp ) != 4 ) {
		G_Printf( S_COLOR_RED "%s: Can't read 4 bytes of the nav file version\n", function );
		return false;
	}

	version = LittleLong( version );
	if( version != NAV_FILE_VERSION ) {
		G_Printf( S_COLOR_RED "%s: Invalid nav file version %d\n", function, version );
		return false;
	}

	int32_t numRawNodes;
	if( trap_FS_Read( &numRawNodes, 4, fp ) != 4 ) {
		G_Printf( S_COLOR_RED "%s: Can't read 4 bytes of the raw nodes number\n", function );
		return false;
	}

	numRawNodes = LittleLong( numRawNodes );
	if( numRawNodes > MAX_SPOTS ) {
		G_Printf( S_COLOR_RED "%s: Too many nodes in file\n", function );
		return false;
	}

	unsigned expectedDataSize = sizeof( nav_node_s ) * numRawNodes;
	nav_node_s nodesBuffer[MAX_SPOTS];
	if( trap_FS_Read( nodesBuffer, expectedDataSize, fp ) != expectedDataSize ) {
		G_Printf( S_COLOR_RED "%s: Can't read nav nodes data\n", function );
		return false;
	}

	messagePrinter.CancelPendingMessage();

	const char *nodeOriginsData = (const char *)nodesBuffer + offsetof( nav_node_s, origin );
	return LoadSpotsFromRawNavNodes( nodeOriginsData, sizeof( nav_node_s ), (unsigned)numRawNodes );
}

bool TacticalSpotsRegistry::LoadSpotsFromRawNavNodes( const char *nodeOriginsData,
													  unsigned strideInBytes,
													  unsigned numRawNodes ) {
	if( spots ) {
		G_LevelFree( spots );
	}

	spots = (TacticalSpot *) G_LevelMalloc( sizeof( TacticalSpot ) * numRawNodes );
	numSpots = 0;

	const AiAasWorld *aasWorld = AiAasWorld::Instance();

	// Its good idea to make mins/maxs rounded too as was mentioned above
	constexpr int mins[] = { -24, -24, 0 };
	constexpr int maxs[] = { +24, +24, 72 };
	static_assert( mins[0] % 4 == 0 && mins[1] % 4 == 0 && mins[2] % 4 == 0, "" );
	static_assert( maxs[0] % 4 == 0 && maxs[1] % 4 == 0 && maxs[2] % 4 == 0, "" );

	// Prepare a dummy entity for the AAS area sampling
	edict_t dummyEnt;
	VectorSet( dummyEnt.r.mins, -12, -12, -12 );
	VectorSet( dummyEnt.r.maxs, +12, +12, +12 );

	for( int i = 0; i < numRawNodes; ++i ) {
		const float *fileOrigin = (const float *)( nodeOriginsData + strideInBytes * i );
		float *spotOrigin = dummyEnt.s.origin;

		for( int j = 0; j < 3; ++j ) {
			// Convert byte order
			spotOrigin[j] = LittleFloat( fileOrigin[j] );
			// Cached tactical spot origins are rounded up to 4 units.
			// We want tactical spot origins to match spot origins exactly.
			// Otherwise an original tactical spot may pass reachability check
			// and one restored from packed values may not,
			// and it happens quite often (blame AAS for it).
			spotOrigin[j] = 4.0f * ( (short)( ( (int)spotOrigin[j] ) / 4 ) );
		}

		VectorAdd( dummyEnt.s.origin, dummyEnt.r.mins, dummyEnt.r.absmin );
		VectorAdd( dummyEnt.s.origin, dummyEnt.r.maxs, dummyEnt.r.absmax );
		// AiAasWorld tries many attempts to find an area for an entity,
		// do not try to request finding a point area num
		if( int aasAreaNum = aasWorld->FindAreaNum( &dummyEnt ) ) {
			TacticalSpot &spot = spots[numSpots];
			spot.aasAreaNum = aasAreaNum;
			VectorCopy( spotOrigin, spot.origin );
			VectorCopy( spotOrigin, spot.absMins );
			VectorCopy( spotOrigin, spot.absMaxs );
			VectorAdd( spot.absMins, mins, spot.absMins );
			VectorAdd( spot.absMaxs, maxs, spot.absMaxs );
			numSpots++;
		} else {
			const char *format = S_COLOR_YELLOW "Can't find AAS area num for spot @ %.1f %.1f %.1f (rounded to 4 units)\n";
			G_Printf( format, spotOrigin[0], spotOrigin[1], spotOrigin[2] );
		}
	}

	return numSpots > 0;
}

bool TacticalSpotsRegistry::Load( const char *mapname ) {
	if( TryLoadPrecomputedData( mapname ) ) {
		return true;
	}

	if( !LoadRawNavFileData( mapname ) ) {
		return false;
	}

	ComputeMutualSpotsVisibility();
	ComputeMutualSpotsReachability();
	MakeSpotsGrid();
	needsSavingPrecomputedData = true;
	return true;
}

static bool WriteLengthAndData( const char *data, uint32_t dataLength, int fp ) {
	if( trap_FS_Write( &dataLength, 4, fp ) <= 0 ) {
		return false;
	}

	if( trap_FS_Write( data, dataLength, fp ) <= 0 ) {
		return false;
	}

	return true;
}

static bool ReadLengthAndData( char **data, uint32_t *dataLength, int fp ) {
	uint32_t length;
	if( trap_FS_Read( &length, 4, fp ) <= 0 ) {
		return false;
	}

	length = LittleLong( length );
	char *mem = (char *)G_LevelMalloc( length );
	if( trap_FS_Read( mem, length, fp ) <= 0 ) {
		G_LevelFree( mem );
		return false;
	}

	*data = mem;
	*dataLength = length;
	return true;
}

// We assume the end zero byte is written to a file too
static bool ExpectFileString( const char *expected, int fp, const char *message ) {
	uint32_t dataLength;
	char *data;

	if( !ReadLengthAndData( &data, &dataLength, fp ) ) {
		return false;
	}

	if( !dataLength ) {
		return expected[0] == 0;
	}

	data[dataLength - 1] = 0;
	if( Q_stricmp( expected, data ) ) {
		G_Printf( message );
		G_Printf( "Actual string: `%s`\n", data );
		G_Printf( "Expected string: `%s`\n", expected );
		G_LevelFree( data );
		return false;
	}

	G_LevelFree( data );
	return true;
}

constexpr const uint32_t PRECOMPUTED_DATA_VERSION = 13371337;

bool TacticalSpotsRegistry::TryLoadPrecomputedData( const char *mapname ) {
	char filename[MAX_QPATH];
	Q_snprintfz( filename, MAX_QPATH, "ai/%s.nav.cache", mapname );

	constexpr const char *function = "TacticalSpotsRegistry::TryLoadPrecomputedData()";
	ScopedMessagePrinter messagePrinter( S_COLOR_YELLOW "%s: Can't load %s\n", function, filename );

	int fp;
	if( trap_FS_FOpenFile( filename, &fp, FS_READ ) <= 0 ) {
		return false;
	}

	FileCloseGuard fileCloseGuard( fp );

	uint32_t version;
	if( trap_FS_Read( &version, 4, fp ) != 4 ) {
		return false;
	}

	version = LittleLong( version );
	if( version != PRECOMPUTED_DATA_VERSION ) {
		G_Printf( S_COLOR_YELLOW "Precomputed data version mismatch\n" );
		return false;
	}

	const char *mapMessage = va( S_COLOR_YELLOW "The map version differs with the precomputed data one\n", function );
	if( !ExpectFileString( trap_GetConfigString( CS_MAPCHECKSUM ), fp, mapMessage ) ) {
		return false;
	}

	const char *aasMessage = va( S_COLOR_YELLOW "The AAS data version differs with the precomputed data one\n", function );
	if( !ExpectFileString( AiAasWorld::Instance()->Checksum(), fp, aasMessage ) ) {
		return false;
	}

	uint32_t dataLength;
	char *data;

	// Read spots
	if( !ReadLengthAndData( &data, &dataLength, fp ) ) {
		return false;
	}

	// We do not need to add cleanup guards for any piece of data read below
	// (the pointers are saved in members and get freed in the class destructor)

	spots = (TacticalSpot *)data;
	numSpots = dataLength / sizeof( TacticalSpot );

	// Read spots travel time table
	if( !ReadLengthAndData( &data, &dataLength, fp ) ) {
		return false;
	}

	spotTravelTimeTable = (uint16_t *)data;
	if( dataLength / sizeof( uint16_t ) != numSpots * numSpots ) {
		G_Printf( S_COLOR_RED "%s: Travel time table size does not match the number of spots\n", function );
		return false;
	}

	// Read spots visibility table
	if( !ReadLengthAndData( &data, &dataLength, fp ) ) {
		return false;
	}

	spotVisibilityTable = (uint8_t *)data;
	if( dataLength / sizeof( uint8_t ) != numSpots * numSpots ) {
		G_Printf( S_COLOR_RED "%s: Spots visibility table size does not match the number of spots\n", function );
		return false;
	}

	SetupGridParams();
	const unsigned numGridCells = NumGridCells();

	// Read grid list offsets
	if( !ReadLengthAndData( &data, &dataLength, fp ) ) {
		return false;
	}

	gridListOffsets = (uint32_t *)data;
	if( dataLength / sizeof( uint32_t ) != numGridCells ) {
		G_Printf( S_COLOR_RED "%s: Grid spot list offsets size does not match the number of cells\n", function );
		return false;
	}

	// Read grid spot lists
	if( !ReadLengthAndData( &data, &dataLength, fp ) ) {
		return false;
	}

	gridSpotsLists = (uint16_t *)data;
	const unsigned gridListsArraySize = dataLength / sizeof( uint16_t );
	if( gridListsArraySize != numGridCells + numSpots ) {
		G_Printf( S_COLOR_RED "%s: Grid spot lists array size does not match numbers of cells and spots\n", function );
		return false;
	}

	// Byte swap and validate tactical spots
	const int numAasAreas = AiAasWorld::Instance()->NumAreas();
	for( unsigned i = 0; i < numSpots; ++i ) {
		auto &spot = spots[i];
		spot.aasAreaNum = LittleLong( spot.aasAreaNum );
		if( spot.aasAreaNum <= 0 || spot.aasAreaNum >= numAasAreas ) {
			G_Printf( S_COLOR_RED "%s: Bogus spot %d area num %d\n", function, i, spot.aasAreaNum );
			return false;
		}

		for( unsigned j = 0; j < 3; ++j ) {
			spot.origin[j] = LittleFloat( spot.origin[j] );
			spot.absMins[j] = LittleFloat( spot.absMins[j] );
			spot.absMaxs[j] = LittleFloat( spot.absMaxs[j] );
		}
	}

	// Byte swap and travel times
	for( unsigned i = 0; i < numSpots; ++i )
		spotTravelTimeTable[i] = LittleShort( spotTravelTimeTable[i] );

	// Spot visibility does not need neither byte swap nor validation being just an unsigned byte
	static_assert( sizeof( *spotVisibilityTable ) == 1, "" );

	// Byte swap and validate offsets
	for( unsigned i = 0; i < numGridCells; ++i ) {
		gridListOffsets[i] = LittleLong( gridListOffsets[i] );
		if( gridListOffsets[i] >= gridListsArraySize ) {
			G_Printf( S_COLOR_RED "%s: Bogus grid list offset %d for cell %d\n", function, gridListOffsets[i], i );
			return false;
		}
	}

	// Byte swap and validate lists
	for( unsigned i = 0; i < numGridCells; ++i ) {
		uint16_t *gridSpotsList = gridSpotsLists + gridListOffsets[i];
		// Byte swap and validate list spots number
		uint16_t numListSpots = gridSpotsList[0] = LittleShort( gridSpotsList[0] );
		if( gridListOffsets[i] + numListSpots > gridListsArraySize ) {
			G_Printf( S_COLOR_RED "%s: Bogus grid list num spots %d for cell %d", function, numListSpots, i );
			return false;
		}

		// Byte swap grid spot list nums
		for( uint16_t j = 0; j < numListSpots; ++j )
			gridSpotsList[j + 1] = LittleShort( gridSpotsList[j + 1] );
	}

	messagePrinter.CancelPendingMessage();
	return true;
}

void TacticalSpotsRegistry::SavePrecomputedData( const char *mapname ) {
	char filename[MAX_QPATH];
	Q_snprintfz( filename, MAX_QPATH, "ai/%s.nav.cache", mapname );

	ScopedMessagePrinter messagePrinter( S_COLOR_RED "Can't save %s\n", filename );

	int fp;
	if( trap_FS_FOpenFile( filename, &fp, FS_WRITE ) < 0 ) {
		return;
	}

	FileCloseGuard fileCloseGuard( fp );
	FileRemoveGuard fileRemoveGuard( filename );

	uint32_t version = LittleLong( PRECOMPUTED_DATA_VERSION );
	if( trap_FS_Write( &version, 4, fp ) != 4 ) {
		return;
	}

	uint32_t dataLength;
	const char *mapChecksum = trap_GetConfigString( CS_MAPCHECKSUM );
	dataLength = (uint32_t)strlen( mapChecksum ) + 1;
	if( !WriteLengthAndData( mapChecksum, dataLength, fp ) ) {
		return;
	}

	const char *aasChecksum = AiAasWorld::Instance()->Checksum();
	dataLength = (uint32_t)strlen( aasChecksum ) + 1;
	if( !WriteLengthAndData( aasChecksum, dataLength, fp ) ) {
		return;
	}

	// Byte swap spots
	static_assert( sizeof( spots->aasAreaNum ) == 4, "LittleLong() is not applicable" );
	for( unsigned i = 0; i < numSpots; ++i ) {
		auto &spot = spots[i];
		spot.aasAreaNum = LittleLong( spot.aasAreaNum );
		for( int j = 0; j < 3; ++j ) {
			spot.origin[j] = LittleFloat( spot.origin[j] );
			spot.absMins[j] = LittleFloat( spot.origin[j] );
			spot.absMaxs[j] = LittleFloat( spot.origin[j] );
		}
	}

	dataLength = numSpots * sizeof( TacticalSpot );
	if( !WriteLengthAndData( (const char *)spots, dataLength, fp ) ) {
		return;
	}

	// Prevent using byte-swapped spots
	G_LevelFree( spots );
	spots = nullptr;

	// Byte swap travel times
	static_assert( sizeof( *spotTravelTimeTable ) == 2, "LittleShort() is not applicable" );
	for( unsigned i = 0, end = numSpots * numSpots; i < end; ++i )
		spotTravelTimeTable[i] = LittleShort( spotTravelTimeTable[i] );

	dataLength = numSpots * numSpots * sizeof( *spotTravelTimeTable );
	if( !WriteLengthAndData( (const char *)spotTravelTimeTable, dataLength, fp ) ) {
		return;
	}

	// Prevent using byte-swapped travel times table
	G_LevelFree( spotTravelTimeTable );
	spotTravelTimeTable = nullptr;

	static_assert( sizeof( *spotVisibilityTable ) == 1, "Byte swapping is required" );
	dataLength = numSpots * numSpots * sizeof( *spotVisibilityTable );
	if( !WriteLengthAndData( (const char *)spotVisibilityTable, dataLength, fp ) ) {
		return;
	}

	// Release the data for conformance with the rest of the saved data
	G_LevelFree( spotVisibilityTable );
	spotVisibilityTable = nullptr;

	// Byte swap grid list offsets and grid spots lists
	static_assert( sizeof( *gridListOffsets ) == 4, "LittleLong() is not applicable" );
	for( unsigned i = 0, end = NumGridCells(); i < end; ++i ) {
		unsigned listOffset = gridListOffsets[i];
		gridListOffsets[i] = LittleLong( gridListOffsets[i] );
		uint16_t *list = gridSpotsLists + listOffset;
		unsigned numListSpots = *list;
		// Byte-swap the list data (number of lists spots and the spots nums)
		for( unsigned j = 0; j < numListSpots + 1; ++j )
			list[j] = LittleShort( list[j] );
	}

	dataLength = NumGridCells() * sizeof( *gridListOffsets );
	if( !WriteLengthAndData( (const char *)gridListOffsets, dataLength, fp ) ) {
		return;
	}

	// Prevent using byte-swapped grid list offsets
	G_LevelFree( gridListOffsets );
	gridListOffsets = nullptr;

	dataLength = sizeof( uint16_t ) * ( NumGridCells() + numSpots );
	if( !WriteLengthAndData( (const char *)gridSpotsLists, dataLength, fp ) ) {
		return;
	}

	// Prevent using byte-swapped grid spots lists
	G_LevelFree( gridSpotsLists );
	gridSpotsLists = nullptr;

	fileRemoveGuard.CancelPendingRemoval();
	messagePrinter.CancelPendingMessage();

	G_Printf( "The precomputed nav data file %s has been saved successfully\n", filename );
}

void TacticalSpotsRegistry::Shutdown() {
	instance.~TacticalSpotsRegistry();
}

TacticalSpotsRegistry::~TacticalSpotsRegistry() {
	if( needsSavingPrecomputedData ) {
		SavePrecomputedData( level.mapname );
		needsSavingPrecomputedData = false;
	}

	numSpots = 0;
	if( spots ) {
		G_LevelFree( spots );
		spots = nullptr;
	}
	if( spotVisibilityTable ) {
		G_LevelFree( spotVisibilityTable );
		spotVisibilityTable = nullptr;
	}
	if( spotTravelTimeTable ) {
		G_LevelFree( spotTravelTimeTable );
		spotTravelTimeTable = nullptr;
	}
	if( gridListOffsets ) {
		G_LevelFree( gridListOffsets );
		gridListOffsets = nullptr;
	}
	if( gridSpotsLists ) {
		G_LevelFree( gridSpotsLists );
		gridSpotsLists = nullptr;
	}
}

void TacticalSpotsRegistry::ComputeMutualSpotsVisibility() {
	G_Printf( "Computing mutual tactical spots visibility (it might take a while)...\n" );

	if( spotVisibilityTable ) {
		G_LevelFree( spotVisibilityTable );
	}

	spotVisibilityTable = (unsigned char *)G_LevelMalloc( numSpots * numSpots );

	float *mins = vec3_origin;
	float *maxs = vec3_origin;

	trace_t trace;
	for( unsigned i = 0; i < numSpots; ++i ) {
		// Consider each spot visible to itself
		spotVisibilityTable[i * numSpots + i] = 255;

		TacticalSpot &currSpot = spots[i];
		vec3_t currSpotBounds[2];
		VectorCopy( currSpot.absMins, currSpotBounds[0] );
		VectorCopy( currSpot.absMaxs, currSpotBounds[1] );

		// Mutual visibility for spots [0, i) has been already computed
		for( unsigned j = i + 1; j < numSpots; ++j ) {
			TacticalSpot &testedSpot = spots[j];
			if( !trap_inPVS( currSpot.origin, testedSpot.origin ) ) {
				spotVisibilityTable[j * numSpots + i] = 0;
				spotVisibilityTable[i * numSpots + j] = 0;
				continue;
			}

			unsigned char visibility = 0;
			vec3_t testedSpotBounds[2];
			VectorCopy( testedSpot.absMins, testedSpotBounds[0] );
			VectorCopy( testedSpot.absMaxs, testedSpotBounds[1] );

			// Do not test against any entities using G_Trace() as these tests are expensive and pointless in this case.
			// Test only against the solid world.
			SolidWorldTrace( &trace, currSpot.origin, testedSpot.origin, mins, maxs );
			bool areOriginsMutualVisible = ( trace.fraction == 1.0f );

			for( unsigned n = 0; n < 8; ++n ) {
				float from[] =
				{
					currSpotBounds[( n >> 2 ) & 1][0],
					currSpotBounds[( n >> 1 ) & 1][1],
					currSpotBounds[( n >> 0 ) & 1][2]
				};
				for( unsigned m = 0; m < 8; ++m ) {
					float to[] =
					{
						testedSpotBounds[( m >> 2 ) & 1][0],
						testedSpotBounds[( m >> 1 ) & 1][1],
						testedSpotBounds[( m >> 0 ) & 1][2]
					};
					SolidWorldTrace( &trace, from, to, mins, maxs );
					// If all 64 traces succeed, the visibility is a half of the maximal score
					visibility += 2 * (unsigned char)trace.fraction;
				}
			}

			// Prevent marking of the most significant bit
			if( visibility == 128 ) {
				visibility = 127;
			}

			// Mutual origins visibility counts is a half of the maximal score.
			// Also, if the most significant bit of visibility bits is set, spot origins are mutually visible
			if( areOriginsMutualVisible ) {
				visibility |= 128;
			}

			spotVisibilityTable[i * numSpots + j] = visibility;
			spotVisibilityTable[j * numSpots + i] = visibility;
		}
	}
}

void TacticalSpotsRegistry::ComputeMutualSpotsReachability() {
	G_Printf( "Computing mutual tactical spots reachability (it might take a while)...\n" );

	if( spotTravelTimeTable ) {
		G_LevelFree( spotTravelTimeTable );
	}

	spotTravelTimeTable = (unsigned short *)G_LevelMalloc( sizeof( unsigned short ) * numSpots * numSpots );
	const int flags = Bot::ALLOWED_TRAVEL_FLAGS;
	AiAasRouteCache *routeCache = AiAasRouteCache::Shared();
	// Note: spots reachabilities are not reversible
	// (for spots two A and B reachabilies A->B and B->A might differ, including being invalid, non-existent)
	// Thus we have to find a reachability for each possible pair of spots
	for( unsigned i = 0; i < numSpots; ++i ) {
		const int currAreaNum = spots[i].aasAreaNum;
		for( unsigned j = 0; j < i; ++j ) {
			const int testedAreaNum = spots[j].aasAreaNum;
			const int travelTime = routeCache->TravelTimeToGoalArea( currAreaNum, testedAreaNum, flags );
			// AAS uses short for travel time computation. If one changes it, this assertion might be triggered.
			assert( travelTime <= std::numeric_limits<unsigned short>::max() );
			spotTravelTimeTable[i * numSpots + j] = (unsigned short)travelTime;
		}
		// Set the lowest feasible travel time value for traveling from the curr spot to the curr spot itself.
		spotTravelTimeTable[i * numSpots + i] = 1;
		for( unsigned j = i + 1; j < numSpots; ++j ) {
			const int testedAreaNum = spots[j].aasAreaNum;
			const int travelTime = routeCache->TravelTimeToGoalArea( currAreaNum, testedAreaNum, flags );
			assert( travelTime <= std::numeric_limits<unsigned short>::max() );
			spotTravelTimeTable[i * numSpots + j] = (unsigned short)travelTime;
		}
	}
}

void TacticalSpotsRegistry::MakeSpotsGrid() {
	SetupGridParams();

	if( gridListOffsets ) {
		G_LevelFree( gridListOffsets );
	}

	if( gridSpotsLists ) {
		G_LevelFree( gridSpotsLists );
	}

	unsigned totalNumCells = NumGridCells();
	gridListOffsets = (uint32_t *)G_LevelMalloc( sizeof( uint32_t ) * totalNumCells );
	// For each cell at least 1 short value is used to store spots count.
	// Also totalNumCells short values are required to store spot nums
	// assuming that each spot belongs to a single cell.
	gridSpotsLists = (uint16_t *)G_LevelMalloc( sizeof( uint16_t ) * ( totalNumCells + numSpots ) );

	uint16_t *listPtr = gridSpotsLists;
	// For each cell of all possible cells
	for( unsigned cellNum = 0; cellNum < totalNumCells; ++cellNum ) {
		// Store offset of the cell spots list
		gridListOffsets[cellNum] = (unsigned)( listPtr - gridSpotsLists );
		// Use a reference to cell spots list head that contains number of spots in the cell
		uint16_t &listSize = listPtr[0];
		listSize = 0;
		// Skip list head
		++listPtr;
		// For each loaded spot
		for( uint16_t spotNum = 0; spotNum < numSpots; ++spotNum ) {
			auto pointCellNum = PointGridCellNum( spots[spotNum].origin );
			// If the spot belongs to the cell
			if( pointCellNum == cellNum ) {
				*listPtr = spotNum;
				++listPtr;
				if( listPtr - gridSpotsLists > totalNumCells + numSpots ) {
					abort();
				}
				listSize++;
			}
		}
	}
}

void TacticalSpotsRegistry::SetupGridParams() {
	// Get world bounds
	trap_CM_InlineModelBounds( trap_CM_InlineModel( 0 ), worldMins, worldMaxs );

	vec3_t worldDims;
	VectorSubtract( worldMaxs, worldMins, worldDims );

	for( int i = 0; i < 3; ++i ) {
		unsigned roundedDimension = (unsigned)worldDims[i];
		if( roundedDimension > MIN_GRID_CELL_SIDE * MAX_GRID_DIMENSION ) {
			gridCellSize[i] = roundedDimension / MAX_GRID_DIMENSION;
			gridNumCells[i] = MAX_GRID_DIMENSION;
		} else {
			gridCellSize[i] = MIN_GRID_CELL_SIDE;
			gridNumCells[i] = ( roundedDimension / MIN_GRID_CELL_SIDE ) + 1;
		}
	}
}

uint16_t TacticalSpotsRegistry::FindSpotsInRadius( const OriginParams &originParams,
												   unsigned short *spotNums,
												   unsigned short *insideSpotNum ) const {
	if( !IsLoaded() ) {
		abort();
	}

	vec3_t boxMins, boxMaxs;
	VectorCopy( originParams.origin, boxMins );
	VectorCopy( originParams.origin, boxMaxs );
	const float radius = originParams.searchRadius;
	vec3_t radiusBounds = { radius, radius, radius };
	VectorSubtract( boxMins, radiusBounds, boxMins );
	VectorAdd( boxMaxs, radiusBounds, boxMaxs );

	// Find loop bounds for each dimension
	unsigned minCellDimIndex[3];
	unsigned maxCellDimIndex[3];
	for( int i = 0; i < 3; ++i ) {
		// Clamp box bounds by world bounds
		clamp( boxMins[i], worldMins[i], worldMaxs[i] );
		clamp( boxMaxs[i], worldMins[i], worldMaxs[i] );

		// Convert box bounds to relative
		boxMins[i] -= worldMins[i];
		boxMaxs[i] -= worldMins[i];

		minCellDimIndex[i] = (unsigned)( boxMins[i] / gridCellSize[i] );
		maxCellDimIndex[i] = (unsigned)( boxMaxs[i] / gridCellSize[i] );
	}

	// Avoid unsigned wrapping
	static_assert( MAX_SPOTS < std::numeric_limits<uint16_t>::max(), "" );
	*insideSpotNum = MAX_SPOTS + 1;

	// Copy to locals for faster access
	const Vec3 searchOrigin( originParams.origin );
	const float squareRadius = originParams.searchRadius * originParams.searchRadius;
	uint16_t numSpotsInRadius = 0;
	// For each index for X dimension in the query bounding box
	for( unsigned i = minCellDimIndex[0]; i <= maxCellDimIndex[0]; ++i ) {
		unsigned indexIOffset = i * ( gridNumCells[1] * gridNumCells[2] );
		// For each index for Y dimension in the query bounding box
		for( unsigned j = minCellDimIndex[1]; j <= maxCellDimIndex[1]; ++j ) {
			unsigned indexJOffset = j * gridNumCells[2];
			// For each index for Z dimension in the query bounding box
			for( unsigned k = minCellDimIndex[2]; k <= maxCellDimIndex[2]; ++k ) {
				// The cell is at this offset from the beginning of a linear cells array
				unsigned cellIndex = indexIOffset + indexJOffset + k;
				// Get the offset of the list of spot nums for the cell
				unsigned gridListOffset = gridListOffsets[cellIndex];
				uint16_t *spotsList = gridSpotsLists + gridListOffset;
				// List head contains the count of spots (spot numbers)
				uint16_t numGridSpots = spotsList[0];
				// Skip list head
				spotsList++;
				// For each spot number fetch a spot and test against the problem params
				for( uint16_t spotNumIndex = 0; spotNumIndex < numGridSpots; ++spotNumIndex ) {
					uint16_t spotNum = spotsList[spotNumIndex];
					const TacticalSpot &spot = spots[spotNum];
					if( DistanceSquared( spot.origin, searchOrigin.Data() ) < squareRadius ) {
						spotNums[numSpotsInRadius++] = spotNum;
						// Test whether search origin is inside the spot
						if( searchOrigin.X() < spot.absMins[0] || searchOrigin.X() > spot.absMaxs[0] ) {
							continue;
						}
						if( searchOrigin.Y() < spot.absMins[1] || searchOrigin.Y() > spot.absMaxs[1] ) {
							continue;
						}
						if( searchOrigin.Z() < spot.absMins[2] || searchOrigin.Z() > spot.absMaxs[2] ) {
							continue;
						}
						// Spots should not overlap. But if spots overlap, last matching spot will be returned
						*insideSpotNum = spotNum;
					}
				}
			}
		}
	}

	return numSpotsInRadius;
}

int TacticalSpotsRegistry::CopyResults( const SpotAndScore *spotsBegin,
										const SpotAndScore *spotsEnd,
										const CommonProblemParams &problemParams,
										vec3_t *spotOrigins, int maxSpots ) const {
	const unsigned resultsSize = (unsigned)( spotsEnd - spotsBegin );
	if( maxSpots == 0 || resultsSize == 0 ) {
		return 0;
	}

	// Its a common case so give it an optimized branch
	if( maxSpots == 1 ) {
		VectorCopy( spots[spotsBegin->spotNum].origin, spotOrigins[0] );
		return 1;
	}

	const float spotProximityThreshold = problemParams.spotProximityThreshold;

	bool isSpotExcluded[CandidateSpots::capacity()];
	memset( isSpotExcluded, 0, sizeof( bool ) * CandidateSpots::capacity() );

	int numSpots = 0;
	unsigned keptSpotIndex = 0;
	for(;; ) {
		if( keptSpotIndex >= resultsSize ) {
			return numSpots;
		}
		if( numSpots >= maxSpots ) {
			return numSpots;
		}

		// Spots are sorted by score.
		// So first spot not marked as excluded yet has higher priority and should be kept.

		const TacticalSpot &keptSpot = spots[spotsBegin[keptSpotIndex].spotNum];
		VectorCopy( keptSpot.origin, spotOrigins[numSpots] );
		++numSpots;

		// Exclude all next (i.e. lower score) spots that are too close to the kept spot.

		unsigned testedSpotIndex = keptSpotIndex + 1;
		keptSpotIndex = 999999;
		for(; testedSpotIndex < resultsSize; testedSpotIndex++ ) {
			// Skip already excluded areas
			if( isSpotExcluded[testedSpotIndex] ) {
				continue;
			}

			const TacticalSpot &testedSpot = spots[spotsBegin[testedSpotIndex].spotNum];
			if( DistanceSquared( keptSpot.origin, testedSpot.origin ) < spotProximityThreshold * spotProximityThreshold ) {
				isSpotExcluded[testedSpotIndex] = true;
			} else if( keptSpotIndex > testedSpotIndex ) {
				keptSpotIndex = testedSpotIndex;
			}
		}
	}
}

inline float ComputeDistanceFactor( float distance, float weightFalloffDistanceRatio, float searchRadius ) {
	float weightFalloffRadius = weightFalloffDistanceRatio * searchRadius;
	if( distance < weightFalloffRadius ) {
		return distance / weightFalloffRadius;
	}

	return 1.0f - ( ( distance - weightFalloffRadius ) / ( 0.000001f + searchRadius - weightFalloffRadius ) );
}

inline float ComputeDistanceFactor( const vec3_t v1, const vec3_t v2, float weightFalloffDistanceRatio, float searchRadius ) {
	float squareDistance = DistanceSquared( v1, v2 );
	float distance = 1.0f;
	if( squareDistance >= 1.0f ) {
		distance = 1.0f / Q_RSqrt( squareDistance );
	}

	return ComputeDistanceFactor( distance, weightFalloffDistanceRatio, searchRadius );
}

// Units of travelTime and maxFeasibleTravelTime must match!
inline float ComputeTravelTimeFactor( int travelTime, float maxFeasibleTravelTime ) {
	float factor = 1.0f - BoundedFraction( travelTime, maxFeasibleTravelTime );
	return 1.0f / Q_RSqrt( 0.0001f + factor );
}

inline float ApplyFactor( float value, float factor, float factorInfluence ) {
	float keptPart = value * ( 1.0f - factorInfluence );
	float modifiedPart = value * factor * factorInfluence;
	return keptPart + modifiedPart;
}

int TacticalSpotsRegistry::FindPositionalAdvantageSpots( const OriginParams &originParams,
														 const AdvantageProblemParams &problemParams,
														 vec3_t *spotOrigins, int maxSpots ) const {
	uint16_t boundsSpots[MAX_SPOTS];
	uint16_t insideSpotNum;
	uint16_t numSpotsInBounds = FindSpotsInRadius( originParams, boundsSpots, &insideSpotNum );

	CandidateSpots candidateSpots;
	SelectCandidateSpots( originParams, problemParams, boundsSpots, numSpotsInBounds, candidateSpots );

	ReachCheckedSpots reachCheckedSpots;
	if( problemParams.checkToAndBackReachability ) {
		CheckSpotsReachFromOriginAndBack( originParams, problemParams, candidateSpots,  insideSpotNum, reachCheckedSpots );
	} else {
		CheckSpotsReachFromOrigin( originParams, problemParams, candidateSpots, insideSpotNum, reachCheckedSpots );
	}

	TraceCheckedSpots traceCheckedSpots;
	CheckSpotsVisibleOriginTrace( originParams, problemParams, reachCheckedSpots, traceCheckedSpots );

	SortByVisAndOtherFactors( originParams, problemParams, traceCheckedSpots );

	return CopyResults( traceCheckedSpots.begin(), traceCheckedSpots.end(), problemParams, spotOrigins, maxSpots );
}

void TacticalSpotsRegistry::SelectCandidateSpots( const OriginParams &originParams,
												  const CommonProblemParams &problemParams,
												  const uint16_t *spotNums,
												  uint16_t numSpots, CandidateSpots &result ) const {
	const float minHeightAdvantageOverOrigin = problemParams.minHeightAdvantageOverOrigin;
	const float heightOverOriginInfluence = problemParams.heightOverOriginInfluence;
	const float searchRadius = originParams.searchRadius;
	const float originZ = originParams.origin[2];
	// Copy to stack for faster access
	Vec3 origin( originParams.origin );

	for( unsigned i = 0; i < numSpots && result.size() < result.capacity(); ++i ) {
		const TacticalSpot &spot = spots[spotNums[i]];

		float heightOverOrigin = spot.absMins[2] - originZ;
		if( heightOverOrigin < minHeightAdvantageOverOrigin ) {
			continue;
		}

		float squareDistanceToOrigin = DistanceSquared( origin.Data(), spot.origin );
		if( squareDistanceToOrigin > searchRadius * searchRadius ) {
			continue;
		}

		float score = 1.0f;
		float factor = BoundedFraction( heightOverOrigin - minHeightAdvantageOverOrigin, searchRadius );
		score = ApplyFactor( score, factor, heightOverOriginInfluence );

		result.push_back( SpotAndScore( spotNums[i], score ) );
	}

	// Sort result so best score areas are first
	std::sort( result.begin(), result.end() );
}

void TacticalSpotsRegistry::SelectCandidateSpots( const OriginParams &originParams,
												  const AdvantageProblemParams &problemParams,
												  const uint16_t *spotNums,
												  uint16_t numSpots, CandidateSpots &result ) const {
	const float minHeightAdvantageOverOrigin = problemParams.minHeightAdvantageOverOrigin;
	const float minHeightAdvantageOverEntity = problemParams.minHeightAdvantageOverEntity;
	const float heightOverOriginInfluence = problemParams.heightOverOriginInfluence;
	const float heightOverEntityInfluence = problemParams.heightOverEntityInfluence;
	const float minSquareDistanceToEntity = problemParams.minSpotDistanceToEntity * problemParams.minSpotDistanceToEntity;
	const float maxSquareDistanceToEntity = problemParams.maxSpotDistanceToEntity * problemParams.maxSpotDistanceToEntity;
	const float searchRadius = originParams.searchRadius;
	const float originZ = originParams.origin[2];
	const float entityZ = problemParams.keepVisibleOrigin[2];
	// Copy to stack for faster access
	Vec3 origin( originParams.origin );
	Vec3 entityOrigin( problemParams.keepVisibleOrigin );

	for( unsigned i = 0; i < numSpots && result.size() < result.capacity(); ++i ) {
		const TacticalSpot &spot = spots[spotNums[i]];

		float heightOverOrigin = spot.absMins[2] - originZ;
		if( heightOverOrigin < minHeightAdvantageOverOrigin ) {
			continue;
		}

		float heightOverEntity = spot.absMins[2] - entityZ;
		if( heightOverEntity < minHeightAdvantageOverEntity ) {
			continue;
		}

		float squareDistanceToOrigin = DistanceSquared( origin.Data(), spot.origin );
		if( squareDistanceToOrigin > searchRadius * searchRadius ) {
			continue;
		}

		float squareDistanceToEntity = DistanceSquared( entityOrigin.Data(), spot.origin );
		if( squareDistanceToEntity < minSquareDistanceToEntity ) {
			continue;
		}
		if( squareDistanceToEntity > maxSquareDistanceToEntity ) {
			continue;
		}

		float score = 1.0f;
		float factor;
		factor = BoundedFraction( heightOverOrigin - minHeightAdvantageOverOrigin, searchRadius );
		score = ApplyFactor( score, factor, heightOverOriginInfluence );
		factor = BoundedFraction( heightOverEntity - minHeightAdvantageOverEntity, searchRadius );
		score = ApplyFactor( score, factor, heightOverEntityInfluence );

		result.push_back( SpotAndScore( spotNums[i], score ) );
	}

	// Sort result so best score areas are first
	std::sort( result.begin(), result.end() );
}

void TacticalSpotsRegistry::CheckSpotsReachFromOrigin( const OriginParams &originParams,
													   const CommonProblemParams &problemParams,
													   const CandidateSpots &candidateSpots,
													   uint16_t insideSpotNum,
													   ReachCheckedSpots &result ) const {
	AiAasRouteCache *routeCache = originParams.routeCache;
	const int originAreaNum = originParams.originAreaNum;
	const float *origin = originParams.origin;
	const float searchRadius = originParams.searchRadius;
	// AAS uses travel time in centiseconds
	const int maxFeasibleTravelTimeCentis = problemParams.maxFeasibleTravelTimeMillis / 10;
	const float weightFalloffDistanceRatio = problemParams.originWeightFalloffDistanceRatio;
	const float distanceInfluence = problemParams.originDistanceInfluence;
	const float travelTimeInfluence = problemParams.travelTimeInfluence;

	// Do not more than result.capacity() iterations.
	// Some feasible areas in candidateAreas tai that pass test may be skipped,
	// but this is intended to reduce performance drops (do not more than result.capacity() pathfinder calls).
	if( insideSpotNum < MAX_SPOTS ) {
		const auto *travelTimeTable = this->spotTravelTimeTable;
		const auto tableRowOffset = insideSpotNum * this->numSpots;
		for( unsigned i = 0, end = std::min( candidateSpots.size(), result.capacity() ); i < end; ++i ) {
			const SpotAndScore &spotAndScore = candidateSpots[i];
			const TacticalSpot &spot = spots[spotAndScore.spotNum];
			// If zero, the spotNum spot is not reachable from insideSpotNum
			int tableTravelTime = travelTimeTable[tableRowOffset + spotAndScore.spotNum];
			if( !tableTravelTime || tableTravelTime > maxFeasibleTravelTimeCentis ) {
				continue;
			}

			// Get an actual travel time (non-zero table value does not guarantee reachability)
			int travelTime = routeCache->TravelTimeToGoalArea( originAreaNum, spot.aasAreaNum, Bot::ALLOWED_TRAVEL_FLAGS );
			if( !travelTime || travelTime > maxFeasibleTravelTimeCentis ) {
				continue;
			}

			float travelTimeFactor = 1.0f - ComputeTravelTimeFactor( travelTime, maxFeasibleTravelTimeCentis );
			float distanceFactor = ComputeDistanceFactor( spot.origin, origin, weightFalloffDistanceRatio, searchRadius );
			float newScore = spotAndScore.score;
			newScore = ApplyFactor( newScore, distanceFactor, distanceInfluence );
			newScore = ApplyFactor( newScore, travelTimeFactor, travelTimeInfluence );
			result.push_back( SpotAndScore( spotAndScore.spotNum, newScore ) );
		}
	} else {
		for( unsigned i = 0, end = std::min( candidateSpots.size(), result.capacity() ); i < end; ++i ) {
			const SpotAndScore &spotAndScore = candidateSpots[i];
			const TacticalSpot &spot = spots[spotAndScore.spotNum];
			int travelTime = routeCache->TravelTimeToGoalArea( originAreaNum, spot.aasAreaNum, Bot::ALLOWED_TRAVEL_FLAGS );
			if( !travelTime || travelTime > maxFeasibleTravelTimeCentis ) {
				continue;
			}

			float travelTimeFactor = 1.0f - ComputeTravelTimeFactor( travelTime, maxFeasibleTravelTimeCentis );
			float distanceFactor = ComputeDistanceFactor( spot.origin, origin, weightFalloffDistanceRatio, searchRadius );
			float newScore = spotAndScore.score;
			newScore = ApplyFactor( newScore, distanceFactor, distanceInfluence );
			newScore = ApplyFactor( newScore, travelTimeFactor, travelTimeInfluence );
			result.push_back( SpotAndScore( spotAndScore.spotNum, newScore ) );
		}
	}

	// Sort result so best score areas are first
	std::sort( result.begin(), result.end() );
}

void TacticalSpotsRegistry::CheckSpotsReachFromOriginAndBack( const OriginParams &originParams,
															  const CommonProblemParams &problemParams,
															  const CandidateSpots &candidateSpots,
															  uint16_t insideSpotNum,
															  ReachCheckedSpots &result ) const {
	AiAasRouteCache *routeCache = originParams.routeCache;
	const int originAreaNum = originParams.originAreaNum;
	const float *origin = originParams.origin;
	const float searchRadius = originParams.searchRadius;
	// AAS uses time in centiseconds
	const int maxFeasibleTravelTimeCentis = problemParams.maxFeasibleTravelTimeMillis / 10;
	const float weightFalloffDistanceRatio = problemParams.originWeightFalloffDistanceRatio;
	const float distanceInfluence = problemParams.originDistanceInfluence;
	const float travelTimeInfluence = problemParams.travelTimeInfluence;

	// Do not more than result.capacity() iterations.
	// Some feasible areas in candidateAreas tai that pass test may be skipped,
	// but it is intended to reduce performance drops (do not more than 2 * result.capacity() pathfinder calls).
	if( insideSpotNum < MAX_SPOTS ) {
		const auto *travelTimeTable = this->spotTravelTimeTable;
		const auto numSpots = this->numSpots;
		for( unsigned i = 0, end = std::min( candidateSpots.size(), result.capacity() ); i < end; ++i ) {
			const SpotAndScore &spotAndScore = candidateSpots[i];
			const TacticalSpot &spot = spots[spotAndScore.spotNum];

			// If the table element i * numSpots + j is zero, j-th spot is not reachable from i-th one.
			int tableToTravelTime = travelTimeTable[insideSpotNum * numSpots + spotAndScore.spotNum];
			if( !tableToTravelTime ) {
				continue;
			}
			int tableBackTravelTime = travelTimeTable[spotAndScore.spotNum * numSpots + insideSpotNum];
			if( !tableBackTravelTime ) {
				continue;
			}
			if( tableToTravelTime + tableBackTravelTime > maxFeasibleTravelTimeCentis ) {
				continue;
			}

			// Get an actual travel time (non-zero table values do not guarantee reachability)
			int toTravelTime = routeCache->TravelTimeToGoalArea( originAreaNum, spot.aasAreaNum, Bot::ALLOWED_TRAVEL_FLAGS );
			// If `to` travel time is apriori greater than maximum allowed one (and thus the sum would be), reject early.
			if( !toTravelTime || toTravelTime > maxFeasibleTravelTimeCentis ) {
				continue;
			}
			int backTimeTravelTime = routeCache->TravelTimeToGoalArea( spot.aasAreaNum, originAreaNum, Bot::ALLOWED_TRAVEL_FLAGS );
			if( !backTimeTravelTime || toTravelTime + backTimeTravelTime > maxFeasibleTravelTimeCentis ) {
				continue;
			}

			int totalTravelTimeCentis = toTravelTime + backTimeTravelTime;
			float travelTimeFactor = ComputeTravelTimeFactor( totalTravelTimeCentis, maxFeasibleTravelTimeCentis );
			float distanceFactor = ComputeDistanceFactor( spot.origin, origin, weightFalloffDistanceRatio, searchRadius );
			float newScore = spotAndScore.score;
			newScore = ApplyFactor( newScore, distanceFactor, distanceInfluence );
			newScore = ApplyFactor( newScore, travelTimeFactor, travelTimeInfluence );
			result.push_back( SpotAndScore( spotAndScore.spotNum, newScore ) );
		}
	} else {
		for( unsigned i = 0, end = std::min( candidateSpots.size(), result.capacity() ); i < end; ++i ) {
			const SpotAndScore &spotAndScore = candidateSpots[i];
			const TacticalSpot &spot = spots[spotAndScore.spotNum];
			int toSpotTime = routeCache->TravelTimeToGoalArea( originAreaNum, spot.aasAreaNum, Bot::ALLOWED_TRAVEL_FLAGS );
			if( !toSpotTime ) {
				continue;
			}
			int toEntityTime = routeCache->TravelTimeToGoalArea( spot.aasAreaNum, originAreaNum, Bot::ALLOWED_TRAVEL_FLAGS );
			if( !toEntityTime ) {
				continue;
			}

			int totalTravelTimeCentis = 10 * ( toSpotTime + toEntityTime );
			float travelTimeFactor = ComputeTravelTimeFactor( totalTravelTimeCentis, maxFeasibleTravelTimeCentis );
			float distanceFactor = ComputeDistanceFactor( spot.origin, origin, weightFalloffDistanceRatio, searchRadius );
			float newScore = spotAndScore.score;
			newScore = ApplyFactor( newScore, distanceFactor, distanceInfluence );
			newScore = ApplyFactor( newScore, travelTimeFactor, travelTimeInfluence );
			result.push_back( SpotAndScore( spotAndScore.spotNum, newScore ) );
		}
	}

	// Sort result so best score areas are first
	std::sort( result.begin(), result.end() );
}

void TacticalSpotsRegistry::CheckSpotsVisibleOriginTrace( const OriginParams &originParams,
														  const AdvantageProblemParams &problemParams,
														  const ReachCheckedSpots &candidateSpots,
														  TraceCheckedSpots &result ) const {
	edict_t *passent = const_cast<edict_t*>( originParams.originEntity );
	edict_t *keepVisibleEntity = const_cast<edict_t *>( problemParams.keepVisibleEntity );
	Vec3 entityOrigin( problemParams.keepVisibleOrigin );
	// If not only origin but an entity too is supplied
	if( keepVisibleEntity ) {
		// Its a good idea to add some offset from the ground
		entityOrigin.Z() += 0.66f * keepVisibleEntity->r.maxs[2];
	}
	// Copy to locals for faster access
	const edict_t *gameEdicts = game.edicts;

	trace_t trace;
	// Do not more than result.capacity() iterations
	// (do not do more than result.capacity() traces even if it may cause loose of feasible areas).
	for( unsigned i = 0, end = std::min( candidateSpots.size(), result.capacity() ); i < end; ++i ) {
		const SpotAndScore &spotAndScore = candidateSpots[i];
		G_Trace( &trace, spots[spotAndScore.spotNum].origin, nullptr, nullptr, entityOrigin.Data(), passent, MASK_AISOLID );
		if( trace.fraction == 1.0f || gameEdicts + trace.ent == keepVisibleEntity ) {
			result.push_back( spotAndScore );
		}
	}
}

void TacticalSpotsRegistry::SortByVisAndOtherFactors( const OriginParams &originParams,
													  const AdvantageProblemParams &problemParams,
													  TraceCheckedSpots &result ) const {
	const Vec3 origin( originParams.origin );
	const Vec3 entityOrigin( problemParams.keepVisibleOrigin );
	const float originZ = originParams.origin[2];
	const float entityZ = problemParams.keepVisibleOrigin[2];
	const float searchRadius = originParams.searchRadius;
	const float minHeightAdvantageOverOrigin = problemParams.minHeightAdvantageOverOrigin;
	const float heightOverOriginInfluence = problemParams.heightOverOriginInfluence;
	const float minHeightAdvantageOverEntity = problemParams.minHeightAdvantageOverEntity;
	const float heightOverEntityInfluence = problemParams.heightOverEntityInfluence;
	const float originWeightFalloffDistanceRatio = problemParams.originWeightFalloffDistanceRatio;
	const float originDistanceInfluence = problemParams.originDistanceInfluence;
	const float entityWeightFalloffDistanceRatio = problemParams.entityWeightFalloffDistanceRatio;
	const float entityDistanceInfluence = problemParams.entityDistanceInfluence;
	const float minSpotDistanceToEntity = problemParams.minSpotDistanceToEntity;
	const float entityDistanceRange = problemParams.maxSpotDistanceToEntity - problemParams.minSpotDistanceToEntity;

	const unsigned resultSpotsSize = result.size();
	if( resultSpotsSize <= 1 ) {
		return;
	}

	for( unsigned i = 0; i < resultSpotsSize; ++i ) {
		unsigned visibilitySum = 0;
		unsigned testedSpotNum = result[i].spotNum;
		// Get address of the visibility table row
		unsigned char *spotVisibilityForSpotNum = spotVisibilityTable + testedSpotNum * numSpots;

		for( unsigned j = 0; j < i; ++j )
			visibilitySum += spotVisibilityForSpotNum[j];

		// Skip i-th index

		for( unsigned j = i + 1; j < resultSpotsSize; ++j )
			visibilitySum += spotVisibilityForSpotNum[j];

		const TacticalSpot &testedSpot = spots[testedSpotNum];
		float score = result[i].score;

		// The maximum possible visibility score for a pair of spots is 255
		float visFactor = visibilitySum / ( ( result.size() - 1 ) * 255 );
		visFactor = 1.0f / Q_RSqrt( visFactor );
		score *= visFactor;

		float heightOverOrigin = testedSpot.absMins[2] - originZ - minHeightAdvantageOverOrigin;
		float heightOverOriginFactor = BoundedFraction( heightOverOrigin, searchRadius - minHeightAdvantageOverOrigin );
		score = ApplyFactor( score, heightOverOriginFactor, heightOverOriginInfluence );

		float heightOverEntity = testedSpot.absMins[2] - entityZ - minHeightAdvantageOverEntity;
		float heightOverEntityFactor = BoundedFraction( heightOverEntity, searchRadius - minHeightAdvantageOverEntity );
		score = ApplyFactor( score, heightOverEntityFactor, heightOverEntityInfluence );

		float originDistance = 1.0f / Q_RSqrt( 0.001f + DistanceSquared( testedSpot.origin, origin.Data() ) );
		float originDistanceFactor = ComputeDistanceFactor( originDistance, originWeightFalloffDistanceRatio, searchRadius );
		score = ApplyFactor( score, originDistanceFactor, originDistanceInfluence );

		float entityDistance = 1.0f / Q_RSqrt( 0.001f + DistanceSquared( testedSpot.origin, entityOrigin.Data() ) );
		entityDistance -= minSpotDistanceToEntity;
		float entityDistanceFactor = ComputeDistanceFactor( entityDistance,
															entityWeightFalloffDistanceRatio,
															entityDistanceRange );
		score = ApplyFactor( score, entityDistanceFactor, entityDistanceInfluence );

		result[i].score = score;
	}

	// Sort results so best score spots are first
	std::stable_sort( result.begin(), result.end() );
}

int TacticalSpotsRegistry::FindCoverSpots( const OriginParams &originParams,
										   const CoverProblemParams &problemParams,
										   vec3_t *spotOrigins, int maxSpots ) const {
	uint16_t boundsSpots[MAX_SPOTS];
	uint16_t insideSpotNum;
	uint16_t numSpotsInBounds = FindSpotsInRadius( originParams, boundsSpots, &insideSpotNum );

	CandidateSpots candidateSpots;
	SelectCandidateSpots( originParams, problemParams, boundsSpots, numSpotsInBounds, candidateSpots );

	ReachCheckedSpots reachCheckedSpots;
	if( problemParams.checkToAndBackReachability ) {
		CheckSpotsReachFromOriginAndBack( originParams, problemParams, candidateSpots, insideSpotNum, reachCheckedSpots );
	} else {
		CheckSpotsReachFromOrigin( originParams, problemParams, candidateSpots, insideSpotNum, reachCheckedSpots );
	}

	TraceCheckedSpots coverSpots;
	SelectSpotsForCover( originParams, problemParams, reachCheckedSpots, coverSpots );

	return CopyResults( coverSpots.begin(), coverSpots.end(), problemParams, spotOrigins, maxSpots );
}

void TacticalSpotsRegistry::SelectSpotsForCover( const OriginParams &originParams,
												 const CoverProblemParams &problemParams,
												 const ReachCheckedSpots &candidateSpots,
												 TraceCheckedSpots &result ) const {
	// Do not do more than result.capacity() iterations
	for( unsigned i = 0, end = std::min( candidateSpots.size(), result.capacity() ); i < end; ++i ) {
		const SpotAndScore &spotAndScore = candidateSpots[i];
		if( LooksLikeACoverSpot( spotAndScore.spotNum, originParams, problemParams ) ) {
			result.push_back( spotAndScore );
		}
	}
	;
}

bool TacticalSpotsRegistry::LooksLikeACoverSpot( uint16_t spotNum, const OriginParams &originParams,
												 const CoverProblemParams &problemParams ) const {
	const TacticalSpot &spot = spots[spotNum];

	edict_t *passent = const_cast<edict_t *>( problemParams.attackerEntity );
	float *attackerOrigin = const_cast<float *>( problemParams.attackerOrigin );
	float *spotOrigin = const_cast<float *>( spot.origin );
	const edict_t *doNotHitEntity = originParams.originEntity;

	trace_t trace;
	G_Trace( &trace, attackerOrigin, nullptr, nullptr, spotOrigin, passent, MASK_AISOLID );
	if( trace.fraction == 1.0f ) {
		return false;
	}

	float harmfulRayThickness = problemParams.harmfulRayThickness;

	vec3_t bounds[2] =
	{
		{ -harmfulRayThickness, -harmfulRayThickness, -harmfulRayThickness },
		{ +harmfulRayThickness, +harmfulRayThickness, +harmfulRayThickness }
	};

	// Convert bounds from relative to absolute
	VectorAdd( bounds[0], spot.origin, bounds[0] );
	VectorAdd( bounds[1], spot.origin, bounds[1] );

	for( int i = 0; i < 8; ++i ) {
		vec3_t traceEnd;
		traceEnd[0] = bounds[( i >> 2 ) & 1][0];
		traceEnd[1] = bounds[( i >> 1 ) & 1][1];
		traceEnd[2] = bounds[( i >> 0 ) & 1][2];
		G_Trace( &trace, attackerOrigin, nullptr, nullptr, traceEnd, passent, MASK_AISOLID );
		if( trace.fraction == 1.0f || game.edicts + trace.ent == doNotHitEntity ) {
			return false;
		}
	}

	return true;
}

int TacticalSpotsRegistry::FindEvadeDangerSpots( const OriginParams &originParams,
												 const DodgeDangerProblemParams &problemParams,
												 vec3_t *spotOrigins, int maxSpots ) const {
	uint16_t boundsSpots[MAX_SPOTS];
	uint16_t insideSpotNum;
	uint16_t numSpotsInBounds = FindSpotsInRadius( originParams, boundsSpots, &insideSpotNum );

	CandidateSpots candidateSpots;
	SelectPotentialDodgeSpots( originParams, problemParams, boundsSpots, numSpotsInBounds, candidateSpots );

	ReachCheckedSpots reachCheckedSpots;
	if( problemParams.checkToAndBackReachability ) {
		CheckSpotsReachFromOriginAndBack( originParams, problemParams, candidateSpots, insideSpotNum, reachCheckedSpots );
	} else {
		CheckSpotsReachFromOrigin( originParams, problemParams, candidateSpots, insideSpotNum, reachCheckedSpots );
	}

	return CopyResults( reachCheckedSpots.begin(), reachCheckedSpots.end(), problemParams, spotOrigins, maxSpots );
}

void TacticalSpotsRegistry::SelectPotentialDodgeSpots( const OriginParams &originParams,
													   const DodgeDangerProblemParams &problemParams,
													   const uint16_t *spotNums,
													   uint16_t numSpots,
													   CandidateSpots &result ) const {
	bool mightNegateDodgeDir = false;
	Vec3 dodgeDir = MakeDodgeDangerDir( originParams, problemParams, &mightNegateDodgeDir );

	const float searchRadius = originParams.searchRadius;
	const float minHeightAdvantageOverOrigin = problemParams.minHeightAdvantageOverOrigin;
	const float heightOverOriginInfluence = problemParams.heightOverOriginInfluence;
	const float originZ = originParams.origin[2];
	// Copy to stack for faster access
	Vec3 origin( originParams.origin );

	if( mightNegateDodgeDir ) {
		for( unsigned i = 0; i < numSpots && result.size() < result.capacity(); ++i ) {
			const TacticalSpot &spot = spots[spotNums[i]];

			float heightOverOrigin = spot.absMins[2] - originZ;
			if( heightOverOrigin < minHeightAdvantageOverOrigin ) {
				continue;
			}

			Vec3 toSpotDir( spot.origin );
			toSpotDir -= origin;
			float squaredDistanceToSpot = toSpotDir.SquaredLength();
			if( squaredDistanceToSpot < 1 ) {
				continue;
			}

			toSpotDir *= Q_RSqrt( squaredDistanceToSpot );
			float dot = toSpotDir.Dot( dodgeDir );
			if( dot < 0.2f ) {
				continue;
			}

			heightOverOrigin -= minHeightAdvantageOverOrigin;
			float heightOverOriginFactor = BoundedFraction( heightOverOrigin, searchRadius - minHeightAdvantageOverOrigin );
			float score = ApplyFactor( dot, heightOverOriginFactor, heightOverOriginInfluence );

			result.push_back( SpotAndScore( spotNums[i], score ) );
		}
	} else {
		for( unsigned i = 0; i < numSpots && result.size() < result.capacity(); ++i ) {
			const TacticalSpot &spot = spots[spotNums[i]];

			float heightOverOrigin = spot.absMins[2] - originZ;
			if( heightOverOrigin < minHeightAdvantageOverOrigin ) {
				continue;
			}

			Vec3 toSpotDir( spot.origin );
			toSpotDir -= origin;
			float squaredDistanceToSpot = toSpotDir.SquaredLength();
			if( squaredDistanceToSpot < 1 ) {
				continue;
			}

			toSpotDir *= Q_RSqrt( squaredDistanceToSpot );
			float absDot = fabsf( toSpotDir.Dot( dodgeDir ) );
			if( absDot < 0.2f ) {
				continue;
			}

			heightOverOrigin -= minHeightAdvantageOverOrigin;
			float heightOverOriginFactor = BoundedFraction( heightOverOrigin, searchRadius - minHeightAdvantageOverOrigin );
			float score = ApplyFactor( absDot, heightOverOriginFactor, heightOverOriginInfluence );

			result.push_back( SpotAndScore( spotNums[i], score ) );
		}
	}

	// Sort result so best score areas are first
	std::sort( result.begin(), result.end() );
}

Vec3 TacticalSpotsRegistry::MakeDodgeDangerDir( const OriginParams &originParams,
												const DodgeDangerProblemParams &problemParams,
												bool *mightNegateDodgeDir ) const {
	*mightNegateDodgeDir = false;
	if( problemParams.avoidSplashDamage ) {
		Vec3 result( 0, 0, 0 );
		Vec3 originToHitDir = problemParams.dangerHitPoint - originParams.origin;
		float degrees = originParams.originEntity ? -originParams.originEntity->s.angles[YAW] : -90;
		RotatePointAroundVector( result.Data(), &axis_identity[AXIS_UP], originToHitDir.Data(), degrees );
		result.NormalizeFast();

		if( fabs( result.X() ) < 0.3 ) {
			result.X() = 0;
		}
		if( fabs( result.Y() ) < 0.3 ) {
			result.Y() = 0;
		}
		result.Z() = 0;
		result.X() *= -1.0f;
		result.Y() *= -1.0f;
		return result;
	}

	Vec3 selfToHitPoint = problemParams.dangerHitPoint - originParams.origin;
	selfToHitPoint.Z() = 0;
	// If bot is not hit in its center, try pick a direction that is opposite to a vector from bot center to hit point
	if( selfToHitPoint.SquaredLength() > 4 * 4 ) {
		selfToHitPoint.NormalizeFast();
		// Check whether this direction really helps to dodge the danger
		// (the less is the abs. value of the dot product, the closer is the chosen direction to a perpendicular one)
		if( fabs( selfToHitPoint.Dot( originParams.origin ) ) < 0.5f ) {
			if( fabs( selfToHitPoint.X() ) < 0.3 ) {
				selfToHitPoint.X() = 0;
			}
			if( fabs( selfToHitPoint.Y() ) < 0.3 ) {
				selfToHitPoint.Y() = 0;
			}
			return -selfToHitPoint;
		}
	}

	*mightNegateDodgeDir = true;
	// Otherwise just pick a direction that is perpendicular to the danger direction
	float maxCrossSqLen = 0.0f;
	Vec3 result( 0, 1, 0 );
	for( int i = 0; i < 3; ++i ) {
		Vec3 cross = problemParams.dangerDirection.Cross( &axis_identity[i * 3] );
		cross.Z() = 0;
		float crossSqLen = cross.SquaredLength();
		if( crossSqLen > maxCrossSqLen ) {
			maxCrossSqLen = crossSqLen;
			float invLen = Q_RSqrt( crossSqLen );
			result.X() = cross.X() * invLen;
			result.Y() = cross.Y() * invLen;
		}
	}
	return result;
}

bool TacticalSpotsRegistry::FindClosestToTargetWalkableSpot( const OriginParams &originParams,
															 int targetAreaNum,
															 vec3_t *spotOrigin ) const {
	uint16_t spotNums[MAX_SPOTS];
	uint16_t insideSpotNum;
	uint16_t numSpots = FindSpotsInRadius( originParams, spotNums, &insideSpotNum );

	const auto *routeCache = originParams.routeCache;
	const int originAreaNum = originParams.originAreaNum;
	const int allowedTravelFlags = originParams.allowedTravelFlags;

	// Assume that max allowed travel time to spot does not exceed travel time
	// required to cover the search radius walking with 250 units of speed
	const int maxTravelTimeToSpotWalking = (int)( originParams.searchRadius / 250.0f );

	const int currTravelTimeToTarget = routeCache->TravelTimeToGoalArea( originAreaNum, targetAreaNum, allowedTravelFlags );
	if( !currTravelTimeToTarget ) {
		return false;
	}

	trace_t trace;

	const TacticalSpot *bestSpot = nullptr;
	int bestCombinedTravelTime = -1;
	int travelTimeToSpotWalking, travelTimeFromSpotToTarget;

	for( uint16_t i = 0; i < numSpots; ++i ) {
		const uint16_t spotNum = spotNums[i];
		const auto &spot = spots[spotNum];

		travelTimeToSpotWalking = routeCache->TravelTimeToGoalArea( originAreaNum, spot.aasAreaNum, TFL_WALK );
		if( !travelTimeToSpotWalking ) {
			continue;
		}

		// Due to triangle inequality travelTimeToSpotWalking + travelTimeFromSpotToTarget >= currTravelTimeToTarget.
		// We can reject spots that does not look like short-range reachable by walking though
		if( travelTimeToSpotWalking > maxTravelTimeToSpotWalking ) {
			continue;
		}

		travelTimeFromSpotToTarget = routeCache->TravelTimeToGoalArea( originAreaNum, spot.aasAreaNum, allowedTravelFlags );
		if( !travelTimeFromSpotToTarget ) {
			continue;
		}

		if( travelTimeFromSpotToTarget > currTravelTimeToTarget ) {
			continue;
		}

		int combinedTravelTime = travelTimeToSpotWalking + travelTimeFromSpotToTarget;
		if( combinedTravelTime <= bestCombinedTravelTime ) {
			continue;
		}

		SolidWorldTrace( &trace, originParams.origin, spot.origin );
		if( trace.fraction != 1.0f ) {
			continue;
		}

		bestCombinedTravelTime = combinedTravelTime;
		bestSpot = &spot;
	}

	if( bestSpot ) {
		VectorCopy( bestSpot->origin, *spotOrigin );
		return true;
	}

	return false;
}