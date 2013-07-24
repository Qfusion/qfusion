/*
Copyright (C) 2013 Victor Luchits

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

#include "g_local.h"

/*
* G_WebRequest
*
* Route request to appropriate submodule
*/
http_response_code_t G_WebRequest( http_query_method_t method, const char *resource, 
		const char *query_string, char **content, size_t *content_length )
{
	if( !Q_strnicmp( resource, "callvote", 8 ) ) {
		return G_CallVotes_WebRequest( method, resource, query_string, content, content_length );
	}
	return HTTP_RESP_NOT_FOUND;
}
