/**
 * Funcdefs
 */

funcdef void CmdFunction();

namespace CGame
{

namespace Camera
{

/**
 * Global functions
 */
Viewport@ GetViewport() {}
Camera@ GetMainCamera() {}
float CalcVerticalFov(float fovX, float width, float height) {}
float CalcHorizontalFov(float fovY, float width, float height) {}
Vec3 SmoothPredictedSteps(Vec3&in org) {}
/**
 * Viewport
 */
class Viewport
{
	/* properties */
	const int x;
	const int y;
	const int width;
	const int height;

	/* methods */
	int get_aspectRatio() const {}
	int get_screenWidth() const {}
	int get_screenHeight() const {}
	float get_screenAspectRatio() const {}
	float get_screenPixelRatio() const {}

}

/**
 * Camera
 */
class Camera
{
	/* properties */
	int type;
	int POVent;
	bool flipped;
	bool thirdPerson;
	bool playerPrediction;
	bool drawWeapon;
	bool draw2D;
	float fovX;
	float fovY;
	float stereoSeparation;
	Vec3 origin;
	Vec3 angles;
	Vec3 velocity;
	Mat3 axis;

	/* methods */
	Refdef@ get_refdef() const {}

}

/**
 * Refdef
 */
class Refdef
{
	/* properties */
	int x;
	int y;
	int width;
	int height;
	int rdflags;
	int scissorX;
	int scissorY;
	int scissorWidth;
	int scissorHeight;
	int orthoX;
	int orthoY;
	Vec3 viewOrigin;

	/* methods */
	Vec3 transformToScreen(const Vec3&in) const {}

}


}


namespace Cmd
{

/**
 * Global functions
 */
void AddCommand(const String&in, CmdFunction@ f) {}
void RemoveCommand(const String&in) {}
uint Argc() {}
const String@ Argv(uint index) {}
const String@ Args() {}

}


namespace Input
{

/**
 * Global functions
 */
Touch@ GetTouch(int id) {}
Touchpad@ GetTouchpad(int id) {}
Vec4 GetThumbsticks() {}
float GetSensitivityScale(float sens, float zoomSens) {}
/**
 * Touch
 */
class Touch
{
	/* properties */
	const bool down;
	const int x;
	const int y;
	const int64 time;
	const int area;
	const bool areaValid;

}

/**
 * Touchpad
 */
class Touchpad
{
	/* properties */
	int x;
	int y;
	const int touch;

}


}


namespace Scene
{

/**
 * Global functions
 */
void PlaceRotatedModelOnTag(Entity@ ent, const Entity@ dest, const Orientation&in) {}
void PlaceModelOnTag(Entity@ ent, const Entity@ dest, const Orientation&in) {}
bool GrabTag(Orientation&out, const Entity@ ent, const String&in) {}
Orientation MoveToTag(const Orientation&in space, const Orientation&in tag) {}
Boneposes@ RegisterTemporaryExternalBoneposes(ModelSkeleton@) {}
Boneposes@ RegisterTemporaryExternalBoneposes(int numBones) {}
bool LerpSkeletonPoses(ModelSkeleton@, int frame, int oldFrame, Boneposes@ boneposes, float frac) {}
void TransformBoneposes(ModelSkeleton@, Boneposes@ boneposes, Boneposes@ sourceBoneposes) {}
void RecurseBlendSkeletalBone(ModelSkeleton@, Boneposes@ inBoneposes, Boneposes@ outBoneposes, int root, float frac) {}
void RotateBonePoses(const Vec3&in angles, Boneposes@ inBoneposes, int[]@ rotators) {}
void SpawnPolyQuad(const Vec3&in, const Vec3&in, const Vec3&in, const Vec3&in, float stx, float sty, const Vec4&in, int64 dieTime, int64 fadeTime, ShaderHandle@, int tag) {}
void SpawnPolyBeam(const Vec3&in start, const Vec3&in end, const Vec4&in, int width, int64 dieTime, int64 fadeTime, ShaderHandle@, int shaderLength, int tag) {}
void AddEntityToScene(Entity@ ent) {}
void AddLightToScene(const Vec3&in origin, float radius, int color) {}
/**
 * Entity
 */
class Entity
{
	/* properties */
	int rtype;
	int renderfx;
	ModelHandle@ model;
	int frame;
	Mat3 axis;
	Vec3 origin;
	Vec3 origin2;
	Vec3 lightingOrigin;
	Boneposes@ boneposes;
	Boneposes@ oldBoneposes;
	int shaderRGBA;
	ShaderHandle@ customShader;
	int64 shaderTime;
	int oldFrame;
	float backLerp;
	float scale;
	float radius;
	float rotation;
	SkinHandle@ customSkin;

	/* behaviors */

	/* factories */
	Entity@ Entity() {}
	Entity@ Entity(const Entity&in) {}

	/* methods */
	void reset() {}
	Entity& opAssign(const Entity&in) {}

}

/**
 * Orientation
 */
class Orientation
{
	/* properties */
	Vec3 origin;
	Mat3 axis;

	/* behaviors */
	Orientation() {}
	Orientation(const Orientation&in) {}

}

/**
 * Boneposes
 */
class Boneposes
{
}


}


namespace Screen
{

/**
 * Global functions
 */
void ShowOverlayMenu(int state, bool showCursor) {}
int FontHeight(FontHandle@ font) {}
void DrawPic(int x, int y, int w, int h, ShaderHandle@ shader, const Vec4&in color, float s1 = 0.0, float t1 = 0.0, float s2 = 1.0, float t2 = 1.0) {}
void DrawPic(int x, int y, int w, int h, ShaderHandle@ shader, float s1 = 0.0, float t1 = 0.0, float s2 = 1.0, float t2 = 1.0) {}
int DrawString(int x, int y, int align, const String&in str, FontHandle@ font, const Vec4&in color) {}
int DrawString(int x, int y, int align, const String&in str, FontHandle@ font) {}
int DrawStringWidth(int x, int y, int align, const String&in str, int maxWidth, FontHandle@ font, const Vec4&in color) {}
int DrawStringWidth(int x, int y, int align, const String&in str, int maxWidth, FontHandle@ font) {}
void DrawClampString(int x, int y, const String&in str, int xMin, int yMin, int xMax, int yMax, FontHandle@ font, const Vec4&in color) {}
void DrawClampString(int x, int y, const String&in str, int xMin, int yMin, int xMax, int yMax, FontHandle@ font) {}
int DrawClampMultineString(int x, int y, const String&in str, int maxWidth, int maxLines, FontHandle@ font, const Vec4&in color) {}
int DrawClampMultineString(int x, int y, const String&in str, int maxWidth, int maxLines, FontHandle@ font) {}
void DrawRawChar(int x, int y, int chr, FontHandle@ font, const Vec4&in color) {}
void DrawRawChar(int x, int y, int chr, FontHandle@ font) {}
void DrawClampChar(int x, int y, int chr, int xMin, int yMin, int xMax, int yMax, FontHandle@ font, const Vec4&in color) {}
void DrawClampChar(int x, int y, int chr, int xMin, int yMin, int xMax, int yMax, FontHandle@ font) {}
int StringWidth(const String&in str, FontHandle@ font, int maxLen = 0) {}
int StrlenForWidth(const String&in str, FontHandle@ font, int maxWidth = 0) {}

}


namespace Sound
{

/**
 * Global functions
 */
void AddLoopSound(SoundHandle@, int entnum, float fvol, float attenuation) {}
void StartRelativeSound(SoundHandle@, int entnum, int channel, float fvol, float attenuation) {}
void StartGlobalSound(SoundHandle@, int channel, float fvol) {}
void StartLocalSound(SoundHandle@, int channel, float fvol) {}
void StartFixedSound(SoundHandle@, const Vec3&in origin, int channel, float fvol, float attenuation) {}
void SetEntitySpatilization(int entnum, const Vec3&in origin, const Vec3&in velocity) {}

}


/**
 * Enums
 */

enum cg_limits_e
{
	CG_MAX_TOUCHES = 0xa,
}

enum cg_touchpad_e
{
	TOUCHPAD_MOVE = 0x0,
	TOUCHPAD_VIEW = 0x1,
	TOUCHPAD_COUNT = 0x2,
}

enum cg_toucharea_e
{
	TOUCHAREA_NONE = 0x0,
	TOUCHAREA_HUD = 0x1,
	TOUCHAREA_SUB_SHIFT = 0x10,
	TOUCHAREA_MASK = 0xffff,
}

enum cg_cameratype_e
{
	VIEWDEF_DEMOCAM = 0x0,
	VIEWDEF_PLAYERVIEW = 0x1,
	VIEWDEF_OVERHEAD = 0x2,
}

enum cg_rdflags_e
{
	RDF_UNDERWATER = 0x1,
	RDF_NOWORLDMODEL = 0x2,
	RDF_SKYPORTALINVIEW = 0x10,
	RDF_FLIPPED = 0x20,
	RDF_WORLDOUTLINES = 0x40,
	RDF_CROSSINGWATER = 0x80,
	RDF_USEORTHO = 0x100,
	RDF_BLURRED = 0x200,
}

enum cg_overlayMenuState_e
{
	OVERLAY_MENU_LEFT = 0xffffffff,
	OVERLAY_MENU_HIDDEN = 0x0,
	OVERLAY_MENU_RIGHT = 0x1,
}

enum cg_fontStyle_e
{
	QFONT_STYLE_NONE = 0x0,
	QFONT_STYLE_ITALIC = 0x1,
	QFONT_STYLE_BOLD = 0x2,
	QFONT_STYLE_MASK = 0x3,
}

enum cg_alingment_e
{
	ALIGN_LEFT_TOP = 0x0,
	ALIGN_CENTER_TOP = 0x1,
	ALIGN_RIGHT_TOP = 0x2,
	ALIGN_LEFT_MIDDLE = 0x3,
	ALIGN_CENTER_MIDDLE = 0x4,
	ALIGN_RIGHT_MIDDLE = 0x5,
	ALIGN_LEFT_BOTTOM = 0x6,
	ALIGN_CENTER_BOTTOM = 0x7,
	ALIGN_RIGHT_BOTTOM = 0x8,
}

enum cg_entrenderfx_e
{
	RF_MINLIGHT = 0x1,
	RF_FULLBRIGHT = 0x2,
	RF_FRAMELERP = 0x4,
	RF_NOSHADOW = 0x8,
	RF_VIEWERMODEL = 0x10,
	RF_WEAPONMODEL = 0x20,
	RF_CULLHACK = 0x40,
	RF_FORCENOLOD = 0x80,
	RF_NOPORTALENTS = 0x100,
	RF_ALPHAHACK = 0x200,
	RF_GREYSCALE = 0x400,
	RF_NODEPTHTEST = 0x800,
	RF_NOCOLORWRITE = 0x1000,
}

enum cg_entreftype_e
{
	RT_MODEL = 0x0,
	RT_SPRITE = 0x1,
	RT_PORTALSURFACE = 0x2,
}

/**
 * Global properties
 */
PlayerState@ PredictedPlayerState;
Snapshot@ Snap;
Snapshot@ OldSnap;

/**
 * Global functions
 */
void Print(const String&in) {}
void Error(const String&in) {}
int get_ExtrapolationTime() {}
int get_SnapFrameTime() {}
void AddEntityToSolidList(int number) {}
void AddEntityToTriggerList(int number) {}
ModelHandle@ RegisterModel(const String&in) {}
SoundHandle@ RegisterSound(const String&in) {}
ShaderHandle@ RegisterShader(const String&in) {}
FontHandle@ RegisterFont(const String&in, int style, uint size) {}
SkinHandle@ RegisterSkin(const String&in) {}
PlayerModel@ RegisterPlayerModel(const String&in) {}
WeaponModel@ LoadWeaponModel(const String&in) {}
ModelSkeleton@ SkeletonForModel(ModelHandle@) {}
bool IsViewerEntity(int entNum) {}
String@ GetConfigString(int entNum) {}
Vec3 PredictionError() {}
bool IsPureFile(const String&in) {}
/**
 * ModelHandle
 */
class ModelHandle
{
}

/**
 * SoundHandle
 */
class SoundHandle
{
}

/**
 * ShaderHandle
 */
class ShaderHandle
{
}

/**
 * FontHandle
 */
class FontHandle
{
}

/**
 * Snapshot
 */
class Snapshot
{
	/* properties */
	bool valid;
	int64 serverFrame;
	int64 serverTime;
	int64 ucmdExecuted;
	bool delta;
	bool allentities;
	bool multipov;
	int64 deltaFrameNum;
	int numPlayers;
	PlayerState@ playerState;
	int numEntities;

	/* methods */
	EntityState@ getEntityState(int index) const {}

}

/**
 * ModelSkeleton
 */
class ModelSkeleton
{
}

/**
 * SkinHandle
 */
class SkinHandle
{
}

/**
 * PlayerModel
 */
class PlayerModel
{
	/* methods */
	uint get_numAnims() const {}
	void getAnim(uint index, int&out first, int&out last, int&out loop, int&out fps) const {}
	ModelHandle@ get_model() const {}
	int getRootAnim(const String&in name) const {}
	int[]@ getRotators(const String&in name) const {}

}

/**
 * WeaponModel
 */
class WeaponModel
{
	/* behaviors */

	/* methods */
	uint get_numAnims() const {}
	void getAnim(uint index, int&out first, int&out last, int&out loop, int&out fps) const {}
	uint getNumInfoLines(const String&in name) const {}
	String@[]@ getInfoLine(const String&in name, uint index) const {}

}


}


namespace FilePath
{

/**
 * Global functions
 */
String@ StripExtension(const String&in) {}

}


namespace GS
{

namespace Info
{

/**
 * Global functions
 */
bool Validate(const String&in info) {}
String@ ValueForKey(const String&in info, const String&in key) {}

}


/**
 * Global properties
 */
GameState gameState;
int maxClients;

/**
 * Global functions
 */
void Print(const String&in) {}
void Error(const String&in) {}
int PointContents(const Vec3&in) {}
int PointContents4D(const Vec3&in, int timeDelta) {}
void PredictedEvent(int entityNumber, int event, int param) {}
void RoundUpToHullSize(const Vec3&in inmins, const Vec3&in inmaxs, Vec3&out mins, Vec3&out maxs) {}
Vec3 ClipVelocity(const Vec3&in, const Vec3&in, float overbounce) {}
void GetPlayerStandSize(Vec3&out, Vec3&out) {}
void GetPlayerCrouchSize(Vec3&out, Vec3&out) {}
void GetPlayerGibSize(Vec3&out, Vec3&out) {}
float GetPlayerStandViewHeight() {}
float GetPlayerCrouchHeight() {}
float GetPlayerGibHeight() {}
EntityState@ GetEntityState(int number, int deltaTime = 0) {}
const Firedef@ FiredefForPlayerState(const PlayerState@ state, int checkWeapon) {}
int DirToByte(const Vec3&in) {}
bool IsEventEntity(const EntityState@) {}
bool IsBrushModel(int modelindex) {}
CModelHandle@ InlineModel(int modNum) {}
void InlineModelBounds(CModelHandle@ handle, Vec3&out, Vec3&out) {}
Item@ FindItemByTag(int tag) {}
Item@ FindItemByName(const String&in) {}
Item@ FindItemByClassname(const String&in) {}

}


namespace StringUtils
{

/**
 * Global functions
 */
String@ FormatInt(int64 val, const String&in options, uint width = 0) {}
String@ FormatFloat(double val, const String&in options, uint width = 0, uint precision = 0) {}
String@ Format(const String&in format, const String&in arg1) {}
String@ Format(const String&in format, const String&in arg1, const String&in arg2) {}
String@ Format(const String&in format, const String&in arg1, const String&in arg2, const String&in arg3) {}
String@ Format(const String&in format, const String&in arg1, const String&in arg2, const String&in arg3, const String&in arg4) {}
String@ Format(const String&in format, const String&in arg1, const String&in arg2, const String&in arg3, const String&in arg4, const String&in arg5) {}
String@ Format(const String&in format, const String&in arg1, const String&in arg2, const String&in arg3, const String&in arg4, const String&in arg5, const String&in arg6) {}
String@ Format(const String&in format, const String&in arg1, const String&in arg2, const String&in arg3, const String&in arg4, const String&in arg5, const String&in arg6, const String&in arg7) {}
String@ Format(const String&in format, const String&in arg1, const String&in arg2, const String&in arg3, const String&in arg4, const String&in arg5, const String&in arg6, const String&in arg7, const String&in arg8) {}
String@[]@ Split(const String&in string, const String&in delimiter) {}
String@ Join(String@[]&in, const String&in delimiter) {}
uint Strtol(const String&in string, uint base) {}
String@ FromCharCode(uint charCode) {}
String@ FromCharCode(uint[]&in charCodes) {}

}


/**
 * Enums
 */

enum eCvarFlag
{
	CVAR_ARCHIVE = 0x1,
	CVAR_USERINFO = 0x2,
	CVAR_SERVERINFO = 0x4,
	CVAR_NOSET = 0x8,
	CVAR_LATCH = 0x10,
	CVAR_LATCH_VIDEO = 0x20,
	CVAR_LATCH_SOUND = 0x40,
	CVAR_CHEAT = 0x80,
	CVAR_READONLY = 0x100,
}

enum configstrings_e
{
	CS_MODMANIFEST = 0x3,
	CS_MESSAGE = 0x5,
	CS_MAPNAME = 0x6,
	CS_AUDIOTRACK = 0x7,
	CS_HOSTNAME = 0x0,
	CS_SKYBOX = 0x8,
	CS_STATNUMS = 0x9,
	CS_POWERUPEFFECTS = 0xa,
	CS_GAMETYPETITLE = 0xb,
	CS_GAMETYPENAME = 0xc,
	CS_GAMETYPEVERSION = 0xd,
	CS_GAMETYPEAUTHOR = 0xe,
	CS_AUTORECORDSTATE = 0xf,
	CS_SCB_PLAYERTAB_LAYOUT = 0x10,
	CS_SCB_PLAYERTAB_TITLES = 0x11,
	CS_TEAM_ALPHA_NAME = 0x14,
	CS_TEAM_BETA_NAME = 0x15,
	CS_MAXCLIENTS = 0x2,
	CS_MAPCHECKSUM = 0x1f,
	CS_MATCHNAME = 0x16,
	CS_MATCHSCORE = 0x17,
	CS_ACTIVE_CALLVOTE = 0x19,
	CS_MODELS = 0x20,
	CS_SOUNDS = 0x420,
	CS_IMAGES = 0x820,
	CS_SKINFILES = 0x920,
	CS_LIGHTS = 0xa20,
	CS_ITEMS = 0xb20,
	CS_PLAYERINFOS = 0xb60,
	CS_GAMECOMMANDS = 0xc60,
	CS_LOCATIONS = 0xd60,
	CS_GENERAL = 0xea0,
}

enum state_effects_e
{
	EF_ROTATE_AND_BOB = 0x1,
	EF_SHELL = 0x2,
	EF_STRONG_WEAPON = 0x4,
	EF_QUAD = 0x8,
	EF_CARRIER = 0x10,
	EF_BUSYICON = 0x20,
	EF_FLAG_TRAIL = 0x40,
	EF_TAKEDAMAGE = 0x80,
	EF_TEAMCOLOR_TRANSITION = 0x100,
	EF_EXPIRING_QUAD = 0x200,
	EF_EXPIRING_SHELL = 0x400,
	EF_GODMODE = 0x800,
	EF_REGEN = 0x1000,
	EF_EXPIRING_REGEN = 0x2000,
	EF_GHOST = 0x4000,
	EF_NOPORTALENTS = 0x10,
	EF_PLAYER_STUNNED = 0x1,
	EF_PLAYER_HIDENAME = 0x100,
	EF_AMMOBOX = 0x10000,
	EF_RACEGHOST = 0x20000,
	EF_OUTLINE = 0x40000,
	EF_GHOSTITEM = 0x80000,
}

enum matchstates_e
{
	MATCH_STATE_WARMUP = 0x1,
	MATCH_STATE_COUNTDOWN = 0x2,
	MATCH_STATE_PLAYTIME = 0x3,
	MATCH_STATE_POSTMATCH = 0x4,
	MATCH_STATE_WAITEXIT = 0x5,
}

enum gamestats_e
{
	GAMESTAT_FLAGS = 0x0,
	GAMESTAT_MATCHSTATE = 0x1,
	GAMESTAT_MATCHSTART = 0x2,
	GAMESTAT_MATCHDURATION = 0x3,
	GAMESTAT_CLOCKOVERRIDE = 0x4,
	GAMESTAT_MAXPLAYERSINTEAM = 0x5,
	GAMESTAT_COLORCORRECTION = 0x6,
}

enum gamestatflags_e
{
	GAMESTAT_FLAG_PAUSED = 0x1,
	GAMESTAT_FLAG_WAITING = 0x2,
	GAMESTAT_FLAG_INSTAGIB = 0x4,
	GAMESTAT_FLAG_MATCHEXTENDED = 0x8,
	GAMESTAT_FLAG_FALLDAMAGE = 0x10,
	GAMESTAT_FLAG_HASCHALLENGERS = 0x20,
	GAMESTAT_FLAG_INHIBITSHOOTING = 0x40,
	GAMESTAT_FLAG_ISTEAMBASED = 0x80,
	GAMESTAT_FLAG_ISRACE = 0x100,
	GAMESTAT_FLAG_COUNTDOWN = 0x200,
	GAMESTAT_FLAG_SELFDAMAGE = 0x400,
	GAMESTAT_FLAG_INFINITEAMMO = 0x800,
	GAMESTAT_FLAG_CANFORCEMODELS = 0x1000,
	GAMESTAT_FLAG_CANSHOWMINIMAP = 0x2000,
	GAMESTAT_FLAG_TEAMONLYMINIMAP = 0x4000,
	GAMESTAT_FLAG_MMCOMPATIBLE = 0x8000,
	GAMESTAT_FLAG_ISTUTORIAL = 0x10000,
	GAMESTAT_FLAG_CANDROPWEAPON = 0x20000,
}

enum teams_e
{
	TEAM_SPECTATOR = 0x0,
	TEAM_PLAYERS = 0x2,
	TEAM_ALPHA = 0x3,
	TEAM_BETA = 0x4,
	GS_MAX_TEAMS = 0x5,
}

enum entitytype_e
{
	ET_GENERIC = 0x0,
	ET_PLAYER = 0x1,
	ET_CORPSE = 0x2,
	ET_BEAM = 0x3,
	ET_PORTALSURFACE = 0x4,
	ET_PUSH_TRIGGER = 0x5,
	ET_GIB = 0x6,
	ET_BLASTER = 0x7,
	ET_ELECTRO_WEAK = 0x8,
	ET_ROCKET = 0x9,
	ET_GRENADE = 0xa,
	ET_PLASMA = 0xb,
	ET_SPRITE = 0xc,
	ET_ITEM = 0xd,
	ET_LASERBEAM = 0xe,
	ET_CURVELASERBEAM = 0xf,
	ET_FLAG_BASE = 0x10,
	ET_MINIMAP_ICON = 0x11,
	ET_DECAL = 0x12,
	ET_ITEM_TIMER = 0x13,
	ET_PARTICLES = 0x14,
	ET_SPAWN_INDICATOR = 0x15,
	ET_RADAR = 0x17,
	ET_MONSTER_PLAYER = 0x18,
	ET_MONSTER_CORPSE = 0x19,
	ET_EVENT = 0x60,
	ET_SOUNDEVENT = 0x61,
}

enum entityevent_e
{
	EV_NONE = 0x0,
	EV_WEAPONACTIVATE = 0x1,
	EV_FIREWEAPON = 0x2,
	EV_ELECTROTRAIL = 0x3,
	EV_INSTATRAIL = 0x4,
	EV_FIRE_RIOTGUN = 0x5,
	EV_FIRE_BULLET = 0x6,
	EV_SMOOTHREFIREWEAPON = 0x7,
	EV_NOAMMOCLICK = 0x8,
	EV_DASH = 0x9,
	EV_WALLJUMP = 0xa,
	EV_WALLJUMP_FAILED = 0xb,
	EV_DOUBLEJUMP = 0xc,
	EV_JUMP = 0xd,
	EV_JUMP_PAD = 0xe,
	EV_FALL = 0xf,
	EV_WEAPONDROP = 0x20,
	EV_ITEM_RESPAWN = 0x21,
	EV_PAIN = 0x22,
	EV_DIE = 0x23,
	EV_GIB = 0x24,
	EV_PLAYER_RESPAWN = 0x25,
	EV_PLAYER_TELEPORT_IN = 0x26,
	EV_PLAYER_TELEPORT_OUT = 0x27,
	EV_GESTURE = 0x28,
	EV_DROP = 0x29,
	EV_SPOG = 0x2a,
	EV_BLOOD = 0x2b,
	EV_BLADE_IMPACT = 0x2c,
	EV_GUNBLADEBLAST_IMPACT = 0x2d,
	EV_GRENADE_BOUNCE = 0x2e,
	EV_GRENADE_EXPLOSION = 0x2f,
	EV_ROCKET_EXPLOSION = 0x30,
	EV_PLASMA_EXPLOSION = 0x31,
	EV_BOLT_EXPLOSION = 0x32,
	EV_INSTA_EXPLOSION = 0x33,
	EV_FREE2 = 0x34,
	EV_FREE3 = 0x35,
	EV_FREE4 = 0x36,
	EV_EXPLOSION1 = 0x37,
	EV_EXPLOSION2 = 0x38,
	EV_BLASTER = 0x39,
	EV_SPARKS = 0x3a,
	EV_BULLET_SPARKS = 0x3b,
	EV_SEXEDSOUND = 0x3c,
	EV_VSAY = 0x3d,
	EV_LASER_SPARKS = 0x3e,
	EV_FIRE_SHOTGUN = 0x3f,
	EV_PNODE = 0x40,
	EV_GREEN_LASER = 0x41,
	EV_PLAT_HIT_TOP = 0x42,
	EV_PLAT_HIT_BOTTOM = 0x43,
	EV_PLAT_START_MOVING = 0x44,
	EV_DOOR_HIT_TOP = 0x45,
	EV_DOOR_HIT_BOTTOM = 0x46,
	EV_DOOR_START_MOVING = 0x47,
	EV_BUTTON_FIRE = 0x48,
	EV_TRAIN_STOP = 0x49,
	EV_TRAIN_START = 0x4a,
}

enum solid_e
{
	SOLID_NOT = 0x0,
	SOLID_TRIGGER = 0x1,
	SOLID_YES = 0x2,
	SOLID_BMODEL = 0x1f,
}

enum pmovestats_e
{
	PM_STAT_FEATURES = 0x0,
	PM_STAT_NOUSERCONTROL = 0x1,
	PM_STAT_KNOCKBACK = 0x2,
	PM_STAT_CROUCHTIME = 0x3,
	PM_STAT_ZOOMTIME = 0x4,
	PM_STAT_DASHTIME = 0x5,
	PM_STAT_WJTIME = 0x6,
	PM_STAT_NOAUTOATTACK = 0x7,
	PM_STAT_STUN = 0x8,
	PM_STAT_MAXSPEED = 0x9,
	PM_STAT_JUMPSPEED = 0xa,
	PM_STAT_DASHSPEED = 0xb,
	PM_STAT_FWDTIME = 0xc,
	PM_STAT_CROUCHSLIDETIME = 0xd,
	PM_STAT_SIZE = 0x20,
}

enum pmovefeats_e
{
	PMFEAT_CROUCH = 0x1,
	PMFEAT_WALK = 0x2,
	PMFEAT_JUMP = 0x4,
	PMFEAT_DASH = 0x8,
	PMFEAT_WALLJUMP = 0x10,
	PMFEAT_FWDBUNNY = 0x20,
	PMFEAT_AIRCONTROL = 0x40,
	PMFEAT_ZOOM = 0x80,
	PMFEAT_GHOSTMOVE = 0x100,
	PMFEAT_CONTINOUSJUMP = 0x200,
	PMFEAT_ITEMPICK = 0x400,
	PMFEAT_GUNBLADEAUTOATTACK = 0x800,
	PMFEAT_WEAPONSWITCH = 0x1000,
	PMFEAT_CORNERSKIMMING = 0x2000,
	PMFEAT_CROUCHSLIDING = 0x4000,
	PMFEAT_ALL = 0xffff,
	PMFEAT_DEFAULT = 0x9eff,
}

enum pmovetype_e
{
	PM_NORMAL = 0x0,
	PM_SPECTATOR = 0x1,
	PM_GIB = 0x2,
	PM_FREEZE = 0x3,
	PM_CHASECAM = 0x4,
}

enum pmoveflag_e
{
	PMF_WALLJUMPCOUNT = 0x1,
	PMF_JUMP_HELD = 0x2,
	PMF_ON_GROUND = 0x4,
	PMF_TIME_WATERJUMP = 0x8,
	PMF_TIME_LAND = 0x10,
	PMF_TIME_TELEPORT = 0x20,
	PMF_NO_PREDICTION = 0x40,
	PMF_DASHING = 0x80,
	PMF_SPECIAL_HELD = 0x100,
	PMF_WALLJUMPING = 0x200,
	PMF_DOUBLEJUMPED = 0x400,
	PMF_JUMPPAD_TIME = 0x800,
	PMF_CROUCH_SLIDING = 0x1000,
}

enum itemtype_e
{
	IT_WEAPON = 0x1,
	IT_AMMO = 0x2,
	IT_ARMOR = 0x4,
	IT_POWERUP = 0x8,
	IT_HEALTH = 0x40,
}

enum weapon_tag_e
{
	WEAP_NONE = 0x0,
	WEAP_GUNBLADE = 0x1,
	WEAP_MACHINEGUN = 0x2,
	WEAP_RIOTGUN = 0x3,
	WEAP_GRENADELAUNCHER = 0x4,
	WEAP_ROCKETLAUNCHER = 0x5,
	WEAP_PLASMAGUN = 0x6,
	WEAP_LASERGUN = 0x7,
	WEAP_ELECTROBOLT = 0x8,
	WEAP_INSTAGUN = 0x9,
	WEAP_TOTAL = 0xa,
}

enum ammo_tag_e
{
	AMMO_NONE = 0x0,
	AMMO_GUNBLADE = 0xa,
	AMMO_BULLETS = 0xb,
	AMMO_SHELLS = 0xc,
	AMMO_GRENADES = 0xd,
	AMMO_ROCKETS = 0xe,
	AMMO_PLASMA = 0xf,
	AMMO_LASERS = 0x10,
	AMMO_BOLTS = 0x11,
	AMMO_INSTAS = 0x12,
	AMMO_WEAK_GUNBLADE = 0x13,
	AMMO_WEAK_BULLETS = 0x14,
	AMMO_WEAK_SHELLS = 0x15,
	AMMO_WEAK_GRENADES = 0x16,
	AMMO_WEAK_ROCKETS = 0x17,
	AMMO_WEAK_PLASMA = 0x18,
	AMMO_WEAK_LASERS = 0x19,
	AMMO_WEAK_BOLTS = 0x1a,
	AMMO_WEAK_INSTAS = 0x1b,
	AMMO_TOTAL = 0x1c,
}

enum armor_tag_e
{
	ARMOR_NONE = 0x0,
	ARMOR_GA = 0x1c,
	ARMOR_YA = 0x1d,
	ARMOR_RA = 0x1e,
	ARMOR_SHARD = 0x1f,
}

enum health_tag_e
{
	HEALTH_NONE = 0x0,
	HEALTH_SMALL = 0x20,
	HEALTH_MEDIUM = 0x21,
	HEALTH_LARGE = 0x22,
	HEALTH_MEGA = 0x23,
	HEALTH_ULTRA = 0x24,
}

enum powerup_tag_e
{
	POWERUP_NONE = 0x0,
	POWERUP_QUAD = 0x25,
	POWERUP_SHELL = 0x26,
	POWERUP_REGEN = 0x27,
	POWERUP_TOTAL = 0x28,
}

enum otheritems_tag_e
{
	AMMO_PACK_WEAK = 0x28,
	AMMO_PACK_STRONG = 0x29,
	AMMO_PACK = 0x2a,
}

enum client_statest_e
{
	CS_FREE = 0x0,
	CS_ZOMBIE = 0x1,
	CS_CONNECTING = 0x2,
	CS_CONNECTED = 0x3,
	CS_SPAWNED = 0x4,
}

enum sound_channels_e
{
	CHAN_AUTO = 0x0,
	CHAN_PAIN = 0x1,
	CHAN_VOICE = 0x2,
	CHAN_ITEM = 0x3,
	CHAN_BODY = 0x4,
	CHAN_MUZZLEFLASH = 0x5,
	CHAN_FIXED = 0x80,
}

enum contents_e
{
	CONTENTS_SOLID = 0x1,
	CONTENTS_LAVA = 0x8,
	CONTENTS_SLIME = 0x10,
	CONTENTS_WATER = 0x20,
	CONTENTS_FOG = 0x40,
	CONTENTS_AREAPORTAL = 0x8000,
	CONTENTS_PLAYERCLIP = 0x10000,
	CONTENTS_MONSTERCLIP = 0x20000,
	CONTENTS_TELEPORTER = 0x40000,
	CONTENTS_JUMPPAD = 0x80000,
	CONTENTS_CLUSTERPORTAL = 0x100000,
	CONTENTS_DONOTENTER = 0x200000,
	CONTENTS_ORIGIN = 0x1000000,
	CONTENTS_BODY = 0x2000000,
	CONTENTS_CORPSE = 0x4000000,
	CONTENTS_DETAIL = 0x8000000,
	CONTENTS_STRUCTURAL = 0x10000000,
	CONTENTS_TRANSLUCENT = 0x20000000,
	CONTENTS_TRIGGER = 0x40000000,
	CONTENTS_NODROP = 0x80000000,
	MASK_ALL = 0xffffffff,
	MASK_SOLID = 0x1,
	MASK_PLAYERSOLID = 0x2010001,
	MASK_DEADSOLID = 0x10001,
	MASK_MONSTERSOLID = 0x2020001,
	MASK_WATER = 0x38,
	MASK_OPAQUE = 0x19,
	MASK_SHOT = 0x6000001,
}

enum surfaceflags_e
{
	SURF_NODAMAGE = 0x1,
	SURF_SLICK = 0x2,
	SURF_SKY = 0x4,
	SURF_LADDER = 0x8,
	SURF_NOIMPACT = 0x10,
	SURF_NOMARKS = 0x20,
	SURF_FLESH = 0x40,
	SURF_NODRAW = 0x80,
	SURF_HINT = 0x100,
	SURF_SKIP = 0x200,
	SURF_NOLIGHTMAP = 0x400,
	SURF_POINTLIGHT = 0x800,
	SURF_METALSTEPS = 0x1000,
	SURF_NOSTEPS = 0x2000,
	SURF_NONSOLID = 0x4000,
	SURF_LIGHTFILTER = 0x8000,
	SURF_ALPHASHADOW = 0x10000,
	SURF_NODLIGHT = 0x20000,
	SURF_DUST = 0x40000,
	SURF_NOWALLJUMP = 0x80000,
}

enum serverflags_e
{
	SVF_NOCLIENT = 0x1,
	SVF_PORTAL = 0x2,
	SVF_TRANSMITORIGIN2 = 0x8,
	SVF_SOUNDCULL = 0x10,
	SVF_FAKECLIENT = 0x20,
	SVF_BROADCAST = 0x40,
	SVF_CORPSE = 0x80,
	SVF_PROJECTILE = 0x100,
	SVF_ONLYTEAM = 0x200,
	SVF_FORCEOWNER = 0x400,
	SVF_ONLYOWNER = 0x800,
	SVF_FORCETEAM = 0x1000,
}

enum meaningsofdeath_e
{
	MOD_GUNBLADE_W = 0x24,
	MOD_GUNBLADE_S = 0x25,
	MOD_MACHINEGUN_W = 0x26,
	MOD_MACHINEGUN_S = 0x27,
	MOD_RIOTGUN_W = 0x28,
	MOD_RIOTGUN_S = 0x29,
	MOD_GRENADE_W = 0x2a,
	MOD_GRENADE_S = 0x2b,
	MOD_ROCKET_W = 0x2c,
	MOD_ROCKET_S = 0x2d,
	MOD_PLASMA_W = 0x2e,
	MOD_PLASMA_S = 0x2f,
	MOD_ELECTROBOLT_W = 0x30,
	MOD_ELECTROBOLT_S = 0x31,
	MOD_INSTAGUN_W = 0x32,
	MOD_INSTAGUN_S = 0x33,
	MOD_LASERGUN_W = 0x34,
	MOD_LASERGUN_S = 0x35,
	MOD_GRENADE_SPLASH_W = 0x36,
	MOD_GRENADE_SPLASH_S = 0x37,
	MOD_ROCKET_SPLASH_W = 0x38,
	MOD_ROCKET_SPLASH_S = 0x39,
	MOD_PLASMA_SPLASH_W = 0x3a,
	MOD_PLASMA_SPLASH_S = 0x3b,
	MOD_WATER = 0x3c,
	MOD_SLIME = 0x3d,
	MOD_LAVA = 0x3e,
	MOD_CRUSH = 0x3f,
	MOD_TELEFRAG = 0x40,
	MOD_FALLING = 0x41,
	MOD_SUICIDE = 0x42,
	MOD_EXPLOSIVE = 0x43,
	MOD_BARREL = 0x44,
	MOD_BOMB = 0x45,
	MOD_EXIT = 0x46,
	MOD_SPLASH = 0x47,
	MOD_TARGET_LASER = 0x48,
	MOD_TRIGGER_HURT = 0x49,
	MOD_HIT = 0x4a,
}

enum keyicon_e
{
	KEYICON_FORWARD = 0x0,
	KEYICON_BACKWARD = 0x1,
	KEYICON_LEFT = 0x2,
	KEYICON_RIGHT = 0x3,
	KEYICON_FIRE = 0x4,
	KEYICON_JUMP = 0x5,
	KEYICON_CROUCH = 0x6,
	KEYICON_SPECIAL = 0x7,
	KEYICON_TOTAL = 0x8,
}

enum axis_e
{
	PITCH = 0x0,
	YAW = 0x1,
	ROLL = 0x2,
}

enum button_e
{
	BUTTON_NONE = 0x0,
	BUTTON_ATTACK = 0x1,
	BUTTON_WALK = 0x2,
	BUTTON_SPECIAL = 0x4,
	BUTTON_USE = 0x8,
	BUTTON_ZOOM = 0x10,
	BUTTON_BUSYICON = 0x20,
	BUTTON_ANY = 0x80,
}

enum slidemoveflags_e
{
	SLIDEMOVEFLAG_MOVED = 0x1,
	SLIDEMOVEFLAG_BLOCKED = 0x2,
	SLIDEMOVEFLAG_TRAPPED = 0x4,
	SLIDEMOVEFLAG_WALL_BLOCKED = 0x8,
	SLIDEMOVEFLAG_PLANE_TOUCHED = 0x10,
}

enum stat_e
{
	STAT_LAYOUTS = 0x0,
	STAT_HEALTH = 0x1,
	STAT_ARMOR = 0x2,
	STAT_WEAPON = 0x3,
	STAT_WEAPON_TIME = 0x4,
	STAT_PENDING_WEAPON = 0x5,
	STAT_PICKUP_ITEM = 0x6,
	STAT_SCORE = 0x7,
	STAT_TEAM = 0x8,
	STAT_REALTEAM = 0x9,
	STAT_NEXT_RESPAWN = 0xa,
	STAT_POINTED_PLAYER = 0xb,
	STAT_POINTED_TEAMPLAYER = 0xc,
	STAT_TEAM_ALPHA_SCORE = 0xd,
	STAT_TEAM_BETA_SCORE = 0xe,
	STAT_LAST_KILLER = 0xf,
	STAT_PROGRESS_SELF = 0x20,
	STAT_PROGRESS_OTHER = 0x21,
	STAT_PROGRESS_ALPHA = 0x22,
	STAT_PROGRESS_BETA = 0x23,
	STAT_IMAGE_SELF = 0x24,
	STAT_IMAGE_OTHER = 0x25,
	STAT_IMAGE_ALPHA = 0x26,
	STAT_IMAGE_BETA = 0x27,
	STAT_TIME_SELF = 0x28,
	STAT_TIME_BEST = 0x29,
	STAT_TIME_RECORD = 0x2a,
	STAT_TIME_ALPHA = 0x2b,
	STAT_TIME_BETA = 0x2c,
	STAT_MESSAGE_SELF = 0x2d,
	STAT_MESSAGE_OTHER = 0x2e,
	STAT_MESSAGE_ALPHA = 0x2f,
	STAT_MESSAGE_BETA = 0x30,
	STAT_IMAGE_CLASSACTION1 = 0x31,
	STAT_IMAGE_CLASSACTION2 = 0x32,
	STAT_IMAGE_DROP_ITEM = 0x33,
}

/**
 * Global properties
 */
const int PS_MAX_STATS;
const int MAX_GAME_STATS;
const int MAX_EVENTS;
const int MAX_TOUCHENTS;
const float BASEGRAVITY;
const float GRAVITY;
const float GRAVITY_COMPENSATE;
const int ZOOMTIME;
const float STEPSIZE;
const float SLIDEMOVE_PLANEINTERACT_EPSILON;
const float PROJECTILE_PRESTEP;
const int MAX_CLIENTS;
const int MAX_EDICTS;
const int MAX_LIGHTSTYLES;
const int MAX_MODELS;
const int MAX_IMAGES;
const int MAX_SKINFILES;
const int MAX_ITEMS;
const int MAX_SOUNDS;
const int MAX_GENERAL;
const int MAX_MMPLAYERINFOS;
const int MAX_CONFIGSTRINGS;
const int MAX_TEAMS;
const int PREDICTABLE_EVENTS_MAX;

/**
 * Global functions
 */
int abs(int x) {}
double abs(double x) {}
double log(double x) {}
double pow(double x, double y) {}
double cos(double x) {}
double sin(double x) {}
double tan(double x) {}
double acos(double x) {}
double asin(double x) {}
double atan(double x) {}
double atan2(double x, double y) {}
double sqrt(double x) {}
double ceil(double x) {}
double max(double x, double y) {}
double min(double x, double y) {}
double max(int64 x, int64 y) {}
double min(int64 x, int64 y) {}
double max(uint64 x, uint64 y) {}
double min(uint64 x, uint64 y) {}
double floor(double x) {}
double random() {}
double brandom(double min, double max) {}
double deg2rad(double deg) {}
double rad2deg(double rad) {}
int rand() {}
Vec3 RotatePointAroundVector(const Vec3&in dir, const Vec3&in point, float degrees) {}
float AngleSubtract(float v1, float v2) {}
Vec3 AnglesSubtract(const Vec3&in a1, const Vec3&in a2) {}
float AngleNormalize360(float a) {}
float AngleNormalize180(float a) {}
float AngleDelta(float a1, float a2) {}
float anglemod(float a) {}
float LerpAngle(float v1, float v2, float lerp) {}
Vec3 LerpAngles(const Vec3&in a1, const Vec3&in a2, float f) {}
/**
 * array
 */
class array
{
	/* behaviors */
	T[]@ _beh_4_(int&in, int&in) { repeat T } {}

	/* factories */
	T[]@ array(int&in) {}
	T[]@ array(int&in, uint) {}
	T[]@ array(int&in, uint, const T&in) {}

	/* methods */
	T& opIndex(uint) {}
	const T& opIndex(uint) const {}
	T[]& opAssign(const T[]&in) {}
	void insertAt(uint, const T&in) {}
	void removeAt(uint) {}
	void insertLast(const T&in) {}
	void removeLast() {}
	uint length() const {}
	void reserve(uint) {}
	void resize(uint) {}
	void sortAsc() {}
	void sortAsc(uint, uint) {}
	void sortDesc() {}
	void sortDesc(uint, uint) {}
	void reverse() {}
	int find(const T&in) const {}
	int find(uint, const T&in) const {}
	bool opEquals(const T[]&in) const {}
	bool isEmpty() const {}
	uint get_length() const {}
	void set_length(uint) {}
	uint size() const {}
	bool empty() const {}
	void push_back(const T&in) {}
	void pop_back() {}
	void insert(uint, const T&in) {}
	void erase(uint) {}

}

/**
 * String
 */
class String
{
	/* behaviors */

	/* factories */
	String@ String() {}
	String@ String(const String&in) {}
	String@ String(int) {}
	String@ String(float) {}
	String@ String(double) {}

	/* methods */
	int opImplConv() {}
	float opImplConv() {}
	double opImplConv() {}
	String& opAssign(const String&in) {}
	String& opAssign(int) {}
	String& opAssign(double) {}
	String& opAssign(float) {}
	uint8& opIndex(uint) {}
	const uint8& opIndex(uint) const {}
	String& opAddAssign(const String&in) {}
	String& opAddAssign(int) {}
	String& opAddAssign(double) {}
	String& opAddAssign(float) {}
	String@ opAdd(const String&in) const {}
	String@ opAdd(int) const {}
	String@ opAdd_r(int) const {}
	String@ opAdd(double) const {}
	String@ opAdd_r(double) const {}
	String@ opAdd(float) const {}
	String@ opAdd_r(float) const {}
	bool opEquals(const String&in) const {}
	uint len() const {}
	uint length() const {}
	bool empty() const {}
	String@ tolower() const {}
	String@ toupper() const {}
	String@ trim() const {}
	String@ removeColorTokens() const {}
	String@ getToken(const uint) const {}
	int toInt() const {}
	float toFloat() const {}
	uint locate(String&inout, const uint) const {}
	String@ substr(const uint start, const uint length) const {}
	String@ subString(const uint start, const uint length) const {}
	String@ substr(const uint start) const {}
	String@ subString(const uint start) const {}
	String@ replace(const String&in search, const String&in replace) const {}
	bool isAlpha() const {}
	bool isNumerical() const {}
	bool isNumeric() const {}
	bool isAlphaNumerical() const {}

}

/**
 * Dictionary
 */
class Dictionary
{
	/* behaviors */
	Dictionary@ _beh_4_(int&in) { repeat { String, ? } } {}

	/* factories */
	Dictionary@ Dictionary() {}

	/* methods */
	Dictionary& opAssign(const Dictionary&in) {}
	void set(const String&in, ?&in) {}
	bool get(const String&in, ?&out) const {}
	void set(const String&in, int64&in) {}
	bool get(const String&in, int64&out) const {}
	void set(const String&in, double&in) {}
	bool get(const String&in, double&out) const {}
	void set(const String&in, const String&in) {}
	bool get(const String&in, String&out) const {}
	bool exists(const String&in) const {}
	bool isEmpty() const {}
	uint getSize() const {}
	void delete(const String&in) {}
	void deleteAll() {}
	String@[]@ getKeys() const {}
	bool empty() const {}
	uint size() const {}
	void erase(const String&in) {}
	void clear() {}

}

/**
 * Time
 */
class Time
{
	/* properties */
	const int64 time;
	const int sec;
	const int min;
	const int hour;
	const int mday;
	const int mon;
	const int year;
	const int wday;
	const int yday;
	const int isdst;

	/* behaviors */
	Time() {}
	Time(int64) {}
	Time(const Time&in) {}

	/* methods */
	Time& opAssign(const Time&in) {}
	bool opEquals(const Time&in) {}

}

/**
 * any
 */
class any
{
	/* behaviors */

	/* factories */
	any@ any() {}
	any@ any(?&in) {}

	/* methods */
	any& opAssign(any&in) {}
	void store(?&in) {}
	void store(int64&in) {}
	void store(double&in) {}
	bool retrieve(?&out) {}
	bool retrieve(int64&out) {}
	bool retrieve(double&out) {}

}

/**
 * Vec3
 */
class Vec3
{
	/* properties */
	float x;
	float y;
	float z;

	/* behaviors */
	Vec3() {}
	Vec3(float, float, float) {}
	Vec3(const Vec3&in) {}
	Vec3(const float[]&inout) {}

	/* methods */
	Vec3& opAssign(Vec3&in) {}
	Vec3& opAddAssign(Vec3&in) {}
	Vec3& opSubAssign(Vec3&in) {}
	Vec3& opMulAssign(Vec3&in) {}
	Vec3& opXorAssign(Vec3&in) {}
	Vec3& opMulAssign(int) {}
	Vec3& opMulAssign(float) {}
	Vec3 opAdd(Vec3&in) const {}
	Vec3 opSub(Vec3&in) const {}
	float opMul(Vec3&in) const {}
	Vec3 opMul(float) const {}
	Vec3 opMul_r(float) const {}
	Vec3 opMul(int) const {}
	Vec3 opMul_r(int) const {}
	Vec3 opXor(const Vec3&in) const {}
	bool opEquals(const Vec3&in) const {}
	void clear() {}
	void set(float x, float y, float z) {}
	float length() const {}
	float lengthSquared() const {}
	float normalize() {}
	float distance(const Vec3&in) const {}
	void angleVectors(Vec3&out, Vec3&out, Vec3&out) const {}
	Vec3 toAngles() const {}
	Vec3 perpendicular() const {}
	void makeNormalVectors(Vec3&out, Vec3&out) const {}
	void anglesToMarix(Mat3&out) const {}
	void anglesToAxis(Mat3&out) const {}
	float& opIndex(uint) {}
	const float& opIndex(uint) const {}
	float[]@ toArray() const {}
	String@ toString() const {}

}

/**
 * Vec4
 */
class Vec4
{
	/* properties */
	float x;
	float y;
	float z;
	float w;

	/* behaviors */
	Vec4() {}
	Vec4(float, float, float, float) {}
	Vec4(const Vec4&in) {}
	Vec4(const float[]&inout) {}

	/* methods */
	Vec4& opAssign(Vec4&in) {}
	Vec4& opAddAssign(Vec4&in) {}
	Vec4& opSubAssign(Vec4&in) {}
	Vec4& opMulAssign(Vec4&in) {}
	Vec4& opMulAssign(int) {}
	Vec4& opMulAssign(float) {}
	Vec4 opAdd(Vec4&in) const {}
	Vec4 opSub(Vec4&in) const {}
	float opMul(Vec4&in) const {}
	Vec4 opMul(float) const {}
	Vec4 opMul_r(float) const {}
	Vec4 opMul(int) const {}
	Vec4 opMul_r(int) const {}
	bool opEquals(const Vec4&in) const {}
	void clear() {}
	void set(float x, float y, float z, float w) {}
	float length() const {}
	float normalize() {}
	float distance(const Vec4&in) const {}
	Vec3 xyz() const {}
	float& opIndex(uint) {}
	const float& opIndex(uint) const {}
	float[]@ toArray() const {}
	String@ toString() const {}

}

/**
 * Cvar
 */
class Cvar
{
	/* behaviors */
	Cvar(const String&in, const String&in, const uint) {}
	Cvar(const Cvar&in) {}

	/* methods */
	void reset() {}
	void set(const String&in) {}
	void set(float value) {}
	void set(int value) {}
	void set(double value) {}
	void set_modified(bool modified) {}
	bool get_modified() const {}
	bool get_boolean() const {}
	int get_integer() const {}
	float get_value() const {}
	const String@ get_name() const {}
	const String@ get_string() const {}
	const String@ get_defaultString() const {}
	const String@ get_latchedString() const {}

}

/**
 * Mat3
 */
class Mat3
{
	/* properties */
	Vec3 x;
	Vec3 y;
	Vec3 z;

	/* behaviors */
	Mat3() {}
	Mat3(const Vec3&in, const Vec3&in, const Vec3&in) {}
	Mat3(const Mat3&in) {}
	Mat3(const float[]&inout) {}

	/* methods */
	Vec3& opAssign(Vec3&in) {}
	Mat3 opMul(Mat3&in) const {}
	Vec3 opMul(Vec3&in) const {}
	bool opEquals(const Mat3&in) const {}
	void identity() {}
	void normalize() {}
	void transpose() {}
	void toVectors(Vec3&out, Vec3&out, Vec3&out) const {}
	Vec3 toAngles() const {}
	float& opIndex(uint) {}
	const float& opIndex(uint) const {}
	float[]@ toArray() {}

}

/**
 * Trace
 */
class Trace
{
	/* properties */
	const bool allSolid;
	const bool startSolid;
	const float fraction;
	const int surfFlags;
	const int contents;
	const int entNum;
	const float planeDist;
	const int16 planeType;
	const int16 planeSignBits;

	/* behaviors */
	Trace() {}
	Trace(const Trace&in) {}

	/* methods */
	bool doTrace(const Vec3&in, const Vec3&in, const Vec3&in, const Vec3&in, int ignore, int contentMask) const {}
	bool doTrace4D(const Vec3&in, const Vec3&in, const Vec3&in, const Vec3&in, int ignore, int contentMask, int timeDelta) const {}
	Vec3 get_endPos() const {}
	Vec3 get_planeNormal() const {}

}

/**
 * Item
 */
class Item
{
	/* properties */
	const int tag;
	const uint type;
	const int flags;
	const int quantity;
	const int inventoryMax;
	const int ammoTag;
	const int weakAmmoTag;
	const int effects;

	/* methods */
	const String@ get_classname() const {}
	const String@ get_name() const {}
	const String@ get_shortName() const {}
	const String@ get_model() const {}
	const String@ get_model2() const {}
	const String@ get_icon() const {}
	const String@ get_simpleIcon() const {}
	const String@ get_pickupSound() const {}
	const String@ get_colorToken() const {}
	const String@ getWorldModel(uint idx) const {}
	bool isPickable() const {}
	bool isUsable() const {}
	bool isDropable() const {}

}

/**
 * EntityState
 */
class EntityState
{
	/* properties */
	uint svFlags;
	int type;
	int solid;
	int modelindex;
	int modelindex2;
	int bodyOwner;
	int channel;
	int frame;
	int ownerNum;
	uint effects;
	int counterNum;
	int skinNum;
	int itemNum;
	int fireMode;
	int damage;
	int targetNum;
	int colorRGBA;
	int range;
	float attenuation;
	int weapon;
	bool teleported;
	int sound;
	int light;
	int team;
	bool linearMovement;
	uint linearMovementDuration;
	int64 linearMovementTimeStamp;

	/* behaviors */

	/* factories */
	EntityState@ EntityState() {}

	/* methods */
	Vec3& opAssign(const EntityState&in) {}
	int get_number() const {}
	Vec3 get_origin() const {}
	void set_origin(const Vec3&in) {}
	Vec3 get_origin2() const {}
	void set_origin2(const Vec3&in) {}
	Vec3 get_origin3() const {}
	void set_origin3(const Vec3&in) {}
	Vec3 get_angles() const {}
	void set_angles(const Vec3&in) {}
	Vec3 get_linearMovementVelocity() const {}
	void set_linearMovementVelocity(const Vec3&in) {}
	Vec3 get_linearMovementBegin() const {}
	void set_linearMovementBegin(const Vec3&in) {}
	Vec3 get_linearMovementEnd() const {}
	void set_linearMovementEnd(const Vec3&in) {}
	int get_events(uint index) const {}
	void set_events(uint index, int value) {}
	int get_eventParms(uint index) const {}
	void set_eventParms(uint index, int value) {}

}

/**
 * UserCmd
 */
class UserCmd
{
	/* properties */
	int8 msec;
	uint buttons;
	int64 serverTimeStamp;
	int8 forwardmove;
	int8 sidemove;
	int8 upmove;

	/* behaviors */
	UserCmd() {}
	UserCmd(const UserCmd&in) {}

}

/**
 * PMoveState
 */
class PMoveState
{
	/* properties */
	int pm_type;
	int pm_time;
	int gravity;

	/* behaviors */

	/* factories */
	PMoveState@ PMoveState() {}

	/* methods */
	Vec3& opAssign(const PMoveState&in) {}
	Vec3 get_origin() const {}
	void set_origin(const Vec3&in) {}
	Vec3 get_velocity() const {}
	void set_velocity(const Vec3&in) {}
	int16 get_stats(uint index) const {}
	void set_stats(uint index, int16 value) {}
	int get_pm_flags() const {}
	void set_pm_flags(int value) {}

}

/**
 * PlayerState
 */
class PlayerState
{
	/* properties */
	uint POVnum;
	uint playerNum;
	float viewHeight;
	float fov;
	uint plrkeys;
	uint8 weaponState;

	/* methods */
	Vec3 get_viewAngles() const {}
	void set_viewAngles(const Vec3&in) {}
	int get_events(uint index) const {}
	void set_events(uint index, int value) {}
	int get_eventParms(uint index) const {}
	void set_eventParms(uint index, int value) {}
	PMoveState& get_pmove() const {}
	int get_inventory(uint index) const {}
	void set_inventory(uint index, int value) {}
	int16 get_stats(uint index) const {}
	void set_stats(uint index, int16 value) {}

}

/**
 * PMove
 */
class PMove
{
	/* properties */
	bool skipCollision;
	int numTouchEnts;
	float step;
	int groundEntity;
	int groundSurfFlags;
	int groundContents;
	int waterType;
	int waterLevel;
	int contentMask;
	bool ladder;
	float groundPlaneDist;
	int16 groundPlaneType;
	int16 groundPlaneSignBits;
	float remainingTime;
	float slideBounce;
	int passEnt;

	/* methods */
	void getSize(Vec3&out, Vec3&out) {}
	void setSize(const Vec3&in, const Vec3&in) {}
	Vec3 get_groundPlaneNormal() const {}
	void set_groundPlaneNormal(const Vec3&in) {}
	int get_touchEnts(uint index) const {}
	void set_touchEnts(uint index, int entNum) {}
	void touchTriggers(PlayerState@, const Vec3&in prevOrigin) {}
	void addTouchEnt(int entNum) {}
	int slideMove() {}
	Vec3 get_origin() const {}
	void set_origin(const Vec3&in) {}
	Vec3 get_velocity() const {}
	void set_velocity(const Vec3&in) {}

}

/**
 * GameState
 */
class GameState
{
	/* methods */
	int64 get_stats(uint index) const {}
	void set_stats(uint index, int64 value) {}

}

/**
 * Firedef
 */
class Firedef
{
	/* properties */
	int fireMode;
	int ammoID;
	int usagCount;
	int projectileCount;
	uint weaponupTime;
	uint weapondownTime;
	uint reloadTime;
	uint cooldownTime;
	uint timeout;
	bool smoothRefire;
	float damage;
	float selfDamage;
	int knockback;
	int stun;
	int splashRadius;
	int minDamage;
	int minKnockback;
	int speed;
	int spread;
	int vspread;
	int weaponPickup;
	int ammoPickup;
	int ammoMax;
	int ammoLow;

}

/**
 * CModelHandle
 */
class CModelHandle
{
}

