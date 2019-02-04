void Con_Init();
void Con_Shutdown();

void Con_Print( const char * str );
void Con_Clear();
void Con_Draw();

struct qfontface_s;
void Con_DrawChat( int x, int y, int width, struct qfontface_s *font );
void Con_SetMessageMode();

void Con_ToggleConsole();
bool Con_IsVisible();
void Con_Close();

// nuke this
void Con_MessageKeyDown( int key );
void Con_MessageCharEvent( wchar_t key );
