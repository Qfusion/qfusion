#!/usr/bin/env python2.7
#-*- coding:utf-8 -*-

"""
Created on 30.3.2011
@author: hc
"""

###################
#
# Imports
import game.skills as skills

import datetime

###################
#
# Constants

###################
#
# Globals

###################
#
# Helpers

###################
#
# Classes

## These are replicas of DB objects

class PlayerStats :
	
	def __init__ (self, uuid=0, dbId=0, wins=0, losses=0, quits=0, rating=skills.DEFAULT_RATING, deviation=skills.DEFAULT_DEVIATION, gametype=''):
		self.uuid = uuid
		self.wins = wins
		self.losses = losses
		self.quits = quits
		self.rating = rating
		self.deviation = deviation
		self.gamesForPeriod = 0
		self.lastGame = 0
		self.dbId = dbId
		self.gametype = gametype
	
	# debug
	def __repr__(self):
		return 'uuid: %u wins: %d losses: %d quits: %d rating: %f deviation: %f gametype: %s' % (
				self.uuid, self.wins, self.losses, self.quits, self.rating, self.deviation, self.gametype )

class Server(object):
	
	def __init__( self, uuid=0, login='', regip='', regipv6='', hostname='', ip='', ipv6='', location='', banned=False, demos_baseurl='' ) :
		self.uuid = uuid
		self.login = login
		self.regip = regip
		self.regipv6 = regipv6
		self.hostname = hostname
		self.ip = ip
		self.ipv6 =ipv6
		self.location = location
		self.banned = banned
		self.demos_baseurl = demos_baseurl

		
class Player(object):
	
	def __init__(self, uuid=0, login='', nickname='', ip='', ipv6='', location='', banned=False ):
		self.uuid = uuid
		self.login = login
		self.nickname = nickname
		self.ip = ip
		self.ipv6 =ipv6
		self.location = location
		self.banned = banned
		
		
#################################

class UserHandler(object):
	
	def __init__(self, mm):
		self.mm = mm
		self.dbHandler = mm.dbHandler
		
	def stats_fromdb(self, uuid, fields) :
		# fields are in order of ( uuid, db-id, wins, losses, quits, rating, deviation, gametype )
		return PlayerStats( uuid, fields[0], fields[1], fields[2], fields[3], fields[4], fields[5], fields[6] )
	
	def stats_todb(self, stats):
		fields = ( stats.dbId, stats.wins, stats.losses, stats.quits, stats.rating, stats.deviation )
		return fields
		
	def server_fromdb(self, fields):
		# fields are (uuid, created, updated, login, regip, regipv6, hostname, ip, ipv6, location, banned, demos_baseurl)
		return Server( fields[0], fields[3], fields[4], fields[5], fields[6], fields[7], fields[8], fields[9], fields[10], fields[11] )
		
	def server_todb(self, server):
		return ( server.uuid, server.login, server.regip, server.regipv6, server.hostname,
				server.ip, server.ipv6, server.location, server.banned, server.demos_baseurl )
	
	def player_fromdb(self, fields):
		# fields are (uuid, created, updated, ogin, nickname, ip, ipv6, location, banned)
		return Player( fields[0], fields[3], fields[4], fields[5], fields[6], fields[7], fields[8] )
	
	def player_todb(self, player):
		return ( player.uuid, player.login, player.nickname, player.ip,
				player.ipv6, player.location, player.banned )
	
	'''
	LoadUserStats
	load a whole bunch of player stats by giving sid as a list of user-ids
	or single players stats by giving user-id as an integer
	'''
	def LoadUserStats(self, uuids, gametypeId):
		
		# FIXED: bug where list of len 1 and mysqldb goes crazy
		
		#########################
		valid = True
		# DONE: filter out 0-uuids
		if( isinstance( uuids, list ) ) :
			uuids_copy = []
			for uuid in uuids :
				if( uuid != 0 ) :
					uuids_copy.append(uuid)
			
			if( len( uuids_copy ) == 0 ) :
				valid = False
			elif( len( uuids_copy ) == 1 ) :
				uuids_copy = uuids_copy[0]
				
		elif( isinstance( uuids, (int, long) ) ) :
			if( uuids != 0 ) :
				uuids_copy = uuids
			else :
				valid = False
		
		else :
			self.mm.log( "LoadUserStats: Invalid type for uuid: %s" % type( uuids ) )
			return {}
		
		#########################
				
		if( valid ) :
			# first load the stats
			stats_fields = self.dbHandler.LoadUserStats( uuids_copy, gametypeId )
			# then the 'last game played'
			matches = self.dbHandler.LoadLastMatches( uuids_copy, gametypeId )
			
		else :
			stats_fields = None
			matches = None
		
		#########################
			
		# now we need to merge these 2 arrays
		ret = {}
		
		# fix single uuid into list form so we have common way of
		# dealing both types
		if( isinstance( uuids, (int, long) ) ) :
			uuids = [ uuids ]
			
		for uuid in uuids :
			if( stats_fields != None and uuid in stats_fields ) :
				stat = self.stats_fromdb(uuid, stats_fields[uuid])
			else :
				# default empty stats
				stat = PlayerStats(uuid=uuid)
				
			# last game played
			if( matches != None and uuid in matches ) :
				stat.lastGame = matches[uuid]
			else :
				stat.lastGame = datetime.datetime.min
			
			ret[uuid] = stat
			
		return ret
	
	'''
	LoadUserRatings
	Specialized version of above.. fetch ratings for single user and for all gametypes.
	Called when client connects to a server.
	returns a map of { gametype : ( rating, deviation ) }
	'''
	def LoadUserRatings( self, uuid ) :
		ratings = self.dbHandler.LoadUserRatings( uuid )
		if( ratings ) :
			return ratings
		
		# shield for None
		return {}
		
	'''
	LoadUserLogin
	'''
	def LoadUserLogin( self, uuid ):
		return self.dbHandler.LoadUserLogin( uuid )
	
	'''
	UpdatePlayers
	Called by matchhandler after matchreport, new skills and w/l statistics
	m = match object with m.players[], each one having stats = PlayerStats()
	'''
	def UpdatePlayers(self, m):
		d = {}
		for player in m.players :
			d[player.uuid] = self.stats_todb(player.stats)
			
		self.dbHandler.SaveUserStats( d, m.gameTypeId )
		
	'''
	LoadServer
	Servers always exist, so we only need to fetch it, if it
	doesnt exist just return None
	'''
	def LoadServer(self, authkey):
		fields = self.dbHandler.LoadUser( authkey, 'server' )
		if( fields != None ) :
			return self.server_fromdb(fields)
		
		return None
	
	'''
	SaveServer
	Servers are saved after they are logged in and we get updated
	information (pretty much only the hostname changes and location
	if we failed to geolocate last time)
	'''
	def SaveServer(self, server):
		fields = ( server.uuid, server.login, server.regip, server.regipv6, server.hostname,
					server.ip, server.ipv6, server.location, server.banned, server.demos_baseurl )
		self.dbHandler.SaveServer( fields )
		return
	
	'''
	LoadPlayer
	This is trickier, cause players either exist in database or
	doesnt yet.. so thats why we need few additional fields to this
	function call so we can populate new user
	'''
	def LoadPlayer(self, login, ip, ipv6):
		fields = self.dbHandler.LoadUser( login, 'client' )
		if( fields != None ) :
			player = self.player_fromdb(fields)
			player.ip = ip
			player.ipv6 = ipv6
			self.dbHandler.SavePlayer( self.player_todb(player) )
		else :
			# TODO: geolocate !
			player = Player( uuid=0, login=login, nickname='', ip=ip, ipv6=ipv6 )
			player.uuid = self.dbHandler.SavePlayer( self.player_todb(player) )
			
		return player

	'''
	SavePlayer
	Update player info
	'''
	def SavePlayer(self, player):
		self.dbHandler.SavePlayer( self.player_todb(player) )
