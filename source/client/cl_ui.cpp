/*
Copyright (C) 2002-2003 Victor Luchits

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "client.h"
#include "qcommon/version.h"

#include "sdl/sdl_window.h"

#include "ui/imgui.h"
#include "ui/imgui_freetype.h"
#include "ui/imgui_internal.h"
#include "ui/imgui_impl_sdl.h"
#include "ui/imgui_impl_opengl3.h"

extern SDL_Window * sdl_window;

enum UIState {
	UIState_Hidden,
	UIState_MainMenu,
	UIState_Connecting,
	UIState_GameMenu,
	UIState_DemoMenu,
};

enum MainMenuState {
	MainMenuState_ServerBrowser,
	MainMenuState_CreateServer,
	MainMenuState_Settings,
};

enum GameMenuState {
	GameMenuState_Menu,
	GameMenuState_Loadout,
	GameMenuState_Settings,
};

enum DemoMenuState {

};

enum SettingsState {
	SettingsState_General,
	SettingsState_Mouse,
	SettingsState_Keys,
	SettingsState_Video,
	SettingsState_Audio,
};

struct Server {
	const char * address;
	const char * info;
};

static ImFont * large_font;

static Server servers[ 1024 ];
static int num_servers = 0;
static int selected_server;

static UIState uistate;

static MainMenuState mainmenu_state;

static GameMenuState gamemenu_state;
static bool is_spectating;
static bool can_ready;
static bool can_unready;
static size_t selected_primary;
static size_t selected_secondary;

static SettingsState settings_state;
static bool reset_video_settings;
static int pressed_key;

static void ResetServerBrowser() {
	for( int i = 0; i < num_servers; i++ ) {
		free( const_cast< char * >( servers[ i ].address ) );
		free( const_cast< char * >( servers[ i ].info ) );
	}
	num_servers = 0;
	selected_server = -1;
}

static void RefreshServerBrowser() {
	ResetServerBrowser();

	for( const char * masterserver : MASTER_SERVERS ) {
		char buf[ 256 ];
		Q_snprintfz( buf, sizeof( buf ), "requestservers global %s %s full empty\n", masterserver, APPLICATION_NOSPACES );
		Cbuf_ExecuteText( EXEC_APPEND, buf );
	}

	Cbuf_ExecuteText( EXEC_APPEND, "requestservers local full empty\n" );
}

void UI_Init() {
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplSDL2_InitForOpenGL( sdl_window, NULL );
	ImGui_ImplOpenGL3_Init( "#version 120" );

	{
		ImGuiIO & io = ImGui::GetIO();
		io.IniFilename = NULL;
		io.KeyMap[ ImGuiKey_Tab ] = K_TAB;
		io.KeyMap[ ImGuiKey_LeftArrow ] = K_LEFTARROW;
		io.KeyMap[ ImGuiKey_RightArrow ] = K_RIGHTARROW;
		io.KeyMap[ ImGuiKey_UpArrow ] = K_UPARROW;
		io.KeyMap[ ImGuiKey_DownArrow ] = K_DOWNARROW;
		io.KeyMap[ ImGuiKey_PageUp ] = K_PGUP;
		io.KeyMap[ ImGuiKey_PageDown ] = K_PGDN;
		io.KeyMap[ ImGuiKey_Home ] = K_HOME;
		io.KeyMap[ ImGuiKey_End ] = K_END;
		io.KeyMap[ ImGuiKey_Insert ] = K_INS;
		io.KeyMap[ ImGuiKey_Delete ] = K_DEL;
		io.KeyMap[ ImGuiKey_Backspace ] = K_BACKSPACE;
		io.KeyMap[ ImGuiKey_Space ] = K_SPACE;
		io.KeyMap[ ImGuiKey_Enter ] = K_ENTER;
		io.KeyMap[ ImGuiKey_Escape ] = K_ESCAPE;
		io.KeyMap[ ImGuiKey_A ] = 'a';
		io.KeyMap[ ImGuiKey_C ] = 'c';
		io.KeyMap[ ImGuiKey_V ] = 'v';
		io.KeyMap[ ImGuiKey_X ] = 'x';
		io.KeyMap[ ImGuiKey_Y ] = 'y';
		io.KeyMap[ ImGuiKey_Z ] = 'z';

		io.Fonts->AddFontFromFileTTF( "base/fonts/Montserrat-SemiBold.ttf", 16.0f );
		large_font = io.Fonts->AddFontFromFileTTF( "base/fonts/Montserrat-Bold.ttf", 64.0f );
		ImGuiFreeType::BuildFontAtlas( io.Fonts );
	}

	{
		ImGuiStyle & style = ImGui::GetStyle();
		style.WindowRounding = 0;
		style.FramePadding = ImVec2( 8, 8 );
		style.FrameBorderSize = 0;
		style.WindowPadding = ImVec2( 16, 16 );
		style.WindowBorderSize = 0;
		style.Colors[ ImGuiCol_WindowBg ] = ImColor( 0x1a, 0x1a, 0x1a );
		style.ItemSpacing.y = 8;
	}

	ResetServerBrowser();

	uistate = UIState_MainMenu;
	mainmenu_state = MainMenuState_ServerBrowser;

	reset_video_settings = true;
}

void UI_Shutdown() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
}

void UI_TouchAllAssets() {
}

static void SettingLabel( const char * label ) {
	ImGui::AlignTextToFramePadding();
	ImGui::Text( "%s", label );
	ImGui::SameLine( 200 );
}

template< size_t maxlen >
static void CvarTextbox( const char * label, const char * cvar_name, const char * def, cvar_flag_t flags ) {
	SettingLabel( label );

	cvar_t * cvar = Cvar_Get( cvar_name, def, flags );

	char buf[ maxlen + 1 ];
	Q_strncpyz( buf, cvar->string, sizeof( buf ) );

	char unique[ 128 ];
	Q_snprintfz( unique, sizeof( unique ), "##%s", cvar_name );
	ImGui::InputText( unique, buf, sizeof( buf ) );

	Cvar_Set( cvar_name, buf );
}

static void CvarCheckbox( const char * label, const char * cvar_name, const char * def, cvar_flag_t flags ) {
	SettingLabel( label );

	cvar_t * cvar = Cvar_Get( cvar_name, def, flags );

	char unique[ 128 ];
	Q_snprintfz( unique, sizeof( unique ), "##%s", cvar_name );
	bool val = cvar->integer != 0;
	ImGui::Checkbox( unique, &val );

	Cvar_Set( cvar_name, val ? "1" : "0" );
}

static void CvarSliderInt( const char * label, const char * cvar_name, int lo, int hi, const char * def, cvar_flag_t flags, const char * format = NULL ) {
	SettingLabel( label );

	cvar_t * cvar = Cvar_Get( cvar_name, def, flags );

	char unique[ 128 ];
	Q_snprintfz( unique, sizeof( unique ), "##%s", cvar_name );
	int val = cvar->integer;
	ImGui::SliderInt( unique, &val, lo, hi, format );

	char buf[ 128 ];
	Q_snprintfz( buf, sizeof( buf ), "%d", val );
	Cvar_Set( cvar_name, buf );
}

static void CvarSliderFloat( const char * label, const char * cvar_name, float lo, float hi, const char * def, cvar_flag_t flags ) {
	SettingLabel( label );

	cvar_t * cvar = Cvar_Get( cvar_name, def, flags );

	char unique[ 128 ];
	Q_snprintfz( unique, sizeof( unique ), "##%s", cvar_name );
	float val = cvar->value;
	ImGui::SliderFloat( unique, &val, lo, hi, "%.2f" );

	char buf[ 128 ];
	Q_snprintfz( buf, sizeof( buf ), "%f", val );
	Cvar_Set( cvar_name, buf );
}

// TODO: put this somewhere else
void CG_GetBoundKeysString( const char *cmd, char *keys, size_t keysSize );

static void KeyBindButton( const char * label, const char * command ) {
	SettingLabel( label );
	ImGui::PushID( label );

	char keys[ 128 ];
	CG_GetBoundKeysString( command, keys, sizeof( keys ) );
	if( ImGui::Button( keys, ImVec2( 200, 0 ) ) ) {
		ImGui::OpenPopup( label );
		pressed_key = -1;
	}

	if( ImGui::BeginPopupModal( label, NULL, ImGuiWindowFlags_NoDecoration ) ) {
		ImGui::Text( "Press a key to set a new bind, or press ESCAPE to cancel." );
		if( pressed_key != -1 ) {
			if( pressed_key != K_ESCAPE ) {
				Key_SetBinding( pressed_key, command );
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::PopID();
}

static void PushDisabled( bool disabled ) {
	if( disabled ) {
		ImGui::PushItemFlag( ImGuiItemFlags_Disabled, true );
		ImGui::PushStyleVar( ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f );
	}
}

static void PopDisabled( bool disabled ) {
	if( disabled ) {
		ImGui::PopItemFlag();
		ImGui::PopStyleVar();
	}
}

static void SettingsGeneral() {
	ImGui::Text( "These settings are saved automatically" );

	CvarTextbox< MAX_NAME_BYTES >( "Name", "name", "Player", CVAR_USERINFO | CVAR_ARCHIVE );
	CvarSliderInt( "FOV", "fov", 60, 140, "100", CVAR_ARCHIVE );
	CvarCheckbox( "Show FPS", "cg_showFPS", "0", CVAR_ARCHIVE );
}

static void SettingsMouse() {
	ImGui::Text( "These settings are saved automatically" );

	CvarSliderFloat( "Sensitivity", "sensitivity", 1.0f, 10.0f, "3", CVAR_ARCHIVE );
	CvarSliderFloat( "Horizontal sensitivity", "horizontalsensscale", 0.5f, 2.0f, "1.0", CVAR_ARCHIVE );
	CvarSliderFloat( "Acceleration", "m_accel", 0.0f, 1.0f, "0", CVAR_ARCHIVE );
}

static void SettingsKeys() {
	ImGui::Text( "These settings are saved automatically" );

	ImGui::BeginChild( "binds" );

	ImGui::Separator();
	ImGui::Text( "Movement" );
	ImGui::Separator();

	KeyBindButton( "Forward", "+forward" );
	KeyBindButton( "Back", "+back" );
	KeyBindButton( "Left", "+left" );
	KeyBindButton( "Right", "+right" );
	KeyBindButton( "Jump", "+jump" );
	KeyBindButton( "Dash/walljump", "+special" );
	KeyBindButton( "Crouch", "+crouch" );
	KeyBindButton( "Walk", "+walk" );

	ImGui::Separator();
	ImGui::Text( "Actions" );
	ImGui::Separator();

	KeyBindButton( "Attack", "+attack" );
	KeyBindButton( "Drop bomb", "drop" );
	KeyBindButton( "Shop", "gametypemenu" );
	KeyBindButton( "Scoreboard", "+scores" );
	KeyBindButton( "Chat", "messagemode" );
	KeyBindButton( "Team chat", "messagemode2" );
	KeyBindButton( "Zoom", "+zoom" );

	ImGui::Separator();
	ImGui::Text( "Weapons" );
	ImGui::Separator();

	KeyBindButton( "Weapon 1", "weapon 1" );
	KeyBindButton( "Weapon 2", "weapon 2" );
	KeyBindButton( "Weapon 3", "weapon 3" );
	KeyBindButton( "Weapon 4", "weapon 4" );
	KeyBindButton( "Next weapon", "weapnext" );
	KeyBindButton( "Previous weapon", "weapprev" );
	KeyBindButton( "Last weapon", "weaplast" );

	ImGui::Separator();
	ImGui::Text( "Voice lines" );
	ImGui::Separator();

	KeyBindButton( "Yes", "vsay yes" );
	KeyBindButton( "No", "vsay no" );
	KeyBindButton( "Thanks", "vsay thanks" );
	KeyBindButton( "Good game", "vsay goodgame" );
	KeyBindButton( "Boomstick", "vsay boomstick" );
	KeyBindButton( "Shut up", "vsay shutup" );

	ImGui::Separator();
	ImGui::Text( "Specific weapons" );
	ImGui::Separator();

	KeyBindButton( "Gunblade", "use gb" );
	// KeyBindButton( "Machine Gun", "use mg" );
	KeyBindButton( "Disrespect Gun", "use rg" );
	KeyBindButton( "Grenade Launcher", "use gl" );
	KeyBindButton( "Rocket Launcher", "use rl" );
	KeyBindButton( "Plasma Gun", "use pg" );
	KeyBindButton( "Lasergun", "use lg" );
	KeyBindButton( "Electrobolt", "use eb" );

	ImGui::EndChild();
}

static void SettingsVideo() {
	static WindowMode mode;

	if( reset_video_settings ) {
		mode = VID_GetWindowMode();
		reset_video_settings = false;
	}

	ImGui::Text( "Changing resolution is buggy and you should restart the game after doing it" );

	SettingLabel( "Fullscreen" );
	bool fullscreen = mode.fullscreen != FullScreenMode_Windowed;
	ImGui::Checkbox( "##vid_fullscreen", &fullscreen );

	PushDisabled( !fullscreen );

	SettingLabel( "Borderless fullscreen" );
	bool borderless = mode.fullscreen == FullScreenMode_FullscreenBorderless;
	ImGui::Checkbox( "##borderless", &borderless );

	mode.fullscreen = FullScreenMode_Windowed;
	if( fullscreen )
		mode.fullscreen = borderless ? FullScreenMode_FullscreenBorderless : FullScreenMode_Fullscreen;

	SettingLabel( "Video mode" );
	ImGui::PushItemWidth( 200 );

	if( fullscreen && mode.video_mode.frequency == 0 ) {
		mode.video_mode = VID_GetVideoMode( 0 );
	}

	char preview[ 128 ] = "";
	if( fullscreen )
		Q_snprintfz( preview, sizeof( preview ), "%dx%d %dHz", mode.video_mode.width, mode.video_mode.height, mode.video_mode.frequency );

	if( ImGui::BeginCombo( "##mode", preview ) ) {
		for( int i = 0; i < VID_GetNumVideoModes(); i++ ) {
			VideoMode video_mode = VID_GetVideoMode( i );

			char buf[ 128 ];
			Q_snprintfz( buf, sizeof( buf ), "%dx%d %dHz", video_mode.width, video_mode.height, video_mode.frequency );

			bool is_selected = mode.video_mode.width == video_mode.width && mode.video_mode.height == video_mode.height && mode.video_mode.frequency == video_mode.frequency;
			if( ImGui::Selectable( buf, is_selected ) ) {
				mode.video_mode = video_mode;
			}
		}
		ImGui::EndCombo();
	}
	ImGui::PopItemWidth();

	PopDisabled( !fullscreen );

	if( ImGui::Button( "Apply mode changes" ) ) {
		char buf[ 128 ];
		VID_WindowModeToString( buf, sizeof( buf ), mode );
		Cvar_Set( "vid_mode", buf );
	}

	ImGui::SameLine();
	if( ImGui::Button( "Discard mode changes" ) ) {
		reset_video_settings = true;
	}

	ImGui::Separator();

	ImGui::Text( "These settings are saved automatically" );

	{
		SettingLabel( "Anti-aliasing" );

		cvar_t * cvar = Cvar_Get( "r_samples", "0", CVAR_ARCHIVE );
		int samples = cvar->integer;

		char current_samples[ 16 ];
		if( samples == 0 )
			Q_snprintfz( current_samples, sizeof( current_samples ), "Off" );
		else
			Q_snprintfz( current_samples, sizeof( current_samples ), "%dx", samples );

		ImGui::PushItemWidth( 100 );
		if( ImGui::BeginCombo( "##r_samples", current_samples ) ) {
			if( ImGui::Selectable( "Off", samples == 0 ) )
				samples = 0;
			if( samples == 0 )
				ImGui::SetItemDefaultFocus();

			for( int i = 2; i <= 16; i *= 2 ) {
				char buf[ 16 ];
				Q_snprintfz( buf, sizeof( buf ), "%dx", i );
				if( ImGui::Selectable( buf, i == samples ) )
					samples = i;
				if( i == samples )
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();

		char buf[ 16 ];
		Q_snprintfz( buf, sizeof( buf ), "%d", samples );
		Cvar_Set( "r_samples", buf );
	}

	CvarCheckbox( "Vsync", "vid_vsync", "0", CVAR_ARCHIVE );

	{
		SettingLabel( "Max FPS" );

		constexpr int values[] = { 60, 75, 120, 144, 165, 180, 200, 240, 333, 500, 1000 };

		cvar_t * cvar = Cvar_Get( "cl_maxfps", "250", CVAR_ARCHIVE );
		int maxfps = cvar->integer;

		char current[ 16 ];
		Q_snprintfz( current, sizeof( current ), "%d", maxfps );

		ImGui::PushItemWidth( 100 );
		if( ImGui::BeginCombo( "##cl_maxfps", current ) ) {
			for( int value : values ) {
				char buf[ 16 ];
				Q_snprintfz( buf, sizeof( buf ), "%d", value );
				if( ImGui::Selectable( buf, maxfps == value ) )
					maxfps = value;
				if( value == maxfps )
					ImGui::SetItemDefaultFocus();
			}

			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();

		char buf[ 16 ];
		Q_snprintfz( buf, sizeof( buf ), "%d", maxfps );
		Cvar_Set( "cl_maxfps", buf );
	}
}

static void SettingsAudio() {
	ImGui::Text( "These settings are saved automatically" );

	CvarSliderFloat( "Master volume", "s_volume", 0.0f, 1.0f, "0.8", CVAR_ARCHIVE );
	CvarSliderFloat( "Music volume", "s_musicvolume", 0.0f, 1.0f, "1.0", CVAR_ARCHIVE );
	CvarCheckbox( "Mute when alt-tabbed", "s_muteinbackground", "1", CVAR_ARCHIVE );
}

static void Settings() {
	if( ImGui::Button( "GENERAL" ) ) {
		settings_state = SettingsState_General;
	}

	ImGui::SameLine();

	if( ImGui::Button( "MOUSE" ) ) {
		settings_state = SettingsState_Mouse;
	}

	ImGui::SameLine();

	if( ImGui::Button( "KEYS" ) ) {
		settings_state = SettingsState_Keys;
	}

	ImGui::SameLine();

	if( ImGui::Button( "VIDEO" ) ) {
		reset_video_settings = true;
		settings_state = SettingsState_Video;
	}

	ImGui::SameLine();

	if( ImGui::Button( "SOUND" ) ) {
		settings_state = SettingsState_Audio;
	}

	if( settings_state == SettingsState_General )
		SettingsGeneral();
	else if( settings_state == SettingsState_Mouse )
		SettingsMouse();
	else if( settings_state == SettingsState_Keys )
		SettingsKeys();
	else if( settings_state == SettingsState_Video )
		SettingsVideo();
	else if( settings_state == SettingsState_Audio )
		SettingsAudio();
}

static void ServerBrowser() {
	if( ImGui::Button( "Refresh" ) ) {
		RefreshServerBrowser();
	}

	ImGui::BeginChild( "servers" );
	ImGui::Columns( 2, "serverbrowser", false );
	ImGui::SetColumnWidth( 0, 200 );
	ImGui::Text( "Address" );
	ImGui::NextColumn();
	ImGui::Text( "Info" );
	ImGui::NextColumn();
	for( int i = 0; i < num_servers; i++ ) {
		if( ImGui::Selectable( servers[ i ].address, i == selected_server, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick ) ) {
			if( ImGui::IsMouseDoubleClicked( 0 ) ) {
				char buf[ 256 ];
				Q_snprintfz( buf, sizeof( buf ), "connect \"%s\"\n", servers[ i ].address );
				Cbuf_ExecuteText( EXEC_APPEND, buf );
			}
			selected_server = i;
		}
		ImGui::NextColumn();
		ImGui::Text( "%s", servers[ i ].info );
		ImGui::NextColumn();
	}

	ImGui::Columns( 1 );
	ImGui::EndChild();
}

static void CreateServer() {
	CvarTextbox< 128 >( "Server name", "sv_hostname", APPLICATION " server", CVAR_SERVERINFO | CVAR_ARCHIVE );

	{
		cvar_t * cvar = Cvar_Get( "sv_maxclients", "16", CVAR_SERVERINFO | CVAR_LATCH );
		int maxclients = cvar->integer;

		SettingLabel( "Max players" );
		ImGui::PushItemWidth( 150 );
		ImGui::InputInt( "##sv_maxclients", &maxclients );
		ImGui::PopItemWidth();

		maxclients = max( maxclients, 1 );
		maxclients = min( maxclients, 64 );

		char buf[ 128 ];
		Q_snprintfz( buf, sizeof( buf ), "%d", maxclients );
		Cvar_Set( "sv_maxclients", buf );
	}

	static int selectedmap = 0;
	{
		SettingLabel( "Map" );
		char selectedmapname[ 128 ];
		ML_GetMapByNum( selectedmap, selectedmapname, sizeof( selectedmapname ) );

		ImGui::PushItemWidth( 200 );
		if( ImGui::BeginCombo( "##map", selectedmapname ) ) {
			for( int i = 0; true; i++ ) {
				char mapname[ 128 ];
				if( ML_GetMapByNum( i, mapname, sizeof( mapname ) ) == 0 )
					break;

				if( ImGui::Selectable( mapname, i == selectedmap ) )
					selectedmap = i;
				if( i == selectedmap )
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();
	}

	CvarCheckbox( "Public", "sv_public", "0", CVAR_ARCHIVE | CVAR_LATCH );

	if( ImGui::Button( "Create server" ) ) {
		char mapname[ 128 ];
		if( ML_GetMapByNum( selectedmap, mapname, sizeof( mapname ) ) != 0 ) {
			char buf[ 256 ];
			Q_snprintfz( buf, sizeof( buf ), "map \"%s\"\n", mapname );
			Cbuf_ExecuteText( EXEC_APPEND, buf );
		}
	}
}

static void MainMenu() {
	ImGui::SetNextWindowPos( ImVec2() );
	ImGui::SetNextWindowSize( ImVec2( viddef.width, viddef.height ) );
	ImGui::Begin( "mainmenu", NULL, ImGuiWindowFlags_NoDecoration );

	ImVec2 window_padding = ImGui::GetStyle().WindowPadding;

	ImGui::BeginChild( "mainmenubody", ImVec2( 0, -ImGui::GetFrameHeightWithSpacing() + window_padding.y ) );

	ImGui::SetCursorPosX( 2 + 2 * sinf( cls.monotonicTime / 20.0f ) );
	ImGui::PushFont( large_font );
	ImGui::Text( "COCAINE DIESEL" );
	ImGui::PopFont();

	if( ImGui::Button( "PLAY" ) ) {
		mainmenu_state = MainMenuState_ServerBrowser;
	}

	ImGui::SameLine();

	if( ImGui::Button( "CREATE SERVER" ) ) {
		mainmenu_state = MainMenuState_CreateServer;
	}

	ImGui::SameLine();

	if( ImGui::Button( "SETTINGS" ) ) {
		mainmenu_state = MainMenuState_Settings;
		settings_state = SettingsState_General;
	}

	ImGui::SameLine();

	if( ImGui::Button( "QUIT" ) ) {
		CL_Quit();
	}

	ImGui::Separator();

	if( mainmenu_state == MainMenuState_ServerBrowser ) {
		ServerBrowser();
	}
	else if( mainmenu_state == MainMenuState_CreateServer ) {
		CreateServer();
	}
	else {
		Settings();
	}

	ImGui::EndChild();

	const char * buf = APP_VERSION u8" \u00A9 AHA CHEERS";
	ImVec2 size = ImGui::CalcTextSize( buf );
	ImGui::SetCursorPosX( ImGui::GetWindowWidth() - size.x - window_padding.x - 1 - sinf( cls.monotonicTime / 18.0f ) );
	ImGui::Text( buf );

	ImGui::End();
}

static void GameMenuButton( const char * label, const char * command, bool * clicked = NULL ) {
	if( ImGui::Button( label, ImVec2( -1, 0 ) ) ) {
		char buf[ 256 ];
		Q_snprintfz( buf, sizeof( buf ), "%s\n", command );
		Cbuf_ExecuteText( EXEC_APPEND, buf );
		if( clicked != NULL )
			*clicked = true;
	}
}

static void GameMenu() {
	ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( ImColor( 0x1a, 0x1a, 0x1a, 192 ) ) );
	bool should_close = false;

	if( gamemenu_state == GameMenuState_Menu ) {
		ImGui::SetNextWindowPosCenter();
		ImGui::SetNextWindowSize( ImVec2( 300, 0 ) );
		ImGui::Begin( "gamemenu", NULL, ImGuiWindowFlags_NoDecoration );

		if( is_spectating ) {
			GameMenuButton( "Join the game", "join", &should_close );
		}
		else {
			GameMenuButton( "Spectate", "spec", &should_close );

			if( can_ready )
				GameMenuButton( "Ready", "ready", &should_close );
			if( can_unready )
				GameMenuButton( "Uneady", "unready", &should_close );

			GameMenuButton( "Change loadout", "gametypemenu", &should_close );
		}

		if( ImGui::Button( "Settings", ImVec2( -1, 0 ) ) ) {
			gamemenu_state = GameMenuState_Settings;
			settings_state = SettingsState_General;
		}

		GameMenuButton( "Disconnect to main menu", "disconnect", &should_close );
		GameMenuButton( "Exit to desktop", "quit", &should_close );

		ImGui::End();
	}
	else if( gamemenu_state == GameMenuState_Loadout ) {
		ImGui::SetNextWindowPosCenter();
		ImGui::SetNextWindowSize( ImVec2( 300, 0 ) );
		ImGui::Begin( "loadout", NULL, ImGuiWindowFlags_NoDecoration );

		ImGui::Text( "Primary weapons" );

		// this has to match up with the order in player.as
		// TODO: should make this less fragile
		const char * primaries[] = { "EB + RL", "RL + LG", "EB + LG" };
		const char * secondaries[] = { "PG", "RG", "GL" };

		ImGui::Columns( ARRAY_COUNT( primaries ), NULL, false );

		for( size_t i = 0; i < ARRAY_COUNT( primaries ); i++ ) {
			char buf[ 128 ];
			int key = '1' + int( i );
			Q_snprintfz( buf, sizeof( buf ), "%c: %s", key, primaries[ i ] );
			if( ImGui::Selectable( buf, i == selected_primary ) || pressed_key == key ) {
				selected_primary = i;
			}
			ImGui::NextColumn();
		}

		ImGui::Columns( 1 );

		ImGui::Text( "Secondary weapon" );

		ImGui::Columns( ARRAY_COUNT( secondaries ), NULL, false );

		bool selected_with_number = false;
		for( size_t i = 0; i < ARRAY_COUNT( secondaries ); i++ ) {
			char buf[ 128 ];
			int key = '1' + int( i + ARRAY_COUNT( primaries ) );
			Q_snprintfz( buf, sizeof( buf ), "%c: %s", key, secondaries[ i ] );
			if( ImGui::Selectable( buf, i == selected_secondary ) || pressed_key == key ) {
				selected_secondary = i;
				selected_with_number = pressed_key == key;
			}
			ImGui::NextColumn();
		}

		if( ImGui::Button( "OK", ImVec2( -1, 0 ) ) || selected_with_number ) {
			const char * primaries_weapselect[] = { "ebrl", "rllg", "eblg" };

			char buf[ 128 ];
			Q_snprintfz( buf, sizeof( buf ), "weapselect %s %s\n",
				primaries_weapselect[ selected_primary ], secondaries[ selected_secondary ] );
			Cbuf_ExecuteText( EXEC_APPEND, buf );
			should_close = true;
		}

		if( pressed_key == K_ESCAPE || pressed_key == 'b' ) {
			should_close = true;
		}

		ImGui::End();
	}
	else if( gamemenu_state == GameMenuState_Settings ) {
		ImVec2 pos = ImGui::GetIO().DisplaySize;
		pos.x *= 0.5f;
		pos.y *= 0.5f;
		ImGui::SetNextWindowPos( pos, ImGuiCond_Always, ImVec2( 0.5f, 0.5f ) );
		ImGui::SetNextWindowSize( ImVec2( 600, 500 ) );
		ImGui::Begin( "settings", NULL, ImGuiWindowFlags_NoDecoration );

		Settings();

		ImGui::End();
	}

	if( pressed_key == K_ESCAPE || should_close ) {
		uistate = UIState_Hidden;
		CL_SetKeyDest( key_game );
	}

	ImGui::PopStyleColor();
}

static void DemoMenu() {
	ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( ImColor( 0x1a, 0x1a, 0x1a, 192 ) ) );
	bool should_close = false;

	ImVec2 pos = ImGui::GetIO().DisplaySize;
	pos.x *= 0.5f;
	pos.y *= 0.8f;
	ImGui::SetNextWindowPos( pos, ImGuiCond_Always, ImVec2( 0.5f, 0.5f ) );
	ImGui::SetNextWindowSize( ImVec2( 600, 0 ) );
	ImGui::Begin( "demomenu", NULL, ImGuiWindowFlags_NoDecoration );

	GameMenuButton( cls.demo.paused ? "Play" : "Pause", "demopause" );
	GameMenuButton( "Jump +15s", "demojump +15" );
	GameMenuButton( "Jump -15s", "demojump -15" );

	GameMenuButton( "Disconnect to main menu", "disconnect", &should_close );
	GameMenuButton( "Exit to desktop", "quit", &should_close );

	ImGui::End();

	if( pressed_key == K_ESCAPE || should_close ) {
		uistate = UIState_Hidden;
		CL_SetKeyDest( key_game );
	}

	ImGui::PopStyleColor();
}

void UI_Refresh( bool background, bool showCursor ) {
	if( uistate == UIState_Hidden )
		return;

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame( sdl_window );
	ImGui::NewFrame();

	if( uistate == UIState_MainMenu ) {
		MainMenu();
	}

	if( uistate == UIState_Connecting ) {
		ImGui::SetNextWindowPos( ImVec2() );
		ImGui::SetNextWindowSize( ImVec2( viddef.width, viddef.height ) );
		ImGui::Begin( "mainmenu", NULL, ImGuiWindowFlags_NoDecoration );

		ImGui::Text( "Connecting..." );

		ImGui::End();
	}

	if( uistate == UIState_GameMenu ) {
		GameMenu();
	}

	if( uistate == UIState_DemoMenu ) {
		DemoMenu();
	}

	// ImGui::ShowDemoWindow();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );

	Cbuf_Execute();
}

void UI_UpdateConnectScreen( bool background ) {
	uistate = UIState_Connecting;
	UI_Refresh( background, false );
}

void UI_KeyEvent( bool mainContext, int key, bool down ) {
	if( down ) {
		pressed_key = key;
	}

	if( key != K_ESCAPE ) {
		if( key == K_MWHEELDOWN || key == K_MWHEELUP ) {
			if( down )
				ImGui::GetIO().MouseWheel += key == K_MWHEELDOWN ? -1 : 1;
		}
		else {
			ImGui::GetIO().KeysDown[ key ] = down;
		}
	}
}

void UI_CharEvent( bool mainContext, wchar_t key ) {
	ImGui::GetIO().AddInputCharacter( key );
}

void UI_ShowMainMenu() {
	uistate = UIState_MainMenu;
	mainmenu_state = MainMenuState_ServerBrowser;
	CL_SoundModule_StartMenuMusic();
	RefreshServerBrowser();
}

void UI_ShowGameMenu( bool spectating, bool ready, bool unready ) {
	uistate = UIState_GameMenu;
	gamemenu_state = GameMenuState_Menu;
	pressed_key = -1;
	is_spectating = spectating;
	can_ready = ready;
	can_unready = unready;
	CL_SetKeyDest( key_menu );
}

void UI_ShowDemoMenu() {
	uistate = UIState_DemoMenu;
	pressed_key = -1;
	CL_SetKeyDest( key_menu );
}

void UI_HideMenu() {
	uistate = UIState_Hidden;
}

void UI_AddToServerList( const char * address, const char *info ) {
	if( size_t( num_servers ) < ARRAY_COUNT( servers ) ) {
		servers[ num_servers ].address = strdup( address );
		servers[ num_servers ].info = strdup( info );
		num_servers++;
	}
}

void UI_MouseSet( bool mainContext, int mx, int my, bool showCursor ) {
}

bool UI_MouseHover( bool mainContext ) {
	return false;
}

void UI_ShowLoadoutMenu( int primary, int secondary ) {
	uistate = UIState_GameMenu;
	gamemenu_state = GameMenuState_Loadout;
	pressed_key = -1;
	selected_primary = primary;
	selected_secondary = secondary;
	CL_SetKeyDest( key_menu );
}
