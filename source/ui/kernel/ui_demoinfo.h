/*
Copyright (C) 2011 Victor Luchits

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

#ifndef __UI_DEMOINFO_H__
#define __UI_DEMOINFO_H__

#include <map>
#include <time.h>
#include "kernel/ui_common.h"

namespace WSWUI
{

typedef std::map<std::string, std::string> DemoMetaData;

class DemoInfo
{
public:
	DemoInfo( void );

	DemoInfo( const char *name );

	/// copy constructor
	DemoInfo( const DemoInfo &other ) { *this = other; }

	/// Assignment operator, required by the AS.
	DemoInfo & operator =( const DemoInfo &other );

	// whether playable at all
	const bool isValid( void ) const;

	// demo name, e.g.: "server/1.wd13"
	void setName( const std::string & name );

	// demo name, e.g.: "server/1.wd13"
	const std::string &getName( void ) const;

	// path to be prepended to the name to get the real file path
	// for meta data (this is actually quite bad, a FIXME even)
	void setDirectory( const std::string &directory );
	const std::string getFullPath( void ) const;

	// only relevant for currently playing demo (single instance)
	void setPlaying( bool playing ) { isPlaying = playing; }
	const bool getPlaying( void ) const { return isPlaying; }

	// only relevant for currently playing demo (single instance)
	void setPaused( bool paused ) { isPaused = paused; }
	const bool getPaused( void ) const { return isPaused; }

	// only relevant for currently playing demo (single instance)
	void setTime( unsigned int time_ ) { time = time_; }
	const unsigned int getTime( void ) const { return time; }

	const DemoMetaData &getMetaData( void );

	// control methods
	void Play( void ) const;
	void Pause( void ) const;
	void Stop( void ) const;
	void Jump( unsigned int time ) const;

private:
	// name, e.g. "server/abc.wd13"
	std::string name;
	std::string directory;

	unsigned int time;
	bool isPlaying, isPaused;
	bool hasMetaData;
	DemoMetaData metaData;
	void readMetaData( void );
};

}

#endif // __UI_DEMOINFO_H__
