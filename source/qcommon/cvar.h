#pragma once

/*

   cvar_t variables are used to hold scalar or string variables that can be changed or displayed at the console or prog code as well as accessed directly
   in C code.

   The user can access cvars from the console in three ways:
   r_draworder			prints the current value
   r_draworder 0		sets the current value to 0
   set r_draworder 0	as above, but creates the cvar if not present
   Cvars are restricted from having the same names as commands to keep this
   interface from being ambiguous.
 */

// checks if the cvar system can still be used
bool Cvar_Initialized( void );

// flag manipulation routines
static inline cvar_flag_t Cvar_FlagSet( cvar_flag_t *flags, cvar_flag_t flag );
static inline cvar_flag_t Cvar_FlagUnset( cvar_flag_t *flags, cvar_flag_t flag );
static inline cvar_flag_t Cvar_FlagsClear( cvar_flag_t *flags );
static inline bool Cvar_FlagIsSet( cvar_flag_t flags, cvar_flag_t flag );

// inlined function declarations

static inline const char *Cvar_GetName( const cvar_t *var );
static inline const char *Cvar_GetLatchedString( const cvar_t *var );
static inline const char *Cvar_GetDefaultValue( const cvar_t *var );
static inline const char *Cvar_GetStringValue( const cvar_t *var );
static inline float     Cvar_GetFloatValue( const cvar_t *var );
static inline int       Cvar_GetIntegerValue( const cvar_t *var );
static inline cvar_flag_t   Cvar_GetFlags( const cvar_t *var );

static inline bool      Cvar_IsModified( const cvar_t *var );
static inline void      Cvar_SetModified( cvar_t *var );
static inline void      Cvar_UnsetModified( cvar_t *var );

// Medar: undefined untill used, so gcc doesn't whine
//static inline cvar_type_t	Cvar_GetType(const cvar_t *var);

// this is set each time a CVAR_USERINFO variable is changed so
// that the client knows to send it to the server
extern bool userinfo_modified;

/*

   cvar_t variables are used to hold scalar or string variables that can be changed or displayed at the console or prog code as well as accessed directly
   in C code.

   The user can access cvars from the console in three ways:
   r_draworder			prints the current value
   r_draworder 0		sets the current value to 0
   set r_draworder 0	as above, but creates the cvar if not present
   Cvars are restricted from having the same names as commands to keep this
   interface from being ambiguous.
 */

cvar_t *Cvar_Get( const char *var_name, const char *value, cvar_flag_t flags );
cvar_t *Cvar_Set( const char *var_name, const char *value );
cvar_t *Cvar_ForceSet( const char *var_name, const char *value );
cvar_t *Cvar_FullSet( const char *var_name, const char *value, cvar_flag_t flags, bool overwrite_flags );
void        Cvar_SetValue( const char *var_name, float value );
float       Cvar_Value( const char *var_name );
const char *Cvar_String( const char *var_name );
int     Cvar_Integer( const char *var_name );
void        Cmd_WriteAliases( int file );
cvar_t      *Cvar_Find( const char *var_name );
int     Cvar_CompleteCountPossible( const char *partial );
char **Cvar_CompleteBuildList( const char *partial );
char *Cvar_TabComplete( const char *partial );
void        Cvar_GetLatchedVars( cvar_flag_t flags );
void        Cvar_FixCheatVars( void );
bool    Cvar_Command( void );
void        Cvar_WriteVariables( int file );
void        Cvar_PreInit( void );
void        Cvar_Init( void );
void        Cvar_Shutdown( void );
char *Cvar_Userinfo( void );
char *Cvar_Serverinfo( void );

// inlined function implementations

static inline const char *Cvar_GetName( const cvar_t *var ) {
	return var->name;
}
static inline const char *Cvar_GetLatchedString( const cvar_t *var ) {
	return var->latched_string;
}
static inline const char *Cvar_GetDefaultValue( const cvar_t *var ) {
	return var->dvalue;
}
static inline const char *Cvar_GetStringValue( const cvar_t *var ) {
	return var->string;
}
static inline float Cvar_GetFloatValue( const cvar_t *var ) {
	return var->value;
}
static inline int Cvar_GetIntegerValue( const cvar_t *var ) {
	return var->integer;
}
static inline cvar_flag_t Cvar_GetFlags( const cvar_t *var ) {
	return var->flags;
}

static inline bool Cvar_IsModified( const cvar_t *var ) {
	return var->modified;
}
static inline void Cvar_SetModified( cvar_t *var ) {
	var->modified = ( bool )1;
}
static inline void Cvar_UnsetModified( cvar_t *var ) {
	var->modified = ( bool )0;
}

static inline cvar_flag_t Cvar_FlagSet( cvar_flag_t *flags, cvar_flag_t flag ) {
	return *flags |= flag;
}
static inline cvar_flag_t Cvar_FlagUnset( cvar_flag_t *flags, cvar_flag_t flag ) {
	return *flags &= ~flag;
}
static inline cvar_flag_t Cvar_FlagsClear( cvar_flag_t *flags ) {
	return *flags = 0;
}
static inline bool Cvar_FlagIsSet( cvar_flag_t flags, cvar_flag_t flag ) {
	return ( bool )( ( flags & flag ) != 0 );
}
