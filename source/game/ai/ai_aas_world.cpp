#include "ai_aas_world.h"
#include "static_vector.h"
#include "ai_local.h"
#include "../../qalgo/md5.h"
#include "../../qalgo/base64.h"

#undef min
#undef max
#include <memory>
#include <tuple>

// Static member definition
AiAasWorld *AiAasWorld::instance = nullptr;

AiAasWorld::AiAasWorld( AiAasWorld &&that ) {
	memcpy( this, &that, sizeof( AiAasWorld ) );
	that.loaded = false;
}

bool AiAasWorld::Init( const char *mapname ) {
	if( instance ) {
		AI_FailWith( "AiAasWorld::Init()", "An instance is already present\n" );
	}
	instance = (AiAasWorld *)G_Malloc( sizeof( AiAasWorld ) );
	new(instance) AiAasWorld;
	// Try to initialize the instance
	if( !instance->Load( mapname ) ) {
		return false;
	}
	instance->PostLoad();
	return true;
}

void AiAasWorld::Shutdown() {
	// This may be called on first map load when an instance has never been instantiated
	if( instance ) {
		instance->~AiAasWorld();
		G_Free( instance );
		// Allow the pointer to be reused, otherwise an assertion will fail on a next Init() call
		instance = nullptr;
	}
}

void AiAasWorld::Frame() {
}

int AiAasWorld::PointAreaNum( const vec3_t point ) const {
	if( !loaded ) {
		return 0;
	}

	//start with node 1 because node zero is a dummy used for solid leafs
	int nodenum = 1;

	while( nodenum > 0 ) {
		aas_node_t *node = &nodes[nodenum];
		aas_plane_t *plane = &planes[node->planenum];
		vec_t dist = DotProduct( point, plane->normal ) - plane->dist;
		if( dist > 0 ) {
			nodenum = node->children[0];
		} else {
			nodenum = node->children[1];
		}
	}
	return -nodenum;
}

int AiAasWorld::FindAreaNum( const vec3_t mins, const vec3_t maxs ) const {
	const vec_t *bounds[2] = { maxs, mins };
	// Test all AABB vertices
	vec3_t origin = { 0, 0, 0 };

	for( int i = 0; i < 8; ++i ) {
		origin[0] = bounds[( i >> 0 ) & 1][0];
		origin[1] = bounds[( i >> 1 ) & 1][1];
		origin[2] = bounds[( i >> 2 ) & 1][2];
		int areaNum = PointAreaNum( origin );
		if( areaNum ) {
			return areaNum;
		}
	}
	return 0;
}

int AiAasWorld::FindAreaNum( const vec3_t origin ) const {
	int areaNum = PointAreaNum( const_cast<float*>( origin ) );

	if( areaNum ) {
		return areaNum;
	}

	vec3_t mins = { -8, -8, 0 };
	VectorAdd( mins, origin, mins );
	vec3_t maxs = { +8, +8, 16 };
	VectorAdd( maxs, origin, maxs );
	return FindAreaNum( mins, maxs );
}

int AiAasWorld::FindAreaNum( const edict_t *ent ) const {
	// Reject degenerate case
	if( ent->r.absmin[0] == ent->r.absmax[0] &&
		ent->r.absmin[1] == ent->r.absmax[1] &&
		ent->r.absmin[2] == ent->r.absmax[2] ) {
		return FindAreaNum( ent->s.origin );
	}

	Vec3 testedOrigin( ent->s.origin );
	int areaNum = PointAreaNum( testedOrigin.Data() );
	if( areaNum ) {
		return areaNum;
	}

	return FindAreaNum( ent->r.absmin, ent->r.absmax );
}

typedef struct aas_tracestack_s {
	vec3_t start;       //start point of the piece of line to trace
	vec3_t end;         //end point of the piece of line to trace
	int planenum;       //last plane used as splitter
	int nodenum;        //node found after splitting with planenum
} aas_tracestack_t;

int AiAasWorld::TraceAreas( const vec3_t start, const vec3_t end, int *areas_, vec3_t *points, int maxareas ) const {
	if( !loaded ) {
		return 0;
	}

	vec3_t cur_start, cur_end, cur_mid;
	aas_tracestack_t tracestack[127];
	aas_tracestack_t *tstack_p;

	int numAreas = 0;
	areas_[0] = 0;

	tstack_p = tracestack;
	//we start with the whole line on the stack
	VectorCopy( start, tstack_p->start );
	VectorCopy( end, tstack_p->end );
	tstack_p->planenum = 0;
	//start with node 1 because node zero is a dummy for a solid leaf
	tstack_p->nodenum = 1;      //starting at the root of the tree
	tstack_p++;

	while( 1 ) {
		//pop up the stack
		tstack_p--;
		//if the trace stack is empty (ended up with a piece of the
		//line to be traced in an area)
		if( tstack_p < tracestack ) {
			return numAreas;
		}

		//number of the current node to test the line against
		int nodenum = tstack_p->nodenum;
		//if it is an area
		if( nodenum < 0 ) {
			areas_[numAreas] = -nodenum;
			if( points ) {
				VectorCopy( tstack_p->start, points[numAreas] );
			}
			numAreas++;
			if( numAreas >= maxareas ) {
				return numAreas;
			}
			continue;
		}
		//if it is a solid leaf
		if( !nodenum ) {
			continue;
		}

		//the node to test against
		aas_node_t *aasnode = &nodes[nodenum];
		//start point of current line to test against node
		VectorCopy( tstack_p->start, cur_start );
		//end point of the current line to test against node
		VectorCopy( tstack_p->end, cur_end );
		//the current node plane
		aas_plane_t *plane = &planes[aasnode->planenum];

		float front = DotProduct( cur_start, plane->normal ) - plane->dist;
		float back = DotProduct( cur_end, plane->normal ) - plane->dist;

		//if the whole to be traced line is totally at the front of this node
		//only go down the tree with the front child
		if( front > 0 && back > 0 ) {
			//keep the current start and end point on the stack
			//and go down the tree with the front child
			tstack_p->nodenum = aasnode->children[0];
			tstack_p++;
			if( tstack_p >= &tracestack[127] ) {
				G_Printf( S_COLOR_RED "AiAasWorld::TraceAreas(): stack overflow\n" );
				return numAreas;
			}
		}
		//if the whole to be traced line is totally at the back of this node
		//only go down the tree with the back child
		else if( front <= 0 && back <= 0 ) {
			//keep the current start and end point on the stack
			//and go down the tree with the back child
			tstack_p->nodenum = aasnode->children[1];
			tstack_p++;
			if( tstack_p >= &tracestack[127] ) {
				G_Printf( S_COLOR_RED "AiAasWorld::TraceAreas(): stack overflow\n" );
				return numAreas;
			}
		}
		//go down the tree both at the front and back of the node
		else {
			int tmpplanenum = tstack_p->planenum;
			//calculate the hitpoint with the node (split point of the line)
			//put the crosspoint TRACEPLANE_EPSILON pixels on the near side
			float frac = front / ( front - back );
			clamp( frac, 0.0f, 1.0f );
			//frac = front / (front-back);
			//
			cur_mid[0] = cur_start[0] + ( cur_end[0] - cur_start[0] ) * frac;
			cur_mid[1] = cur_start[1] + ( cur_end[1] - cur_start[1] ) * frac;
			cur_mid[2] = cur_start[2] + ( cur_end[2] - cur_start[2] ) * frac;

//			AAS_DrawPlaneCross(cur_mid, plane->normal, plane->dist, plane->type, LINECOLOR_RED);
			//side the front part of the line is on
			int side = front < 0;
			//first put the end part of the line on the stack (back side)
			VectorCopy( cur_mid, tstack_p->start );
			//not necesary to store because still on stack
			//VectorCopy(cur_end, tstack_p->end);
			tstack_p->planenum = aasnode->planenum;
			tstack_p->nodenum = aasnode->children[!side];
			tstack_p++;
			if( tstack_p >= &tracestack[127] ) {
				G_Printf( S_COLOR_RED "AiAasWorld::TraceAreas(): stack overflow\n" );
				return numAreas;
			}
			//now put the part near the start of the line on the stack so we will
			//continue with thats part first. This way we'll find the first
			//hit of the bbox
			VectorCopy( cur_start, tstack_p->start );
			VectorCopy( cur_mid, tstack_p->end );
			tstack_p->planenum = tmpplanenum;
			tstack_p->nodenum = aasnode->children[side];
			tstack_p++;
			if( tstack_p >= &tracestack[127] ) {
				G_Printf( S_COLOR_RED "AiAasWorld::TraceAreas(): stack overflow\n" );
				return numAreas;
			}
		}
	}
}

int AiAasWorld::BBoxAreasNonConst( const vec3_t absmins, const vec3_t absmaxs, int *areas_, int maxareas ) {
	if( !loaded ) {
		return 0;
	}

	aas_link_t *linkedareas = LinkEntity( absmins, absmaxs, -1 );
	int num = 0;

	for( aas_link_t *link = linkedareas; link; link = link->next_area ) {
		areas_[num] = link->areanum;
		num++;
		if( num >= maxareas ) {
			break;
		}
	}
	UnlinkFromAreas( linkedareas );
	return num;
}

typedef struct {
	int nodenum;        //node found after splitting
} aas_linkstack_t;

AiAasWorld::aas_link_t *AiAasWorld::LinkEntity( const vec3_t absmins, const vec3_t absmaxs, int entnum ) {
	aas_linkstack_t linkstack[256];
	aas_linkstack_t *lstack_p;

	aas_link_t *linkedAreas = nullptr;

	//
	lstack_p = linkstack;
	//we start with the whole line on the stack
	//start with node 1 because node zero is a dummy used for solid leafs
	lstack_p->nodenum = 1;      //starting at the root of the tree
	lstack_p++;

	while( 1 ) {
		//pop up the stack
		lstack_p--;
		//if the trace stack is empty (ended up with a piece of the
		//line to be traced in an area)
		if( lstack_p < linkstack ) {
			break;
		}

		//number of the current node to test the line against
		int nodenum = lstack_p->nodenum;
		//if it is an area
		if( nodenum < 0 ) {
			//NOTE: the entity might have already been linked into this area
			// because several node children can point to the same area
			aas_link_t *link = arealinkedentities[-nodenum];

			for(; link; link = link->next_ent ) {
				if( link->entnum == entnum ) {
					break;
				}
			}
			if( link ) {
				continue;
			}

			link = AllocLink();
			if( !link ) {
				return linkedAreas;
			}

			link->entnum = entnum;
			link->areanum = -nodenum;
			//put the link into the double linked area list of the entity
			link->prev_area = nullptr;
			link->next_area = linkedAreas;
			if( linkedAreas ) {
				linkedAreas->prev_area = link;
			}
			linkedAreas = link;
			//put the link into the double linked entity list of the area
			link->prev_ent = nullptr;
			link->next_ent = arealinkedentities[-nodenum];
			if( arealinkedentities[-nodenum] ) {
				arealinkedentities[-nodenum]->prev_ent = link;
			}
			arealinkedentities[-nodenum] = link;
			continue;
		}

		//if solid leaf
		if( !nodenum ) {
			continue;
		}

		//the node to test against
		aas_node_t *aasnode = &nodes[nodenum];
		//the current node plane
		aas_plane_t *plane = &planes[aasnode->planenum];
		//get the side(s) the box is situated relative to the plane
		int side = BoxOnPlaneSide2( absmins, absmaxs, plane );
		//if on the front side of the node
		if( side & 1 ) {
			lstack_p->nodenum = aasnode->children[0];
			lstack_p++;
		}
		if( lstack_p >= &linkstack[255] ) {
			G_Printf( S_COLOR_RED "AiAasWorld::LinkEntity(): stack overflow\n" );
			break;
		}
		//if on the back side of the node
		if( side & 2 ) {
			lstack_p->nodenum = aasnode->children[1];
			lstack_p++;
		}
		if( lstack_p >= &linkstack[255] ) {
			G_Printf( S_COLOR_RED "AiAasWorld::LinkEntity(): stack overflow\n" );
			break;
		}
	}
	return linkedAreas;
}

void AiAasWorld::UnlinkFromAreas( aas_link_t *linkedAreas ) {
	aas_link_t *link, *nextlink;

	for( link = linkedAreas; link; link = nextlink ) {
		//next area the entity is linked in
		nextlink = link->next_area;
		//remove the entity from the linked list of this area
		if( link->prev_ent ) {
			link->prev_ent->next_ent = link->next_ent;
		} else {
			arealinkedentities[link->areanum] = link->next_ent;
		}
		if( link->next_ent ) {
			link->next_ent->prev_ent = link->prev_ent;
		}
		//deallocate the link structure
		DeAllocLink( link );
	}
}

AiAasWorld::aas_link_t *AiAasWorld::AllocLink() {
	aas_link_t *link = freelinks;

	if( !link ) {
		G_Printf( S_COLOR_RED "empty aas link heap\n" );
		return nullptr;
	}
	if( freelinks ) {
		freelinks = freelinks->next_ent;
	}
	if( freelinks ) {
		freelinks->prev_ent = nullptr;
	}
	numaaslinks--;
	return link;
}

void AiAasWorld::DeAllocLink( aas_link_t *link ) {
	if( freelinks ) {
		freelinks->prev_ent = link;
	}
	link->prev_ent = nullptr;
	link->next_ent = freelinks;
	link->prev_area = nullptr;
	link->next_area = nullptr;
	freelinks = link;
	numaaslinks++;
}

void AiAasWorld::InitLinkHeap() {
	int max_aaslinks = linkheapsize;

	//if there's no link heap present
	if( !linkheap ) {
		max_aaslinks = 6144;
		linkheapsize = max_aaslinks;
		linkheap = (aas_link_t *) G_LevelMalloc( max_aaslinks * sizeof( aas_link_t ) );
	}
	//link the links on the heap
	linkheap[0].prev_ent = nullptr;
	linkheap[0].next_ent = &linkheap[1];

	for( int i = 1; i < max_aaslinks - 1; i++ ) {
		linkheap[i].prev_ent = &linkheap[i - 1];
		linkheap[i].next_ent = &linkheap[i + 1];
	}
	linkheap[max_aaslinks - 1].prev_ent = &linkheap[max_aaslinks - 2];
	linkheap[max_aaslinks - 1].next_ent = nullptr;
	//pointer to the first free link
	freelinks = &linkheap[0];
	//
	numaaslinks = max_aaslinks;
}

void AiAasWorld::InitLinkedEntities() {
	arealinkedentities = (aas_link_t **) G_LevelMalloc( numareas * sizeof( aas_link_t * ) );
}

void AiAasWorld::FreeLinkHeap() {
	if( linkheap ) {
		G_LevelFree( linkheap );
	}
	linkheap = nullptr;
	linkheapsize = 0;
}

void AiAasWorld::FreeLinkedEntities() {
	if( arealinkedentities ) {
		G_LevelFree( arealinkedentities );
	}
	arealinkedentities = nullptr;
}

void AiAasWorld::ComputeExtraAreaFlags() {
	for( int areaNum = 1; areaNum < numareas; ++areaNum ) {
		TrySetAreaLedgeFlags( areaNum );
		TrySetAreaWallFlags( areaNum );
		TrySetAreaJunkFlags( areaNum );
	}
}

void AiAasWorld::TrySetAreaLedgeFlags( int areaNum ) {
	int reachNum = areasettings[areaNum].firstreachablearea;
	int endReachNum = areasettings[areaNum].firstreachablearea + areasettings[areaNum].numreachableareas;

	for(; reachNum != endReachNum; ++reachNum ) {
		if( reachability[reachNum].traveltype == TRAVEL_WALKOFFLEDGE ) {
			areasettings[areaNum].areaflags |= AREA_LEDGE;
			break;
		}
	}
}

void AiAasWorld::TrySetAreaWallFlags( int areaNum ) {
	int faceIndexNum = areas[areaNum].firstface;
	int endFaceIndexNum = areas[areaNum].firstface + areas[areaNum].numfaces;
	const float *zAxis = &axis_identity[AXIS_UP];

	for(; faceIndexNum != endFaceIndexNum; ++faceIndexNum ) {
		int faceIndex = faceindex[faceIndexNum];
		int areaBehindFace;
		const aas_face_t *face;
		if( faceIndex >= 0 ) {
			face = &faces[faceIndex];
			areaBehindFace = face->backarea;
		} else   {
			face = &faces[-faceIndex];
			areaBehindFace = face->frontarea;
		}

		// There is no solid but some other area behind the face
		if( areaBehindFace ) {
			continue;
		}

		const aas_plane_t *facePlane = &planes[face->planenum];
		// Do not treat bounding ceilings and ground as a wall
		if( fabsf( DotProduct( zAxis, facePlane->normal ) ) < 0.3f ) {
			areasettings[areaNum].areaflags |= AREA_WALL;
			break;
		}
	}
}

void AiAasWorld::TrySetAreaJunkFlags( int areaNum ) {
	const aas_area_t &area = areas[areaNum];
	int junkFactor = 0;

	for( int i = 0; i < 3; ++i ) {
		if( area.maxs[i] - area.mins[i] < 24.0f ) {
			++junkFactor;
		}
	}
	if( junkFactor > 1 ) {
		areasettings[areaNum].areaflags |= AREA_JUNK;
	}
}

int AiAasWorld::BoxOnPlaneSide2( const vec3_t absmins, const vec3_t absmaxs, const aas_plane_t *p ) {
	vec3_t corners[2];

	for( int i = 0; i < 3; i++ ) {
		if( p->normal[i] < 0 ) {
			corners[0][i] = absmins[i];
			corners[1][i] = absmaxs[i];
		} else   {
			corners[1][i] = absmins[i];
			corners[0][i] = absmaxs[i];
		} //end else
	}
	float dist1 = DotProduct( p->normal, corners[0] ) - p->dist;
	float dist2 = DotProduct( p->normal, corners[1] ) - p->dist;
	int sides = 0;
	if( dist1 >= 0 ) {
		sides = 1;
	}
	if( dist2 < 0 ) {
		sides |= 2;
	}

	return sides;
}

static void AAS_DData( unsigned char *data, int size ) {
	for( int i = 0; i < size; i++ ) {
		data[i] ^= (unsigned char) i * 119;
	}
}

#define AAS_LUMPS                   14
#define AASLUMP_BBOXES              0
#define AASLUMP_VERTEXES            1
#define AASLUMP_PLANES              2
#define AASLUMP_EDGES               3
#define AASLUMP_EDGEINDEX           4
#define AASLUMP_FACES               5
#define AASLUMP_FACEINDEX           6
#define AASLUMP_AREAS               7
#define AASLUMP_AREASETTINGS        8
#define AASLUMP_REACHABILITY        9
#define AASLUMP_NODES               10
#define AASLUMP_PORTALS             11
#define AASLUMP_PORTALINDEX         12
#define AASLUMP_CLUSTERS            13

class AasFileReader
{
	int fp;
	int lastoffset;

	//header lump
	typedef struct {
		int fileofs;
		int filelen;
	} aas_lump_t;

	//aas file header
	typedef struct aas_header_s {
		int ident;
		int version;
		int bspchecksum;
		//data entries
		aas_lump_t lumps[AAS_LUMPS];
	} aas_header_t;

	aas_header_t header;
	int fileSize;

	char *LoadLump( int lumpNum, int size );

public:
	AasFileReader( const char *mapname );

	~AasFileReader() {
		if( fp ) {
			trap_FS_FCloseFile( fp );
		}
	}

	inline bool IsValid() { return fp != 0; }

	template<typename T>
	inline std::tuple<T*, int> LoadLump( int lumpNum ) {
		int oldOffset = lastoffset;
		char *rawData = LoadLump( lumpNum, sizeof( T ) );
		int length = lastoffset - oldOffset;

		return std::make_tuple( (T*)rawData, length / sizeof( T ) );
	};

	bool ComputeChecksum( char **base64Digest );
};

#define AASID                       ( ( 'S' << 24 ) + ( 'A' << 16 ) + ( 'A' << 8 ) + 'E' )
#define AASVERSION_OLD              4
#define AASVERSION                  5

AasFileReader::AasFileReader( const char *mapname )
	: lastoffset( 0 ) {
	// Shut up an analyzer
	memset( &header, 0, sizeof( header ) );

	char filename[MAX_QPATH];
	Q_snprintfz( filename, MAX_QPATH, "maps/%s.aas", mapname );

	fileSize = trap_FS_FOpenFile( filename, &fp, FS_READ );
	if( !fp || fileSize <= 0 ) {
		G_Printf( S_COLOR_RED "can't open %s\n", filename );
		return;
	}

	//read the header
	trap_FS_Read( &header, sizeof( aas_header_t ), fp );
	lastoffset = sizeof( aas_header_t );
	//check header identification
	header.ident = LittleLong( header.ident );
	if( header.ident != AASID ) {
		G_Printf( S_COLOR_RED "%s is not an AAS file\n", filename );
		return;
	}

	//check the version
	header.version = LittleLong( header.version );
	if( header.version != AASVERSION_OLD && header.version != AASVERSION ) {
		G_Printf( S_COLOR_RED "aas file %s is version %i, not %i\n", filename, header.version, AASVERSION );
		return;
	}
	if( header.version == AASVERSION ) {
		AAS_DData( (unsigned char *) &header + 8, sizeof( aas_header_t ) - 8 );
	}
}

char *AasFileReader::LoadLump( int lumpNum, int size ) {
	int offset = LittleLong( header.lumps[lumpNum].fileofs );
	int length = LittleLong( header.lumps[lumpNum].filelen );

	if( !length ) {
		//just alloc a dummy
		return (char *) G_LevelMalloc( size + 1 );
	}
	//seek to the data
	if( offset != lastoffset ) {
		G_Printf( S_COLOR_YELLOW "AAS file not sequentially read\n" );
		if( trap_FS_Seek( fp, offset, FS_SEEK_SET ) ) {
			G_Printf( S_COLOR_RED "can't seek to aas lump\n" );
			return nullptr;
		}
	}
	//allocate memory
	char *buf = (char *) G_LevelMalloc( length + 1 );
	//read the data
	if( length ) {
		trap_FS_Read( buf, length, fp );
		lastoffset += length;
	}
	return buf;
}

bool AasFileReader::ComputeChecksum( char **base64Digest ) {
	if( trap_FS_Seek( fp, 0, FS_SEEK_SET ) < 0 ) {
		return false;
	}

	// TODO: Read the entire AAS data at start and then use the read chunk for loading of AAS lumps
	char *mem = (char *)G_LevelMalloc( (unsigned)fileSize );
	if( trap_FS_Read( mem, (unsigned)fileSize, fp ) <= 0 ) {
		G_LevelFree( mem );
		return false;
	}

	// Compute a binary MD5 digest of the file data first
	md5_byte_t binaryDigest[16];
	md5_digest( mem, fileSize, binaryDigest );

	// Get a base64-encoded digest in a temporary buffer allocated via malloc()
	size_t base64Length;
	char *tmpBase64Chars = ( char * )base64_encode( binaryDigest, 16, &base64Length );

	// Free the level data
	G_LevelFree( mem );

	// Copy the base64-encoded digest to the game memory storage to avoid further confusion
	*base64Digest = ( char * )G_LevelMalloc( base64Length + 1 );
	// Include the last zero byte in copied chars
	memcpy( *base64Digest, tmpBase64Chars, base64Length + 1 );

	free( tmpBase64Chars );

	return true;
}

bool AiAasWorld::Load( const char *mapname ) {
	AasFileReader reader( mapname );

	if( !reader.IsValid() ) {
		return false;
	}

	std::tie( bboxes, numbboxes ) = reader.LoadLump<aas_bbox_t>( AASLUMP_BBOXES );
	if( numbboxes && !bboxes ) {
		return false;
	}

	std::tie( vertexes, numvertexes ) = reader.LoadLump<aas_vertex_t>( AASLUMP_VERTEXES );
	if( numvertexes && !vertexes ) {
		return false;
	}

	std::tie( planes, numplanes ) = reader.LoadLump<aas_plane_t>( AASLUMP_PLANES );
	if( numplanes && !planes ) {
		return false;
	}

	std::tie( edges, numedges ) = reader.LoadLump<aas_edge_t>( AASLUMP_EDGES );
	if( numedges && !edges ) {
		return false;
	}

	std::tie( edgeindex, edgeindexsize ) = reader.LoadLump<int>( AASLUMP_EDGEINDEX );
	if( edgeindexsize && !edgeindex ) {
		return false;
	}

	std::tie( faces, numfaces ) = reader.LoadLump<aas_face_t>( AASLUMP_FACES );
	if( numfaces && !faces ) {
		return false;
	}

	std::tie( faceindex, faceindexsize ) = reader.LoadLump<int>( AASLUMP_FACEINDEX );
	if( faceindexsize && !faceindex ) {
		return false;
	}

	std::tie( areas, numareas ) = reader.LoadLump<aas_area_t>( AASLUMP_AREAS );
	if( numareas && !areas ) {
		return false;
	}

	std::tie( areasettings, numareasettings ) = reader.LoadLump<aas_areasettings_t>( AASLUMP_AREASETTINGS );
	if( numareasettings && !areasettings ) {
		return false;
	}

	std::tie( reachability, reachabilitysize ) = reader.LoadLump<aas_reachability_t>( AASLUMP_REACHABILITY );
	if( reachabilitysize && !reachability ) {
		return false;
	}

	std::tie( nodes, numnodes ) = reader.LoadLump<aas_node_t>( AASLUMP_NODES );
	if( numnodes && !nodes ) {
		return false;
	}

	std::tie( portals, numportals ) = reader.LoadLump<aas_portal_t>( AASLUMP_PORTALS );
	if( numportals && !portals ) {
		return false;
	}

	std::tie( portalindex, portalindexsize ) = reader.LoadLump<int>( AASLUMP_PORTALINDEX );
	if( portalindexsize && !portalindex ) {
		return false;
	}

	std::tie( clusters, numclusters ) = reader.LoadLump<aas_cluster_t>( AASLUMP_CLUSTERS );
	if( numclusters && !clusters ) {
		return false;
	}

	checksum = nullptr;
	if( !reader.ComputeChecksum( &checksum ) ) {
		return false;
	}

	SwapData();

	loaded = true;
	return true;
}

AiAasWorld::~AiAasWorld() {
	if( !loaded ) {
		return;
	}

	if( checksum ) {
		G_LevelFree( checksum );
	}

	FreeLinkedEntities();
	FreeLinkHeap();

	// These items may be absent for some stripped AAS files, so check each one.
	if( bboxes ) {
		G_LevelFree( bboxes );
	}
	if( vertexes ) {
		G_LevelFree( vertexes );
	}
	if( planes ) {
		G_LevelFree( planes );
	}
	if( edges ) {
		G_LevelFree( edges );
	}
	if( edgeindex ) {
		G_LevelFree( edgeindex );
	}
	if( faces ) {
		G_LevelFree( faces );
	}
	if( faceindex ) {
		G_LevelFree( faceindex );
	}
	if( areas ) {
		G_LevelFree( areas );
	}
	if( areasettings ) {
		G_LevelFree( areasettings );
	}
	if( reachability ) {
		G_LevelFree( reachability );
	}
	if( nodes ) {
		G_LevelFree( nodes );
	}
	if( portals ) {
		G_LevelFree( portals );
	}
	if( portalindex ) {
		G_LevelFree( portalindex );
	}
	if( clusters ) {
		G_LevelFree( clusters );
	}
}

void AiAasWorld::SwapData() {
	//bounding boxes
	for( int i = 0; i < numbboxes; i++ ) {
		bboxes[i].presencetype = LittleLong( bboxes[i].presencetype );
		bboxes[i].flags = LittleLong( bboxes[i].flags );

		for( int j = 0; j < 3; j++ ) {
			bboxes[i].mins[j] = LittleLong( bboxes[i].mins[j] );
			bboxes[i].maxs[j] = LittleLong( bboxes[i].maxs[j] );
		}
	}

	//vertexes
	for( int i = 0; i < numvertexes; i++ ) {
		for( int j = 0; j < 3; j++ )
			vertexes[i][j] = LittleFloat( vertexes[i][j] );
	}

	//planes
	for( int i = 0; i < numplanes; i++ ) {
		for( int j = 0; j < 3; j++ )
			planes[i].normal[j] = LittleFloat( planes[i].normal[j] );
		planes[i].dist = LittleFloat( planes[i].dist );
		planes[i].type = LittleLong( planes[i].type );
	}

	//edges
	for( int i = 0; i < numedges; i++ ) {
		edges[i].v[0] = LittleLong( edges[i].v[0] );
		edges[i].v[1] = LittleLong( edges[i].v[1] );
	}

	//edgeindex
	for( int i = 0; i < edgeindexsize; i++ ) {
		edgeindex[i] = LittleLong( edgeindex[i] );
	}

	//faces
	for( int i = 0; i < numfaces; i++ ) {
		faces[i].planenum = LittleLong( faces[i].planenum );
		faces[i].faceflags = LittleLong( faces[i].faceflags );
		faces[i].numedges = LittleLong( faces[i].numedges );
		faces[i].firstedge = LittleLong( faces[i].firstedge );
		faces[i].frontarea = LittleLong( faces[i].frontarea );
		faces[i].backarea = LittleLong( faces[i].backarea );
	}

	//face index
	for( int i = 0; i < faceindexsize; i++ ) {
		faceindex[i] = LittleLong( faceindex[i] );
	}

	//convex areas
	for( int i = 0; i < numareas; i++ ) {
		areas[i].areanum = LittleLong( areas[i].areanum );
		areas[i].numfaces = LittleLong( areas[i].numfaces );
		areas[i].firstface = LittleLong( areas[i].firstface );

		for( int j = 0; j < 3; j++ ) {
			areas[i].mins[j] = LittleFloat( areas[i].mins[j] );
			areas[i].maxs[j] = LittleFloat( areas[i].maxs[j] );
			areas[i].center[j] = LittleFloat( areas[i].center[j] );
		}
	}

	//area settings
	for( int i = 0; i < numareasettings; i++ ) {
		areasettings[i].contents = LittleLong( areasettings[i].contents );
		areasettings[i].areaflags = LittleLong( areasettings[i].areaflags );
		areasettings[i].presencetype = LittleLong( areasettings[i].presencetype );
		areasettings[i].cluster = LittleLong( areasettings[i].cluster );
		areasettings[i].clusterareanum = LittleLong( areasettings[i].clusterareanum );
		areasettings[i].numreachableareas = LittleLong( areasettings[i].numreachableareas );
		areasettings[i].firstreachablearea = LittleLong( areasettings[i].firstreachablearea );
	}

	//area reachability
	for( int i = 0; i < reachabilitysize; i++ ) {
		reachability[i].areanum = LittleLong( reachability[i].areanum );
		reachability[i].facenum = LittleLong( reachability[i].facenum );
		reachability[i].edgenum = LittleLong( reachability[i].edgenum );

		for( int j = 0; j < 3; j++ ) {
			reachability[i].start[j] = LittleFloat( reachability[i].start[j] );
			reachability[i].end[j] = LittleFloat( reachability[i].end[j] );
		}
		reachability[i].traveltype = LittleLong( reachability[i].traveltype );
		reachability[i].traveltime = LittleShort( reachability[i].traveltime );
	}

	//nodes
	for( int i = 0; i < numnodes; i++ ) {
		nodes[i].planenum = LittleLong( nodes[i].planenum );
		nodes[i].children[0] = LittleLong( nodes[i].children[0] );
		nodes[i].children[1] = LittleLong( nodes[i].children[1] );
	}

	//cluster portals
	for( int i = 0; i < numportals; i++ ) {
		portals[i].areanum = LittleLong( portals[i].areanum );
		portals[i].frontcluster = LittleLong( portals[i].frontcluster );
		portals[i].backcluster = LittleLong( portals[i].backcluster );
		portals[i].clusterareanum[0] = LittleLong( portals[i].clusterareanum[0] );
		portals[i].clusterareanum[1] = LittleLong( portals[i].clusterareanum[1] );
	}

	//cluster portal index
	for( int i = 0; i < portalindexsize; i++ ) {
		portalindex[i] = LittleLong( portalindex[i] );
	}

	//cluster
	for( int i = 0; i < numclusters; i++ ) {
		clusters[i].numareas = LittleLong( clusters[i].numareas );
		clusters[i].numreachabilityareas = LittleLong( clusters[i].numreachabilityareas );
		clusters[i].numportals = LittleLong( clusters[i].numportals );
		clusters[i].firstportal = LittleLong( clusters[i].firstportal );
	}
}