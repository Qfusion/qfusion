/*
Copyright (C) 2012 Victor Luchits

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

#ifndef __UI_DOWNLOADINFO_H__
#define __UI_DOWNLOADINFO_H__

#include "kernel/ui_common.h"

namespace WSWUI {

class DownloadInfo
{
public:
	DownloadInfo() : name( "" ), type( 0 ), percent( 0 ), speed( 0 )
	{
	}

	DownloadInfo( const char *name, int type ) : name( name ? name : "" ), type( type ), percent( 0 ), speed( 0 )
	{
	}

	DownloadInfo & operator = ( const DownloadInfo &other )
	{
		name = other.getName();
		type = other.getType();
		percent = other.getPercent();
		speed = other.getSpeed();
		return *this;
	}

	std::string getName( void ) const { return name; }

	int getType( void ) const { return type; }

	float getPercent( void ) const { return percent; }
	void setPercent( const float p ) { this->percent = p; }

	int getSpeed( void ) const { return speed; }
	void setSpeed( const int s ) { this->speed = s; }

private:
	std::string name;
	int type; // server or HTTP
	float percent;
	int speed;
};

}

#endif // __UI_DOWNLOADINFO_H__
