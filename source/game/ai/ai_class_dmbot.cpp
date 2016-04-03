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

#include "bot.h"

//==========================================
// BOT_DMclass_InitPersistant
// Persistant after respawns.
//==========================================
void BOT_DMclass_InitPersistant( edict_t *self )
{
    gsitem_t *item;
    int i, w;

    self->classname = "dmbot";

    if( self->r.client->netname )
        self->ai->pers.netname = self->r.client->netname;
    else
        self->ai->pers.netname = "dmBot";

    //available moveTypes for this class
    self->ai->pers.moveTypesMask = ( LINK_MOVE|LINK_STAIRS|LINK_FALL|LINK_WATER|LINK_WATERJUMP|LINK_JUMPPAD|LINK_PLATFORM|LINK_TELEPORT|LINK_LADDER|LINK_JUMP|LINK_CROUCH );

    //Persistant Inventory Weights (0 = can not pick)
    memset( self->ai->pers.inventoryWeights, 0, sizeof( self->ai->pers.inventoryWeights ) );

    // weapons
    self->ai->pers.inventoryWeights[WEAP_GUNBLADE] = 0.0f;
    self->ai->pers.inventoryWeights[WEAP_MACHINEGUN] = 0.75f;
    self->ai->pers.inventoryWeights[WEAP_RIOTGUN] = 0.75f;
    self->ai->pers.inventoryWeights[WEAP_GRENADELAUNCHER] = 0.7f;
    self->ai->pers.inventoryWeights[WEAP_ROCKETLAUNCHER] = 0.8f;
    self->ai->pers.inventoryWeights[WEAP_PLASMAGUN] = 0.75f;
    self->ai->pers.inventoryWeights[WEAP_ELECTROBOLT] = 0.8f;
    self->ai->pers.inventoryWeights[WEAP_LASERGUN] = 0.8f;

    // ammo
    self->ai->pers.inventoryWeights[AMMO_WEAK_GUNBLADE] = 0.0f;
    self->ai->pers.inventoryWeights[AMMO_BULLETS] = 0.7f;
    self->ai->pers.inventoryWeights[AMMO_SHELLS] = 0.7f;
    self->ai->pers.inventoryWeights[AMMO_GRENADES] = 0.7f;
    self->ai->pers.inventoryWeights[AMMO_ROCKETS] = 0.7f;
    self->ai->pers.inventoryWeights[AMMO_PLASMA] = 0.7f;
    self->ai->pers.inventoryWeights[AMMO_BOLTS] = 0.7f;
    self->ai->pers.inventoryWeights[AMMO_LASERS] = 0.7f;

    // armor
    self->ai->pers.inventoryWeights[ARMOR_RA] = self->ai->pers.cha.armor_grabber * 2.0f;
    self->ai->pers.inventoryWeights[ARMOR_YA] = self->ai->pers.cha.armor_grabber * 1.0f;
    self->ai->pers.inventoryWeights[ARMOR_GA] = self->ai->pers.cha.armor_grabber * 0.75f;
    self->ai->pers.inventoryWeights[ARMOR_SHARD] = self->ai->pers.cha.armor_grabber * 0.5f;

    // health
    self->ai->pers.inventoryWeights[HEALTH_MEGA] = /*self->ai->pers.cha.health_grabber **/ 2.0f;
    self->ai->pers.inventoryWeights[HEALTH_ULTRA] = /*self->ai->pers.cha.health_grabber **/ 2.0f;
    self->ai->pers.inventoryWeights[HEALTH_LARGE] = /*self->ai->pers.cha.health_grabber **/ 1.0f;
    self->ai->pers.inventoryWeights[HEALTH_MEDIUM] = /*self->ai->pers.cha.health_grabber **/ 0.9f;
    self->ai->pers.inventoryWeights[HEALTH_SMALL] = /*self->ai->pers.cha.health_grabber **/ 0.4f;

    // backpack
    self->ai->pers.inventoryWeights[AMMO_PACK] = 0.4f;

    self->ai->pers.inventoryWeights[POWERUP_QUAD] = self->ai->pers.cha.offensiveness * 2.0f;
    self->ai->pers.inventoryWeights[POWERUP_SHELL] = self->ai->pers.cha.offensiveness * 2.0f;

    // scale the inventoryWeights by the character weapon affinities
    // FIXME: rewrite this loop!
    for( i = 1; i < MAX_ITEMS; i++ )
    {
        if( ( item = GS_FindItemByTag( i ) ) == NULL )
            continue;

        if( item->type & IT_WEAPON )
        {
            self->ai->pers.inventoryWeights[i] *= self->ai->pers.cha.weapon_affinity[ i - ( WEAP_GUNBLADE - 1 ) ];
        }
        else if( item->type & IT_AMMO )
        {
            // find weapon for ammo
            for( w = WEAP_GUNBLADE; w < WEAP_TOTAL; w++ )
            {
                if( GS_FindItemByTag( w )->ammo_tag == item->tag ||
                    GS_FindItemByTag( w )->weakammo_tag == item->tag )
                {
                    self->ai->pers.inventoryWeights[i] *= self->ai->pers.cha.weapon_affinity[ w - ( WEAP_GUNBLADE - 1 ) ];
                    break;
                }
            }
        }
    }
}

