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

// pretty solid
#define MOREGRAVITY

//==============================================
// undecided status

//#define PURE_CHEAT

#define TCP_SUPPORT

//#define AUTHED_SAY

#define HTTP_SUPPORT

#if defined( HTTP_SUPPORT ) && !defined( TCP_SUPPORT )
#undef HTTP_SUPPORT
#endif

#define DOWNSCALE_ITEMS // Ugly hack for the release. Item models are way too big

// collaborations
//==============================================
