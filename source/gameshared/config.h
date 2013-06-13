/************************************************************************/
/* WARNING                                                              */
/* define this when we compile for a public release                     */
/* this will protect dangerous and untested pieces of code              */
/************************************************************************/
//#define PUBLIC_BUILD

//==============================================
// wsw : jal :	these defines affect every project file. They are
//				work-in-progress stuff which is, sooner or later,
//				going to be removed by keeping or discarding it.
//==============================================

// pretty solid
#define MOREGRAVITY
#define ALT_ZLIB_COMPRESSION

// renderer config
#define CELLSHADEDMATERIAL
#define HALFLAMBERTLIGHTING
#define AREAPORTALS_MATRIX
#define PUTCPU2SLEEP

// collision config
#define TRACE_NOAXIAL // a hack to avoid issues with the return of traces against non axial planes

//==============================================
// undecided status

//#define PURE_CHEAT

//#define ANTICHEAT_MODULE
//#define ALLOWBYNNY_VOTE

#define MATCHMAKER_SUPPORT

//#define UCMDTIMENUDGE
#ifdef MATCHMAKER_SUPPORT
// # define TCP_SUPPORT
#endif
//#define TCP_ALLOW_CONNECT

#ifndef PUBLIC_BUILD
//#define WEAPONDEFS_FROM_DISK
#endif

#define DOWNSCALE_ITEMS // Ugly hack for the release. Item models are way too big
#define ELECTROBOLT_TEST

#define MUMBLE_SUPPORT

// collaborations
//==============================================

// symbol address retrieval
//==============================================
// #define SYS_SYMBOL		// adds "sys_symbol" command and symbol exports to binary release
