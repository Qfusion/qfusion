/*
Copyright (C) 2023 coolelectronics

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/

#ifndef STEAMSHIM_CHILD_H
#define STEAMSHIM_CHILD_H

#include <stdint.h>
#include "../steamshim_types.h"

typedef struct { 
  bool debug;
  bool runclient;
  bool runserver;
} SteamshimOptions;

typedef struct {
  bool advertise;
  int botplayercount;
  bool dedicatedserver;
  const char *gamedata;
  const char *gamedescription;
  const char *gametags;
  int heartbeatinterval;
  const char *mapname;
  int maxplayercount;
  const char *moddir;
  bool passwordprotected;
  const char *product;
  const char *region;
  const char *servername;
} ServerInfo;

#ifdef __cplusplus
extern "C" {
#endif

// return of 0 means failure
int STEAMSHIM_init(SteamshimOptions* options);
void STEAMSHIM_deinit(void);
#ifdef __cplusplus
}
#endif

#endif

