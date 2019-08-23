/************************************************************************/
/* WARNING                                                              */
/* define this when we compile for a public release                     */
/* this will protect dangerous and untested pieces of code              */
/************************************************************************/
//#define PUBLIC_BUILD
//#define BROKEN_BUILD

//==============================================
// wsw : jal :	these defines affect every project file. They are
//				work-in-progress stuff which is, sooner or later,
//				going to be removed by keeping or discarding it.
//==============================================

#ifdef BROKEN_BUILD
break
#endif

// pretty solid
#define MOREGRAVITY

// renderer config
//#define CELSHADEDMATERIAL
#define HALFLAMBERTLIGHTING
#define AREAPORTALS_MATRIX

// collision config
#define TRACE_NOAXIAL // a hack to avoid issues with the return of traces against non axial planes

//==============================================
// undecided status

//#define PURE_CHEAT

//#define UCMDTIMENUDGE

#define TCP_SUPPORT

//#define AUTHED_SAY

//#define TCP_ALLOW_CONNECT

#if defined( TCP_ALLOW_CONNECT ) && !defined( TCP_SUPPORT )
#undef TCP_ALLOW_CONNECT
#endif

#define HTTP_SUPPORT

#if defined( HTTP_SUPPORT ) && !defined( TCP_SUPPORT )
#undef HTTP_SUPPORT
#endif

#ifndef PUBLIC_BUILD
#define TCP_ALLOW_CONNECT_CLIENT
//#define WEAPONDEFS_FROM_DISK
#endif

#define DOWNSCALE_ITEMS // Ugly hack for the release. Item models are way too big
#define ELECTROBOLT_TEST

// collaborations
//==============================================
