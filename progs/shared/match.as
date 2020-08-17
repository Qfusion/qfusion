namespace GS {

bool Instagib() { return ( GS::gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_INSTAGIB != 0 ); }
bool FallDamage() { return ( GS::gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_FALLDAMAGE != 0 ); }
bool ShootingDisabled() { return ( GS::gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_INHIBITSHOOTING != 0 ); }
bool HasChallengers() { return ( GS::gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_HASCHALLENGERS != 0 ); }
bool TeamBasedGametype() { return ( GS::gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_ISTEAMBASED != 0 ); }
bool RaceGametype() { return ( GS::gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_ISRACE != 0 ); }
bool MatchPaused() { return ( GS::gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_PAUSED != 0 ); }
bool MatchWaiting() { return ( GS::gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_WAITING != 0 ); }
bool MatchExtended() { return ( GS::gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_MATCHEXTENDED != 0 ); }
bool SelfDamage() { return ( GS::gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_SELFDAMAGE != 0 ); }
bool Countdown() { return ( GS::gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_COUNTDOWN != 0 ); }
bool InfiniteAmmo() { return ( GS::gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_INFINITEAMMO != 0 ); }
bool CanForceModels() { return ( GS::gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_CANFORCEMODELS != 0 ); }
bool CanShowMinimap() { return ( GS::gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_CANSHOWMINIMAP != 0 ); }
bool TeamOnlyMinimap() { return ( GS::gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_TEAMONLYMINIMAP != 0 ); }
bool MMCompatible() { return ( GS::gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_MMCOMPATIBLE != 0 ); }
bool TutorialGametype() { return ( GS::gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_ISTUTORIAL != 0 ); }
bool CanDropWeapon() { return ( GS::gameState.stats[GAMESTAT_FLAGS] & GAMESTAT_FLAG_CANDROPWEAPON != 0 ); }

int MatchState() { return GS::gameState.stats[GAMESTAT_MATCHSTATE]; }
int MaxPlayersInTeam() { return GS::gameState.stats[GAMESTAT_MAXPLAYERSINTEAM]; }
bool InvidualGameType() { return MaxPlayersInTeam() == 1; }

int MatchDuration() { return GS::gameState.stats[GAMESTAT_MATCHDURATION]; }
int64 MatchStartTime() { return GS::gameState.stats[GAMESTAT_MATCHSTART]; }
int64 MatchEndTime() { return GS::gameState.stats[GAMESTAT_MATCHDURATION] != 0 ? GS::gameState.stats[GAMESTAT_MATCHSTART] + GS::gameState.stats[GAMESTAT_MATCHDURATION] : 0; }
int64 MatchClockOverride() { return GS::gameState.stats[GAMESTAT_CLOCKOVERRIDE]; }

}
