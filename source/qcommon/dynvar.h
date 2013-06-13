#ifndef DYNVAR_H
#define DYNVAR_H

// you may use these functions in Dynvar_Create to create read-only or write-only dynvars
extern const dynvar_getter_f DYNVAR_WRITEONLY;  // always returns DYNVAR_GET_WRITEONLY
extern const dynvar_setter_f DYNVAR_READONLY;   // always returns DYNVAR_SET_READONLY

// initializes the dynvar dictionary, call this function FIRST and only ONCE per execution
void Dynvar_PreInit( void );

// creates dynvar-related commands
void Dynvar_Init( void );
void Dynvar_Shutdown( void );

// creates new dynvar
dynvar_t *Dynvar_Create(
        const char *name,       // the name for dictionary-based lookup
        qboolean console,       // dynvar is console-accessible
        dynvar_getter_f getter, // user-provided getter function, called by Dynvar_GetValue
        dynvar_setter_f setter  // user-provided setter function, called by Dynvar_SetValue
);

// destroys dynvar
void Dynvar_Destroy(
        dynvar_t *dynvar
);

// returns dynvar with given name, returns NULL on error (dynvar not found)
dynvar_t *Dynvar_Lookup(
        const char *name
);

// gets name of dynvar
const char *Dynvar_GetName(
        dynvar_t *dynvar
);

// calls the getter and returns the value
dynvar_get_status_t Dynvar_GetValue(
        dynvar_t *dynvar,
        void **value    // output variable, console-accessible dynvars _must_ return char*
);

// calls the setter, automatically calls all listeners if setter returns DYNVAR_SET_OK
dynvar_set_status_t Dynvar_SetValue(
        dynvar_t *dynvar,
        void *value     // input variable, console-accessible dynvars can expect char*
);

// bypasses the setter, calls all listeners with value
void Dynvar_CallListeners(
        dynvar_t *dynvar,
        void *value
);

// adds a listener instance to this dynvar
void Dynvar_AddListener(
        dynvar_t *dynvar,
        dynvar_listener_f listener
);

// removes one listener instance
// if no instance exists, call has no effect
void Dynvar_RemoveListener(
        dynvar_t *dynvar,
        dynvar_listener_f listener
);

// gives number of console-readable, matching dynvars
// analogous to Cmd_CompleteCoundPossible
int Dynvar_CompleteCountPossible(
        const char *partial
);

// returns a vector of console-readable, matching dynvars
// analogous to Cmd_CompleteBuildList
const char **Dynvar_CompleteBuildList(
        const char *partial
);

// gives prefix-based match of console-readable dynvar
// analogous to Cmd_Complete
const char *Dynvar_CompleteDynvar(
        const char *partial
);

qboolean Dynvar_Command( void );

#endif // DYNVAR_H
