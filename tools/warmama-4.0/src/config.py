#!/usr/bin/env python2.7

'''
Created on 7.2.2011
Warmama
@author: Christian Holmberg 2011

@license:
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
'''

# cgi_mode = 'local'
# cgi_mode = 'wsgi'
cgi_mode = 'fcgi'

logfile_name = '/tmp/warmama.log'
logfile_append = True	# or False

report_dir = '/tmp/warmama-reports'

# database configuration
db_type = 'mysql'
db_host = 'localhost'
db_name = 'warmama'
db_user = 'root'
db_passwd = 'root'
db_engine = ''
db_charset = 'utf8'
db_debug_print = True

# geolocation
geoip_use = True
geoip_path = ''

# URL to send the authentication request
getauth_url = 'http://localhost:6000/getauth'	# local to local
# getauth_url = 'http://forum.picmip.org/mmauth'	# external to external

# this URL is given as a parameter to above request
# it is the URL the auth-server has to respond to
#auth_response_url = 'http://localhost:5000/auth'	# local to local
auth_response_url = 'http://localhost/warmama/auth'	# local to local httpd
# auth_response_url = 'http://kupla.wippies.net/cgi/authresponse/authresponse'	# external to external

# alpha-testing phase, store all anon-players as registered users
alpha_phase = 1

# constants
USER_SERVER = 0
USER_CLIENT = 1
USER_NUM = 2

# in seconds
TICKET_EXPIRATION = 60.0
