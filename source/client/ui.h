#pragma once

void UI_Init();
void UI_Shutdown();
void UI_TouchAllAssets();
void UI_KeyEvent( bool mainContext, int key, bool down );
void UI_CharEvent( bool mainContext, wchar_t key );
void UI_Refresh( bool backGround, bool showCursor );
void UI_UpdateConnectScreen( bool backGround );

void UI_ShowMainMenu();
void UI_ShowGameMenu( bool spectating, bool can_ready, bool can_unready );
void UI_ShowLoadoutMenu( int primary, int secondary );
void UI_ShowDemoMenu();
void UI_HideMenu();

void UI_AddToServerList( const char *adr, const char *info );
void UI_MouseSet( bool mainContext, int mx, int my, bool showCursor );
bool UI_MouseHover( bool mainContext );
