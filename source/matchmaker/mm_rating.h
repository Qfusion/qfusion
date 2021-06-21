/*
Copyright (C) 1997-2001 Id Software, Inc.

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
#ifndef __MM_RATING_H__
#define __MM_RATING_H__

#ifdef __cplusplus
extern "C" {
#endif

//=============================================
//	rating
//=============================================

// rating info structure and utilities freely usable by all modules

#define MM_RATING_DEFAULT       0.0
#define MM_DEVIATION_DEFAULT    1.0
#define MM_DEVIATION_MIN        0.0     // FIXME
#define MM_DEVIATION_MAX        1.0
#define MM_DEFAULT_T            4.0
#define MM_PROBABILITY_DEFAULT  0.5

typedef struct clientRating_s {
	char gametype[32];
	float rating;
	float deviation;
	int uuid;           // can be used to reference clients by sessionID or playernum etc..
	struct  clientRating_s *next;
} clientRating_t;

// returns the given rating or NULL
clientRating_t *Rating_Find( clientRating_t *ratings, const char *gametype );
clientRating_t *Rating_FindId( clientRating_t *ratings, int id );
// detaches given rating from the list, returns the element and sets the ratings argument
// to point to the new root. Returns NULL if gametype wasn't found
clientRating_t *Rating_Detach( clientRating_t **list, const char *gametype );
clientRating_t *Rating_DetachId( clientRating_t **list, int id );

// returns a value between 0-1 for single clientRating against list of other clientRatings
// if single is on the list, it is ignored for the calculation
float Rating_GetProbability( clientRating_t *single, clientRating_t *list );
// head-on probability
float Rating_GetProbabilitySingle( clientRating_t *single, clientRating_t *other );

// TODO: Teams probability
// TODO: balanced team making
// TODO: find best opponent
// TODO: find best pairs

// create an average clientRating out of list of clientRatings
void Rating_AverageRating( clientRating_t *out, clientRating_t *list );

#ifdef __cplusplus
}
#endif

#endif
