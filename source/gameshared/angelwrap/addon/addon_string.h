/*
Copyright (C) 2008 German Garcia
Copyright (C) 2011 Chasseur de bots

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

#ifndef __ADDON_STRING_H__
#define __ADDON_STRING_H__

asstring_t *objectString_FactoryBuffer( const char *buffer, unsigned int length );
const asstring_t *objectString_ConstFactoryBuffer( const char *buffer, unsigned int length );
void objectString_Release( asstring_t *obj );
asstring_t *objectString_AssignString( asstring_t *self, const char *string, size_t strlen );

void PreRegisterStringAddon( asIScriptEngine *engine );
void RegisterStringAddon( asIScriptEngine *engine );

#endif // __ADDON_STRING_H__
