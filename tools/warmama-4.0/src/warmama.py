#!/usr/bin/env python2.7
#-*- coding:utf-8 -*-

"""
Created on 28.3.2011
@author: hc
"""

###################
#
# Imports

import config
import database
import session
import session.login
import game.match

import IPy as ipy
import socket
import random
import zlib
import base64
import json
import datetime
import string
import os
import errno
import re
import urllib

import traceback

###################
#
# Constants

###################
#
# Globals

###################
#
# Helpers

def safestr( s ):
	if( s == None ) :
		return ""
	return s

def safeint( s ):
	try :
		return int(s)
	except:
		return 0

def safebool( s ):
	i = safeint(s)
	if( i != 0) :
		return True
	else :
		return False


#### IP's

# returns tuple (ip, port) from string
def ip_port( s ):

	if( s.find( ':' ) != -1 ) :
		ip, port = s.rsplit( ':', 1 )
	else :
		ip = s
		port = ''
		
	return ip, port

# returns tuple (ipv4, ipv6) from given ip (without port!)
def ipv4_ipv6( s ):
	
	try :
		ip = ipy.IP( s )
	except :
		return ( None, None )
	
	if( ip.version() == 4 ) :
		# we have IPv4 and we want to change this to IPV6
		ipv4 = str( ip )
		ipv6 = ''
		
		# this is kinda hacky hack:
		# 1) get the domain name if available
		# 2) fetch the IPv6 address of this domain if available
		# 3) make a full-length string with IPy
		domain = socket.getfqdn( ipv4 )
		try :
			infos = socket.getaddrinfo(domain, 0, socket.AF_INET6)
		except socket.gaierror :
			infos = []

		# got anything here?		
		if( len(infos) ) :
			addr = infos[0][4]
			ipv6 = ipy.IP(addr[0]).strFullsize()
	
	elif( ip.version() == 6 ) :
		# we cant transform IPv6 into IPv4
		ipv4 = ''
		ipv6 = ip.strFullsize()
		
	return ( ipv4, ipv6 )
	
	
def is_ipv4( s ) :
	# WSW sends IP's in ip:port format so we would need to strip the
	# port section from there
	if( s.find( ':' ) != -1 ) :
		s = s.rsplit( ':', 1 )[0]
		
	try:
		socket.inet_pton(socket.AF_INET, s)
		return True
	except socket.error:
		return False
	
def is_ipv6( s ) :
	# WSW sends IPv6 addresses in [x:x:x:x]:port format so we would
	# have to grab the [x.x.x.x] portion into x.x.x.x
	m = re.match( '\[(\S+)\]:\d+', s )
	if( m != None and len( m.groups ) > 0 ) :
		s = m.groups()[0]
	
	try:
		socket.inet_pton(socket.AF_INET6, s)
		return True
	except socket.error:
		return False

def makedir( dir ):
	try :
		os.makedirs(dir)
	except OSError as err :
		if( err.errno != errno.EEXIST ) :
			return False
		
	return True

###################
#
# Classes
		
class Warmama(object):
	
	def __init__(self):
		
		if( config.logfile_name ) :
			if( config.logfile_append ) :
				flag = 'a'
			else :
				flag = 'w'
				
			self.logFile = open( config.logfile_name, flag )

		self.dbHandler = database.DatabaseWrapper(self,
			config.db_host, 
													config.db_user,
													config.db_passwd,
													config.db_name,
													config.db_engine,
													config.db_charset )
		
		self.sessionHandler = session.SessionHandler( self )
		self.userHandler = session.users.UserHandler( self )
		self.matchHandler = game.match.MatchHandler( self )
		
			
	def gen_uuid(self, mask=0xffffffff):
		return random.randint(0, mask)
	
	def user_uuid(self, users):
		uuid = self.gen_uuid(0x3fffffff)
		while ( uuid == 0 or uuid in users ) :
			uuid = self.gen_uuid(0x3fffffff)
			
		return uuid
	
	def gen_digest(self, length):
		my_printable = string.letters + string.digits+ '_-'
		digest = [ my_printable[random.randint(0, len(my_printable)-1)] for x in xrange( length ) ]
		digest = ''.join( digest )
		return digest
			
			
	def log( self, msg ) :
		# print( msg )
		if( self.logFile ) :
			self.logFile.write('%s %s\n' % (
				datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
				msg))
			self.logFile.flush()
		
	##################################
	#
	#			HANDLERS
	#
	##################################
	
	def Heartbeat(self, sessionId, ip, type):
		try :
			if( sessionId == 0 ) :
				return '0'
			
			s = self.sessionHandler.LoadSession(sid=sessionId, type=type)
			# (TODO: use ipv4_ipv6 and match up with v4 OR v6)
			if( s == None ) :
				self.log( 'Heartbeat no session for %s @ %s' % (type, type))
				return '0'
			if( s.ip != ip ) :
				self.log( "Heartbeat from %s by %s invalid ip, should be %s" % (ip, type, s.ip))
				return '0'
		
			self.log( "Heartbeat from %s @ %s" % (type, ip) )
			
			# this will mark the timestamp by itself	
			self.sessionHandler.SaveSession(s)
			
			return '1'
		except Exception as e:
			self.log( "Heartbeat exception %s" % e)
			return '0'
	
	##################################
	#
	#			SERVER
	#
	##################################

	# TODO: Refine the JSON-api. Consider status/ready items and their meaning.
	
	# authkey's consist of letters, digits and _- (ie URL encoding)
	def ValidateAuthKey(self, authkey):
		for a in authkey :
			if( not( a in string.letters or a in string.digits or a in '_-' ) ) :
				return False		
		return True


	def ServerLogin( self, authkey, ip, port, hostname, demos_baseurl ):
	
		# TODO: move this code to session.login

		_SV_JSON = True

		try :	
			if( not self.ValidateAuthKey(authkey) ) :
				self.log( "authkey not valid")
				if(_SV_JSON):
					return json.dumps({'id':0})
				return '0';
			
			ip, ipv6 = ipv4_ipv6( ip )
			
			# Changed to use LoadServer from User interface so we can save changing attributes
			server = self.userHandler.LoadServer(authkey)
			if( not server ) :
				self.log( "Couldnt authenticate server with authkey \"%s\"" % ( authkey ) )
				if(_SV_JSON):
					return json.dumps({'id':0})
				return '0'

			if( server.uuid == 0 ) :
				self.log( "Couldnt authenticate server with authkey \"%s\"" % ( authkey ) )
				if(_SV_JSON):
					return json.dumps({'id':0})
				return '0'
			elif( server.regip != ip and server.regipv6 != ipv6 ) :
				self.log( "ServerLogin: reg IP doesnt match (%s vs %s)(%s vs %s)" %	
					( ip, server.regip, ipv6, server.regipv6 ) )
				if(_SV_JSON):
					return json.dumps({'id':0})
				return '0'
			elif( server.banned ) :
				self.log( "ServerLogin: server banned %s %s" % ( ip, ipv6 ) )
				if(_SV_JSON):
					return json.dumps({'id':0})
				return '0'
			
			# Update some fields
			server.hostname = hostname
			server.demos_baseurl = demos_baseurl
			# server.location = ..

			self.userHandler.SaveServer(server)

			# THIS SHOULD BE PUT TO SEPARATE FUNCTION, WITH COMMON FACTORS
			# TO CLIENT AND SERVER
			
			# sessions
			s = self.sessionHandler.LoadSession(uuid=server.uuid, type='server')
			if( s != None ) :
				self.log("Found existing session")
				# existing session, do we use this or what?
				# port ? expiration ? (TODO: use ipv4_ipv6 and match up with v4 OR v6)
				if( ip == s.ip or ipv6 == s.ipv6 ) :
					# ehm, reset state (kick players, purge player etc..)
					s.setPort( port )
					self.sessionHandler.SaveSession(s)
					self.log( 'ServerLogin: found old session %d' % s.id )
					if(_SV_JSON):
						return json.dumps({
							'id': s.id
						})
					return '%d' % s.id
				else :
					self.log("ServerLogin: IP's dont match (%s vs %s)(%s vs %s)" % ( ip, s.ip, ipv6, s.ipv6 ) )
				# not allowed
				self.log("Server already logged-in")
				if(_SV_JSON):
					return json.dumps({'id':0})
				return '0'
			else :
				# create a new session yeehaa
				s = self.sessionHandler.NewSession(server.uuid, ip, ipv6, 'server')
				s.setPort( port )
				self.sessionHandler.SaveSession(s)

				self.log( "ServerLogin: Created session %d" % s.id )
				if(_SV_JSON):
					return json.dumps({
						'id': s.id
					})
				return '%d' % s.id
					
			self.log("ServerLogin: unknown error")
			if(_SV_JSON):
				return json.dumps({'id':0})
			return '0'
		except Exception as e :
			self.log( "ServerLogin exception %s" % e)
			self.log( "Traceback: %s" % traceback.format_exc())
			if(_SV_JSON):
				return json.dumps({'id':0})
			return '0'
	
	def ServerLogout(self, ssession, ip):
		# TODO: First remove all purge_players that
		# match this server
		# for all players who's server session matches
		# reset their servers or if their purgable flag is
		# set, remove them totally

		_SV_JSON = True

		try :
			ip, ipv6 = ipv4_ipv6( ip )
			
			s = self.sessionHandler.LoadSession(sid=ssession, type='server')
			if( not s ) :
				self.log( "ServerLogout: no such session %d" % ssession )
				if(_SV_JSON):
					return json.dumps({'status':0})
				return '0'
			
			# validate IP (TODO: use ipv4_ipv6 and match up with v4 OR v6)
			if( ip != s.ip and ipv6 != s.ipv6 ) :
				self.log( "ServerLogout: IP doesnt match! (%s vs %s)" % ( ip, s.ip, ipv6, s.ipv6 ) )
				if(_SV_JSON):
					return json.dumps({'status':0})
				return '0'
			
			self.log( "ServerLogout: server %d logging off" % s.id )
			
			self.sessionHandler.RemoveSession(s)

			if(_SV_JSON):
				return json.dumps({'status':1})
			return '1'

		except Exception as e :
			self.log( "ServerLogout exception %s" % e)
			self.log( "Traceback: %s" % traceback.format_exc())
			if(_SV_JSON):
				return json.dumps({'status':1})
			return '0'
	
	# TODO: server ip anyone?
	def ServerClientConnect(self, ssession, csession, cticket, cip):
		
		_SV_JSON = True

		try :
			if( csession == 0 or ssession == 0 ) :
				self.log( "ServerClientConnect null session: csession %d ssession %d" % (csession, ssession))
				if(_SV_JSON):
					return json.dumps({'id':0})
				return '0'
			
			if( cticket == 0 ) :
				self.log( "ServerClientConnect null ticket")
				if(_SV_JSON):
					return json.dumps({'id':0})
				return '0'
			
			# TODO: bottle these 2 calls to 1
			sv = self.sessionHandler.LoadSession(sid=ssession, type='server')
			if( not sv ) :
				self.log( "ServerClientConnect: No such server %d" % ssession )
				if(_SV_JSON):
					return json.dumps({'id':0})
				return '0'
			
			cl = self.sessionHandler.LoadSession(sid=csession, type='client')
			if( not cl ) :
				self.log( "ServerClientConnect: No such client %d" % csession )
				if(_SV_JSON):
					return json.dumps({'id':0})
				return '0'
				
			( ticketId, ticketServer, ticketExpiration ) = cl.getTicket()
			
			# game server sends the ip in ip:port format so, take off the port
			cip = cip.rsplit( ':', 1 )[0]
				
			cip, cipv6 = ipv4_ipv6( cip )
			
			if( ticketId != cticket ) :
				self.log( "ServerClientConnect invalid ticket: ticketId %d cticket %d" % (ticketId, cticket))
				if(_SV_JSON):
					return json.dumps({'id':0})
				return '0'
			# (TODO: use ipv4_ipv6 and match up with v4 OR v6)
			if( cip != cl.ip  and cipv6 != cl.ipv6 ) :
				self.log("client ip (%s vs %s)(%s vs %s)" % ( cip, cl.ip, cipv6, cl.ipv6 ) )
				if(_SV_JSON):
					return json.dumps({'id':0})
				return '0'
			if( ticketServer != sv.id ) :
				self.log("ticketserver %d vs %d" % ( ticketServer, sv.id ))
				if(_SV_JSON):
					return json.dumps({'id':0})
				return '0'
			
			# ticket expiration
			deadline = ticketExpiration + datetime.timedelta( seconds = config.TICKET_EXPIRATION )
			if( deadline < datetime.datetime.now() ) :
				self.log( "ServerClientConnect deadline %s < %s" % (deadline, datetime.datetime.now()))
				if(_SV_JSON):
					return json.dumps({'id':0})
				return '0'
	
			cl.setServer( sv.id )
			
			self.sessionHandler.SaveSession( cl )
			
			login = self.userHandler.LoadUserLogin( cl.user_id )
			stats = self.userHandler.LoadUserRatings( cl.user_id )
				
			# some very clever python stuff to create the stats string
			# <gametype> <rating> <gametype> <rating>. ..
			statsString = ''.join( map( lambda x: '%s %d ' % (x[0], x[1][0]), stats.iteritems() ) )
			self.log("Created statstring %s" % statsString )
			
			if(_SV_JSON):
				_stats = [
					{
						'gametype': x[0],
						'rating': x[1][0],
						'deviation': x[1][1]
					} for x in stats.iteritems()
				]
				return json.dumps({'id':cl.id, 'login':login, 'ratings':_stats})

			return '%d %s %s' % ( cl.id, login, statsString )

		except Exception as e :
			self.log( "ServerClientConnect exception %s" % e)
			self.log( "Traceback: %s" % traceback.format_exc())
			return '0'

	def ServerClientDisconnect(self, ssession, csession, gameon):
	
		_SV_JSON = True

		try :
			# TODO: add this player to purge_players if
			# 'gameon' is set
			if( csession == 0 or ssession == 0 ) :
				self.log( "ServerClientDisconnect error csession %d ssession %d" % (csession, ssession))
				if(_SV_JSON):
					return json.dumps({'status':0})
				return '0'
			
			cl = self.sessionHandler.LoadSession(sid=csession, type='client')
			if( not cl ) :
				self.log( "ServerClientConnect: No such client %d" % csession )
				if(_SV_JSON):
					return json.dumps({'status':0})
				return '0'
			
			# validate that this client is on this server
			# FIXME: bugs out
			if( cl.getServer() != ssession ) :
				self.log( "cl.getServer() != ssession (%d vs %d)" % (cl.getServer(), ssession) )
				if(_SV_JSON):
					return json.dumps({'status':0})
				return '0'
			
			# reset server
			cl.setServer( 0 )
			
			# dummy
			if( gameon ) :
				self.log("Adding session %d to purgables" % cl.id)
				self.dbHandler.AddPurgable(cl.id, cl.user_id, ssession)
				
			self.sessionHandler.SaveSession(cl)
			
			self.log( "ServerClientDisconnect %d %d ok" % (ssession, csession))
			if(_SV_JSON):
				return json.dumps({'status':1})
			return '1'

		except Exception as e :
			self.log( "ServerClientDisconnect exception %s" % e)
			self.log( "Traceback: %s" % traceback.format_exc())
			if(_SV_JSON):
				return json.dumps({'status':0})
			return '0'

		
	##################################
	
	def MatchReport(self, ssession, report, ip) :
		
		_SV_JSON = True

		try :
			# validate ssession
			if( ssession == 0 ) :
				self.log( "MatchReport null session" )
				if(_SV_JSON):
					return json.dumps({'status':0})
				return '0'
			
			sv = self.sessionHandler.LoadSession( sid = ssession, type = 'server' )
			if( not sv ) :
				self.log( "MatchReport couldnt find session for %d" % ssession)
				if(_SV_JSON):
					return json.dumps({'status':0})
				return '0'
			
			ip, ipv6 = ipv4_ipv6( ip )
			if( ip != sv.ip and ipv6 != sv.ipv6 ) :
				self.log( "MatchReport: Wrong server address (%s vs %s)(%s vs %s)" % ( ip, sv.ip, ipv6, sv.ipv6 ) )
				if(_SV_JSON):
					return json.dumps({'status':0})
				return '0'
			
			# decode report
			try : report = base64.b64decode( report.encode( 'ascii' ), '-_' )
			except TypeError as err:
				self.log( "MatchReport: base64 FAIL %s" % str(err) )
				if(_SV_JSON):
					return json.dumps({'status':0})
				return '0'
			
			try : report = zlib.decompress( report )
			except zlib.error :
				self.log( "MatchReport: zlib FAIL")
				if(_SV_JSON):
					return json.dumps({'status':0})
				return '0'
				
			# WRITE THIS THING TO A FILE
			if( config.report_dir ) :
				filename = os.path.join( config.report_dir, '%s.json' % datetime.datetime.now().strftime('%Y-%m-%d-%H%M') )
				f = open( filename, 'w' )
				f.write( report )
				f.close()
			
			uuid_retries = 0
			uuid = sv.next_match_uuid
			while uuid_retries < 10 and self.dbHandler.CheckMatchUUID( uuid ) == False:
				# diplicate match record found, attempt to store under a different uuid
				uuid = self.dbHandler.GenerateMatchUUID( ssession )
				uuid_retries += 1
			
			# ' this here acknowledges userhandler to save player stats '
			# ratings is a list of tuples (sessionId, rating)
			# FIXED: gave session-id as a parameter when in fact it should be the user-id (UUID)
			( gametype, ratings ) = self.matchHandler.AddReport( sv.user_id, report, uuid )
			
			# TODO: make RemovePurgables return the list of removed sessions
			# so we can filter them out from the ratings list
			# remove sessions_players w/purgable=1 & server=this
			self.dbHandler.RemovePurgables( ssession )
			
			output = ( '%s ' % (gametype) ).join( [ '%d %d ' % (x[0],x[1]) for x in ratings ] )
			# do we have some problems with this?
			self.log( "MatchReport output: %s" % output )

			if(_SV_JSON):
				return json.dumps({
					'status': 1,
					'ratings': {
						'gametype': gametype,
						'ratings':ratings
					}
				})
			return output

		except Exception as e:
			self.log( "MatchReport exception %s" % e)
			self.log( "Traceback %s" % traceback.format_exc())
			if(_SV_JSON):
				return json.dumps({'status':0})
			return '0'
	
	def MatchUUID(self, sessionId, ip):
		_SV_JSON = True

		try :
			if( sessionId == 0 ) :
				if(_SV_JSON):
					return json.dumps({'uuid':''})
				return ''
			
			s = self.sessionHandler.LoadSession(sid=sessionId, type='server')

			if( s == None ) :
				self.log( "MatchUUID from %s: invalid session id %s" % (ip, sessionId))
				return ''

			# (TODO: use ipv4_ipv6 and match up with v4 OR v6)
			if( s.ip != ip ) :
				self.log( "MatchUUID from %s: invalid ip, should be %s" % (ip, s.ip))
				if(_SV_JSON):
					return json.dumps({'uuid':''})
				return ''

			match_uuid = self.dbHandler.GenerateMatchUUID( sessionId )

			self.log( "MatchUUID request from %s, returning %s" % ( ip, match_uuid ) )

			if(_SV_JSON):
				return json.dumps({'uuid':match_uuid})			
			return match_uuid

		except Exception as e:
			self.log( "MatchUUID exception %s" % e)
			if(_SV_JSON):
				return json.dumps({'uuid':''})			
			return ''

	##################################
		
	def ClientLogin( self, login, pw, handle, ip ):
	
		# this here returns "state uuid"
		# where 'state' is 
		#	-1 for 'we gave a handle for you' (uuid = handle)
		# 	1 for 'login isnt ready yet' (uuid = 0)
		#	2 for 'login ready' (uuid = session_id)
		# so "2 0" means error
	
		# TEST
		_CL_JSON = True
	
		try :
			(ip, ipv6) = ipv4_ipv6( ip )
			
			self.log("Clientlogin %s %s %s %s" % ( login, handle, ip, ipv6 ) )
			
			# check if we are on 2nd step
			if( handle != 0 ) :
				
				#### STEP 2
				
				self.log( "Requesting login by handle %d" % handle )
				
				(ready, valid, login, profile_url, profile_url_rml) = self.dbHandler.GetUserLogin(handle)
				if( not ready ) :
					self.log( "Not ready")
					if(_CL_JSON):
						return json.dumps({'ready':1, 'id':0});
					return '1 0'
				
				# we can remove the handle now..
				self.dbHandler.RemoveUserLogin(handle)
				
				if( not valid ) :
					# FAIL
					self.log("GetUserLogin failed to authenticate %s" % login )
					if(_CL_JSON):
						return json.dumps({'ready':2, 'id':0});
					return '2 0'
				
				# now fetch the user
				user = self.userHandler.LoadPlayer(login, ip, ipv6)
				
				# we got real user so create a session for this one
				s = self.sessionHandler.CreatePlayerSession( user.uuid, ip, ipv6 )
				if( s == None ) :
					self.log("CreatePlayerSession failed with %d %s %s" % (user.uuid, ip, ipv6))
					if(_CL_JSON):
						return json.dumps({'ready':2, 'id':0});
					return '2 0'
			
				# Apply some parameters to profile_url, CHEAP HACK THX ALOT CRIZIS ;P
				profile_url_rml = profile_url_rml.format(**{
					'session': s.id,
					# Possibly other parameters?
				});

				stats = self.userHandler.LoadUserRatings( s.user_id )
				statsString = ''.join( map( lambda x: '%s %d ' % (x[0], x[1][0]), stats.iteritems() ) )
				
				self.log( "ClientLogin: Created session %d (user %d)" % (s.id, s.user_id) )
				# TODO: refine the statistics object. Dismiss deviation
				if(_CL_JSON):
					# return json.dumps({'ready':2, 'id':s.id,'stats':stats});
					_stats = [
						{
							'gametype': x[0],
							'rating': x[1][0],
							'deviation': x[1][1]
						} for x in stats.iteritems()
					]
					self.log(str(_stats))
					return json.dumps({
						'ready':2,
						'id':s.id,
						'ratings':_stats,
						'profile_url':profile_url,
						'profile_url_rml':profile_url_rml
					})

				return '2 %d %s %s %s' % ( s.id, statsString, profile_url, profile_url_rml )
					
			#######################
			
			#### STEP 1
			
			# we are on 1st step, start the login process
			cl = session.login.ClientLogin( self, login, pw )
			self.log("Generated login handle %d" % cl.GetHandle() )
			
			if(_CL_JSON):
				return json.dumps({'ready':-1, 'handle':cl.GetHandle(),'id':0});
			return '-1 %d' % cl.GetHandle()

		except Exception as e :
			self.log( "ClientLogin exception %s" % e)
			self.log( "Traceback: %s" % traceback.format_exc())
			if(_CL_JSON):
				return json.dumps({'ready':2, 'id':0});
			return '2 0'
	
	
	def ClientLogout(self, csession, ip):
		# TODO: if client is on a server, i.e. will have his result coming
		# up, mark as purgable and put to purge_players - DONT remove session
		# just yet
		
		_CL_JSON = True

		try :
			(ip, ipv6) = ipv4_ipv6( ip )
			
			s = self.sessionHandler.LoadSession(sid=csession, type='client')
			if( not s ) :
				self.log( "ClientLogout: no such session %d" % csession )
				if(_CL_JSON):
					return json.dumps({'status':0});
				return '0'
			
			# validate IP (TODO: use ipv4_ipv6 and match up with v4 OR v6)
			if( ip != s.ip and ipv6 != s.ipv6 ) :
				self.log( "ClientLogout: IP doesnt match! (%s vs %s)(%s vs %s)" % ( ip, s.ip, ipv6, s.ipv6 ) )
				if(_CL_JSON):
					return json.dumps({'status':0});
				return '0'
			
			self.log( "ClientLogout: client %d logging off" % s.id )
			
			if( self.dbHandler.OnPurgables(s.id, s.user_id) ) :
				self.log( "Marking session %d purgable" % s.id )
				s.purgable = 1
				s.setServer(0)
				self.sessionHandler.SaveSession(s)
				
			else :
				self.log("Removing session %d" % s.id)
				self.sessionHandler.RemoveSession(s)
		
			if(_CL_JSON):
				return json.dumps({'status':1});
			return '1'

		except Exception as e :
			self.log( "ClientLogout exception %s" % e)
			self.log( "Traceback: %s" % traceback.format_exc())
			if(_CL_JSON):
				return json.dumps({'status':0});
			return '0'
	
	# TODO: server's ip in here
	def ClientConnect(self, csession, saddr):
	
		_CL_JSON = True
		try :
			if( csession == 0 ) :
				self.log( "ClientConnect: null session")
				if(_CL_JSON):
					return json.dumps({'ticket':0});
				return '0'
				
			# TODO: IPv6 parsing and whatnots
			self.log("ClientConnect to address %s" % saddr )
			
			ip, port = ip_port( saddr )
			port = safeint( port )
			if( not port ) :
				port = 44400 # DEFAULT PORT, TODO: CONSTANT
			
			ipv4, ipv6 = ipv4_ipv6( ip )
			
			# FIXME: currently only ipv4 is supported
			if( not ipv4 ) :
				self.log( "ClientConnect, couldnt get ipv4 for %s" % saddr )
				if(_CL_JSON):
					return json.dumps({'ticket':0});
				return '0'
			
			# server identification by ip
			sv = self.sessionHandler.ServerSessionByAddr(ipv4, ipv6, port)
			if( not sv ) :
				self.log( "ClientConnect: no such server %s:%d OR %s:%d" % ( ipv4, port, ipv6, port ) )
				if(_CL_JSON):
					return json.dumps({'ticket':0});
				return '0'
			
			# client identification by session
			cl = self.sessionHandler.LoadSession(sid=csession, type='client')
			if( not cl ) :
				self.log( "ClientConnect: no such session %d" % csession )
				if(_CL_JSON):
					return json.dumps({'ticket':0});
				return '0'
		
			self.log("Found server %d for client %d (addr %s %s)" % (sv.id, cl.id, ipv4, ipv6) )
			
			# any random ticket will do
			ticket = self.gen_uuid( 0xfffffff )
			cl.setTicket( ticket, sv.id, datetime.datetime.now() )
			
			self.sessionHandler.SaveSession(cl)
			
			if(_CL_JSON):
				return json.dumps({'ticket':ticket});
			return '%u' % ticket

		except Exception as e :
			self.log( "ClientConnect exception %s" % e)
			self.log( "Traceback: %s" % traceback.format_exc())
			if(_CL_JSON):
				return json.dumps({'ticket':0});
			return '0'

	# AUTH response from auth server
	def ClientAuthenticate(self, handle, secret, valid, profile_url, profile_url_rml):
		# TODO: validate handle and secret
		try :
			self.log( "ClientAuthenticate %s %s %s %s" % (handle, secret, valid, profile_url))

			# parameters: handle, login(not saved), ready, valid
			self.dbHandler.SaveUserLogin(handle, None, 1, valid, profile_url, profile_url_rml)
			return ''
		except Exception as e :
			self.log( "ClientAuthenticate exception %s" % e)
			self.log( "Traceback: %s" % traceback.format_exc())
			return ''

####################################

# printing screws up FCGI
if(config.cgi_mode == 'local'):
	print( "Starting Warmama.." )
warmama = Warmama()
