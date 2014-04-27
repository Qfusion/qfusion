#!/usr/bin/env python2.7
#-*- coding:utf-8 -*-

"""
Created on 30.3.2011
@author: hc
"""

###################
#
# Imports

import config
import models
from models import *
import dbpool

import MySQLdb
import MySQLdb.cursors

import datetime
import threading
import atexit

import inspect
import types

###################
#
# Constants

###################
#
# Globals

# dbpool.set_database(MySQLdb, 15, 60)

###################
#
# Helpers

###################
#
# Classes

class DatabaseHandler(object):
	
	def __init__(self, wmm, host, user, passwd, db, engine=None, charset=None):
		self.wmm = wmm

		self.connection = None
		self.tables = {}
		
		self.host = host
		self.user = user
		self.passwd = passwd
		self.db = db
			
		if( self.open() ) :
			cursor = self.connection.cursor()
			self.CreateTables(cursor, models, engine, charset, False)
			cursor.close()

		# FIXME: i hope this is enough to make the threading to
		# allow exiting for real instead of hanging
		atexit.register(self.close)
		
	def open(self ):
		connection = MySQLdb.connect( host=self.host, user=self.user, passwd=self.passwd, db=self.db )
		# connection = dbpool.connect( host=self.host, user=self.user, passwd=self.passwd, db=self.db )
		connection.autocommit(True)
		self.connection = connection
		return self.connection != None
		
	def close(self):
		if( self.connection ) :
			self.connection.close()
		
	# get last inserted id, cursor has to be valid
	def getid(self, cursor):
		cursor.execute ( 'SELECT LAST_INSERT_ID()' )
		r = cursor.fetchone()
		if ( r ) :
			return r[0]
		return 0
		
	##################################
	#
	#			SESSIONS
	#
	##################################
		
	'''
	GetSession
	Returns existing session or None as tuple
	uuid is user_id
	'''
	def GetSession(self, cursor, uuid=None, sid=None, type=None) :
		if( uuid == None and sid == None ) :
			self.wmm.log("database.GetSession: no UUID or SID!")
			return None
		
		if( type == 'client' ) :
			table = table_SessionsPlayer
		else :
			table = table_SessionsServer
		
		if( uuid != None ) :
			query = 'SELECT * from %s WHERE user_id=%%s' % table.tablename
			values = ( uuid, )
		else :
			query = 'SELECT * from %s WHERE id=%%s' % table.tablename
			values = ( sid, )
			
		# FIXME: create the session object here
		cursor.execute( query, values )
		r = cursor.fetchone()
		return r

	'''
	SaveSession
	saves session to database
	'''
	def SaveSession(self, cursor, s):
		if( s.type == 'client' ) :
			table = table_SessionsPlayer
			query = '''
				INSERT INTO %s
				(created, updated, user_id, ip, ipv6, digest, ticket_id,
				ticket_server, ticket_expiration, server_session, purgable)
				VALUES( NOW(), NOW(), %%s, %%s, %%s, %%s, %%s, %%s, %%s, %%s, %%s )
				''' % table.tablename
			values = ( s.user_id, s.ip, s.ipv6, s.digest, s.ticket_id, s.ticket_server,
					s.ticket_expiration, s.server_session, s.purgable )
		
		else:
			table = table_SessionsServer
			query = '''
				INSERT INTO %s
				(created, updated, user_id, ip, ipv6, digest, port )
				VALUES( NOW(), NOW(), %%s, %%s, %%s, %%s, %%s )
				''' % table.tablename
			values = ( s.user_id, s.ip, s.ipv6, s.digest, s.port )
			
		cursor.execute( query, values )
		cursor.execute( 'SELECT LAST_INSERT_ID()' )
		r = cursor.fetchone()
		self.connection.commit()
		
		if( r ) :
			return r[0]
		return 0
	
	'''
	UpdateSession
	'''
	def UpdateSession(self, cursor, s):
		if( s.type == 'client' ) :
			table = table_SessionsPlayer
			query = '''
				UPDATE %s SET
				updated=NOW(), ip=%%s, ipv6=%%s, digest=%%s, ticket_id=%%s, ticket_server=%%s,
				ticket_expiration=%%s, server_session=%%s, purgable=%%s
				WHERE id=%%s
				''' % table.tablename
			values = ( s.ip, s.ipv6, s.digest, s.ticket_id, s.ticket_server,
					s.ticket_expiration, s.server_session, s.purgable, s.id )
		
		else:
			table = table_SessionsServer
			query = '''
				UPDATE %s SET
				updated=NOW(), ip=%%s, ipv6=%%s, digest=%%s, port=%%s
				WHERE id=%%s
				''' % table.tablename
			values = ( s.ip, s.ipv6, s.digest, s.port, s.id )
			
		cursor.execute( query, values )
		self.connection.commit()

	# Just raw removal of given session
	def RemoveSession(self, cursor, s):
		if( s.type == 'client' ) :
			table = table_SessionsPlayer
		else :
			table = table_SessionsServer
			
		# TODO: match more attributes ?
		query = '''
			DELETE FROM %s WHERE id=%%s
			''' % table.tablename
		values = ( s.id, )
		
		cursor.execute( query, values )
		self.connection.commit()
		
	####################################
	#
	#		MORE ON SESSIONS
	#
	####################################
	
	# server session
	def SessionByAddr(self, cursor, ip, ipv6, port):
		query = '''
			SELECT * FROM %s 
			WHERE (ip=%%s OR (ipv6!='' AND ipv6=%%s))
			AND port=%%s
			''' % table_SessionsServer.tablename
		values = ( ip, ipv6, port )
		
		cursor.execute( query, values )
		r = cursor.fetchone()
		return r
	
	# this here resets all clients sessions that are in
	# given server, removes those marked purgable
	# and removes purge players too
	def ResetServers(self, cursor, ssession):
		return

	'''
	GetUUIDs
	returns dict of { sid : uuid } for given list of sessions
	'''
	def GetUUIDs(self, cursor, sessions):
		if( isinstance( sessions, list ) ) :
			query = '''
				SELECT id, user_id
				FROM %s 
				WHERE id in %%s 
				''' % table_SessionsPlayer.tablename
			
		elif( isinstance( sessions, (int, long) ) ) :
			query = '''
				SELECT id, user_id
				FROM %s 
				WHERE id = %%s 
				''' % table_SessionsPlayer.tablename
			
		values = ( sessions, )
		
		cursor.execute( query, values )
		rows = cursor.fetchall()
		if( rows ) :
			d = {}
			for r in rows :
				d[r[0]] = r[1]
			return d
		return None

	# Generate a UUID for given server which will be used
	# as UUID for the next incoming match result
	def GenerateMatchUUID(self, cursor, ssession):
		# From vic: Due replication issues, generate UUID with select
		# which is run solely on master and then pass that to update

		cursor.execute('SELECT UUID()')
		r = cursor.fetchone()
		uuid = r[0] if(r and len(r)) else '""'
		
		query = '''
			UPDATE %s
			SET next_match_uuid=%%s
			WHERE id=%%s
			''' % table_SessionsServer.tablename
		cursor.execute(query, (uuid, ssession,))
		self.connection.commit()
		return uuid

	# Check whether there is NO match record with matching UUID
	def CheckMatchUUID(self, cursor, uuid):
		query = '''
			SELECT 1 FROM %s 
			WHERE `uuid`=%%s
			LIMIT 1
			''' % table_MatchResults.tablename
		values = ( uuid )
		cursor.execute( query, values )
		r = cursor.fetchone()
		
		if ( r ) :
			return False
		return True

	####################################
	#
	#		PURGABLES
	#
	####################################
	
	def AddPurgable(self, cursor, session_id, user_id, server ):
		query = '''
			INSERT INTO %s
			( created, updated, session_id, player_id, server_session )
			VALUES( NOW(), NOW(), %%s, %%s, %%s )
			''' % table_PurgePlayers.tablename
			
		values = ( session_id, user_id, server )
	
		cursor.execute( query, values )
		self.connection.commit()

	def OnPurgables( self, cursor, session_id, user_id ):
		query = '''
			SELECT id FROM %s
			WHERE session_id=%%s AND player_id=%%s
			''' % table_PurgePlayers.tablename
			
		values = ( session_id, user_id )
		
		cursor.execute( query, values )
		r = cursor.fetchone()
		if( r ) :
			return True
		return False

	# remove all sessions that are marked purgable
	# and connected to this server
	def RemovePurgables(self, cursor, server):
		# first query selects the purgable id's
		query = '''
			SELECT session_id FROM %s
			WHERE server_session = %%s
			''' % table_PurgePlayers.tablename
		values = ( server, )
		
		cursor.execute( query, values )
		rows = cursor.fetchall()
		if( rows == None ) :
			print("RemovePurgables: no purgables")
			return
	
		sids = [ r[0] for r in rows ]
		if( not len(sids) ) :
			return
		
		# second query removes sessions that are marked purgable
		# and are connected to these purge_players
		
		# fix the bizarre issue with mysqldb failing on list-size = 1
		if( len( sids ) == 1 ) :
			query = '''
				DELETE FROM %s
				WHERE id=%%s AND purgable=1
				''' % table_SessionsPlayer.tablename
			values = ( sids[0], )
		else :
			query = '''
				DELETE FROM %s
				WHERE id in %%s AND purgable=1
				''' % table_SessionsPlayer.tablename
			values = ( sids, )
		
		cursor.execute( query, values )
		
		# final step is to remove the purge_players too
		query = '''
			DELETE FROM %s WHERE server_session=%%s
			''' % table_PurgePlayers.tablename
		values = ( server, )
		
		cursor.execute( query, values )
		self.connection.commit()
		
	####################################
	#
	#			USER
	#
	####################################
	
	'''
	GetServerLoginFields
	returns (uid, ip, ipv6)
	'''
	def GetServerLoginFields(self, cursor, authkey):
		query = 'SELECT id, ip, ipv6 from %s WHERE login=%%s' % table_Servers.tablename
		values = ( authkey, )
		
		cursor.execute( query, values )
		r = cursor.fetchone()
		
		return r
		
	
	def LoadUser(self, cursor, login, type):
		if( type == 'server') :
			table = table_Servers.tablename
		else :
			table = table_Players.tablename
		
		query = 'SELECT * from %s WHERE login=%%s' % table
		values = ( login, )
		
		cursor.execute( query, values )
		r = cursor.fetchone()
		
		return r
	
	'''
	SaveServer
	Servers already exist in the database so we are only updating
	information
	fields are ( server.uuid, server.login, server.regip, server.regipv6, server.hostname,
				server.ip, server.ipv6, server.location, server.banned )
	'''
	def SaveServer(self, cursor, fields):
		query = '''
			UPDATE %s
			SET hostname=%%s, location=%%s, demos_baseurl=%%s
			WHERE id=%%s
			''' % table_Servers.tablename
			
		values = ( fields[4], fields[7], fields[9], fields[0] )
		
		cursor.execute(query, values)
		self.connection.commit()

	'''
	SavePlayer
	Players are saved when new players with initial fields are created,
	and also after each login with fresh information (ip, nickname etc..)
	So it can be new or existing.
	fields are ( player.uuid, player.login, player.nickname, player.ip,
				player.ipv6, player.location, player.banned )
	'''
	def SavePlayer(self, cursor, fields):
		_id = 0
		if( fields[0] == 0 ) :
			# new player
			query = '''
				INSERT INTO %s
				(created, updated, login, nickname, ip, ipv6,location, banned)
				VALUES( NOW(), NOW(), %%s, %%s, %%s, %%s, %%s, %%s )
				''' % table_Players.tablename
			values = ( fields[1], fields[2], fields[3], fields[4], fields[5], fields[6] )
		else :
			# existing player
			query = '''
				UPDATE %s
				SET nickname=%%s, ip=%%s, ipv6=%%s, location=%%s
				WHERE id=%%s
				''' % table_Players.tablename
			values = ( fields[2], fields[3], fields[4], fields[5], fields[0] )
			
		cursor.execute( query, values )
		if( fields[0] == 0 ) :
			# fetch the new ID
			_id = self.getid(cursor)
			
		self.connection.commit()
		
		return _id
		
	####################################
	#
	#		USER LOGIN
	#
	####################################
	
	# user logins are intermediate handles to login-process
	def SaveUserLogin(self, cursor, handle, login, ready, valid, profile_url, profile_url_rml ):
		if( handle != 0 ) :
			# make an update
			query = '''
				UPDATE %s SET ready=%%s, valid=%%s, profile_url=%%s, profile_url_rml=%%s WHERE id=%%s
				''' % table_LoginPlayer.tablename
			values = ( ready, valid, profile_url, profile_url_rml, handle )
			cursor.execute( query, values )
		else :
			# insert new
			query = '''
				INSERT INTO %s ( created, login, ready, valid )
				VALUES( NOW(), %%s, %%s, %%s )
				''' % table_LoginPlayer.tablename
			values = ( login, ready, valid )
			cursor.execute( query, values )
			cursor.execute( 'SELECT LAST_INSERT_ID()' )
			r = cursor.fetchone()
			if( r ) :
				handle = r[0]

		self.connection.commit()
		return handle


	'''
	FIXME: we have to 
	'''
	# returns (ready, valid, login)		
	def GetUserLogin(self, cursor, handle):
		query = '''
			SELECT ready, valid, login, profile_url, profile_url_rml FROM %s
			WHERE id=%%s
			''' % table_LoginPlayer.tablename
		values = ( handle, )
		cursor.execute( query, values )
		r = cursor.fetchone()
		if( r ) :
			self.wmm.log( "GetUserLogin returning this: %s" % str(r) )
			return r
		
		# error
		return (1, 0, '', '', '')
	
	def RemoveUserLogin(self, cursor, handle):
		cursor.execute( '''
			DELETE FROM %s WHERE id=%%s
			''' % table_LoginPlayer.tablename,
			( handle, ) )

		self.connection.commit()
		
	####################################
	
	'''
	LoadUserStats
	if uuids is single integer, fetches 1 row of that user,
	if uuids is a list, fetches them all
	gametype is db id of wanted gametype
	returns dict of { uuid: stats } where stats is tuple of
	( db-id, wins, losses, quits, rating, deviation, gametype )
	where id is the DB index of the stat
	'''
	def LoadUserStats(self, cursor, uuids, gametypeId) :
		# if gametypeId == 0, we'd have to fetch the gametype name
		
		# base of query
		query = '''
				SELECT ps.player_id, ps.id, ps.wins, ps.losses, ps.quits, ps.rating, ps.deviation, gt.name
				FROM %(ps)s as ps, %(gt)s as gt
				''' % { 'ps': table_PlayerStats.tablename,
						'gt': table_Gametypes.tablename }
				
		# do we have a list of uuids or single uuid?
		if( isinstance( uuids, list ) ) :
			query += 'WHERE ps.player_id IN %s'
		elif( isinstance( uuids, (int, long) ) ) :
			query += 'WHERE ps.player_id=%s'
				
		# match gametype or spit out all gametypes?
		if( gametypeId != 0 ) :
			# match the gametype
			query += '''
				AND ps.gametype_id = %s
				AND gt.id = %s
				'''
			values = ( uuids, gametypeId, gametypeId )
		else :
			# all gametypes
			query += '''
				AND ps.gametype_id = gt.id
				'''
			values = ( uuids, )
		
		# finally execute
		cursor.execute( query, values )
		rows = cursor.fetchall()
		if( rows ) :
			d = {}
			for r in rows :
				# fix Decimals into floats
				# TypeError: 'tuple' object does not support item assignment
				# r[5] = float(r[5])
				# r[6] = float(r[6])
				# d[r[0]] = r[1:]
				d[r[0]] = ( r[1], r[2], r[3], r[4], float(r[5]), float(r[6])*0.001, r[7] )
			return d
			
		return None

	'''
	SaveUserStats
	for multiple users, stats is a dict of { uuid: fields }
	where fields are ( db-id, wins, losses, quits, rating, deviation )
	'''
	def SaveUserStats(self, cursor, stats, gametypeId):
		query_insert = '''
			INSERT INTO %s
			(created, updated, player_id, gametype_id, wins, losses, quits, rating, deviation)
			VALUES( NOW(), NOW(), %%s, %%s, %%s, %%s, %%s, %%s, %%s )
			''' % table_PlayerStats.tablename
		
		query_update = '''
			UPDATE %s SET
			updated=NOW(), wins=%%s, losses=%%s, quits=%%s, rating=%%s, deviation=%%s
			WHERE id=%%s
			''' % table_PlayerStats.tablename
		
		for uuid, fields in stats.iteritems() :
			if( uuid == 0 ) :
				continue
			
			if( fields[0] == 0 ) :
				query = query_insert
				# TypeError: can't multiply sequence by non-int of type 'float'
				values = ( uuid, gametypeId, fields[1], fields[2], fields[3],
						round(fields[4], 2), round(fields[5]*1000.0, 2) )
			else :
				query = query_update
				values = ( fields[1], fields[2], fields[3], round(fields[4], 2),
						round(fields[5]*1000.0, 2), fields[0] )
				
			cursor.execute( query, values )
			
		self.connection.commit()

	'''
	LoadLastMatches
	loads dates of last matches for a list of uuids,
	or a single uuid (uuids as integer)
	returns dict of { uuid: date }
	'''
	def LoadLastMatches(self, cursor, uuids, gametypeId) :
		if( isinstance( uuids, list ) ) :
			query = '''
			SELECT mp.player_id, mr.utctime
			FROM %(mp)s as mp, %(mr)s as mr
			WHERE mp.player_id in %%s
			AND mp.matchresult_id = mr.id
			AND mr.gametype_id = %%s
			ORDER BY mr.utctime DESC
			LIMIT 1;
			''' % { 'mp' : table_MatchPlayers.tablename,
					'mr' : table_MatchResults.tablename
					}
		elif( isinstance( uuids, (int, long) ) ) :
			query = '''
			SELECT mp.player_id, mr.utctime
			FROM %(mp)s as mp, %(mr)s as mr
			WHERE mp.player_id = %%s
			AND mp.matchresult_id = mr.id
			AND mr.gametype_id = %%s
			ORDER BY mr.utctime DESC
			LIMIT 1;
			''' % { 'mp' : table_MatchPlayers.tablename,
					'mr' : table_MatchResults.tablename
					}
		else :
			return None
		
		values = (uuids, gametypeId )
		
		cursor.execute( query, values )
		rows = cursor.fetchall()
		if( rows ) :
			d = {}
			for r in rows :
				d[r[0]] = r[1]
			return d
		
		return None

	'''
	LoadUserRatings
	return a map of { gametype : ( rating, deviation ) }
	(of all gametypes!)
	'''
	def LoadUserRatings(self, cursor, uuid) :
		# if gametypeId == 0, we'd have to fetch the gametype name
		
		# base of query
		query = '''
				SELECT ps.rating, ps.deviation, gt.name
				FROM %(ps)s as ps, %(gt)s as gt
				WHERE ps.player_id=%%s
				AND ps.gametype_id=gt.id
				''' % { 'ps': table_PlayerStats.tablename,
						'gt': table_Gametypes.tablename }
				
		values = ( uuid, )
		
		# finally execute
		cursor.execute( query, values )
		rows = cursor.fetchall()
		if( rows ) :
			d = {}
			for r in rows :
				# fix Decimals into floats
				# TypeError: 'tuple' object does not support item assignment
				# r[5] = float(r[5])
				# r[6] = float(r[6])
				# d[r[0]] = r[1:]
				d[r[2]] = ( float(r[0]), float(r[1])*0.001 )
			return d
			
		return None
	
	'''
	LoadUserLogin
	'''
	def LoadUserLogin(self, cursor, uuid):
		login = ''
		query = 'SELECT login FROM %s WHERE id=%%s' % table_Players.tablename
		values = ( uuid, )
		
		cursor.execute( query, values )
		r = cursor.fetchone()
		if( r ) :
			login = r[0]
			
		return login

	####################################
	#
	#			RACE
	#
	####################################

	# sector of -1 means the final time
	# doesnt check existance or records authenticity. caller should
	# do that via GetRaceRecordsBatch beforehand
	def AddRaceRun(self, cursor, server, player, mapId, times, offset):
		# first insert the racerun to get the ID
		query = '''
			INSERT INTO %s
			(created, server_id, player_id, map_id, utctime)
			VALUES(NOW(), %%s, %%s, %%s, DATE_SUB(NOW(), INTERVAL %d MICROSECOND))
			''' % (table_RaceRuns.tablename, offset*1000)
		values = ( server, player, mapId )
		cursor.execute( query, values )
		_id = self.getid(cursor)
		
		# now the rest of the runs
		for i in xrange( len(times) - 1 ) :
			query = '''
				INSERT INTO %s
				(created, run_id, sector, time)
				VALUES(NOW(), %%s, %%s, %%s)
			''' % table_RaceSectors.tablename
			values = ( _id, i, times[i] )
			cursor.execute( query, values )
			
		# final time has sector of -1
		query = '''
			INSERT INTO %s
			(created, run_id, sector, time)
			VALUES(NOW(), %%s, %%s, %%s)
		''' % table_RaceSectors.tablename
		values = ( _id, -1, times[-1] )
		cursor.execute( query, values )
		
		self.connection.commit()
	
	"""
	select 'sector_record' as `type`, s.sector, min(s.time) as `time`
	13:43 <@machinemessiah> from race_run r
	13:43 <@machinemessiah> left join race_sector s on (s.run_id=r.id)
	13:43 <@machinemessiah> where r.player_id=? and r.map_id=?
	13:43 <@machinemessiah> group by s.sector;

	13:44 <@machinemessiah> now if you want more records for the same player
	13:44 <@machinemessiah> in 1 query
	13:44 <@machinemessiah> I'd use unions
	13:44 <@machinemessiah> with different `type` values in the first col

	13:45 <@machinemessiah> then you can replace left join with inner join
	
	i need: <player_id, sector_id, time>
	for server+global i just need <sector_id, time>
	
	# batch of players (non-batched omit r.player_id from select and group by)
	select r.player_id, s.sector, min(s.time) as 'time'
	from race_runs r
	inner join race_sectors s on ( s.run_id=r.id )
	where r.player_id in (1,2,3)
	and r.map_id=3
	group by r.player_id,s.sector;
	
	# server
	select s.sector, min(s.time) as 'time'
	from race_runs r
	inner join race_sectors s on ( s.run_id=r.id )
	where r.server_id=? /* servers user id */
	and r.map_id=? /* maps id from mapnames */
	group by s.sector;
	
	# and global, omit the r.server_id=?
	
	# batching these ALL to the same, use union and name the columns
	SELECT '**record_type** as 'type', r.player_id ...
	UNION ALL
	SELECT '**record_type*' as 'type', 0 as player_id, s.sector ...
	UNION ALL
	SELECT '**....


	#####

	select 'player' as record_type, r.player_id, s.sector, min(s.time) as 'time'
		from race_runs r
		inner join race_sectors s on ( s.run_id = r.id )
		where r.player_id in (1,2,3) and r.map_id = %%s
		group by r.player_id, s.sector
	union all
	select 'server' as record_type, 0 as player_id, s.sector, min(s.time) as 'time'
		from race_runs r
		inner join race_sectors s on (s.run_id = r.id)
		where r.server_id = %%s and r.map_id = %%s
		group by s.sector
	union all
	select 'world' as record_type, 0 as player_id, s.sector, min(s.time) as 'time'
		from race_runs r
		inner join race_sectors s on (s.run_id = r.id)
		where r.map_id = %%s
		group by s.sector

	Can we put (select user_id from sessions_player where server_session=%%s) into
	where r.player_id in (..) clause?

	"""
	
	# Players can be a list or integer. List is interpreted as a list
	# of player uuids. Integer is interpreted as server uuid (NOTE!!)
	# mapId is a must. If server or bWorld is 0 then those records are not fetched.
	#
	# Returns a list of tuples that are of the form
	# 	(record_type, player_id, sector_id, time)
	# For global and world records, player_id is 0 (shouldnt be?)
	def GetRaceRecords(self, cursor, players, mapId, server, bWorld):
		args = []

		# First the player query, either None, integer or a list.
		# Integer refers to servers session id.
		query = ''
		if(isinstance(players, list)):
			query += """
				select 'player' as record_type, r.player_id, s.sector, min(s.time) as 'time'
				from race_runs r
				inner join race_sectors s on ( s.run_id = r.id )
				where r.player_id in %%s and r.map_id = %%s
				group by r.player_id, s.sector
			"""
			args.append(players)
			args.append(mapId)
		elif(isinstance(players, (int, long))):
			query += """
				select 'player' as record_type, r.player_id, s.sector, min(s.time) as 'time'
				from race_runs r
				inner join race_sectors s on ( s.run_id = r.id )
				where r.player_id in (
					select user_id
					from sessions_player
					where server_session = %%s
				) and r.map_id = %%s
				group by r.player_id, s.sector
			"""
			args.append(players)
			args.append(mapId)

		# Then the server query, either 0 or server uuid.
		if(server != 0):
			if(query):
				query += 'union all '
			query += """
				select 'server' as record_type, 0 as player_id, s.sector, min(s.time) as 'time'
				from race_runs r
				inner join race_sectors s on (s.run_id = r.id)
				where r.server_id = %%s and r.map_id = %%s
				group by s.sector
			"""
			args.append(server)
			args.append(mapId)

		# Finally the world query
		if(bWorld):
			if(query):
				query += 'union all '
			query += """
				select 'world' as record_type, 0 as player_id, s.sector, min(s.time) as 'time'
				from race_runs r
				inner join race_sectors s on (s.run_id = r.id)
				where r.map_id = %%s
				group by s.sector
			"""
			args.append(mapId)
		
		if(query):
			cursor.execute(query, values)
			rows = cursor.fetchall()
		else:
			rows = []

		return rows

	
	####################################
	#
	#			MATCH/GAME
	#
	####################################
	
	def GetMapnameId(self, cursor, mapname, may_insert=True):
		if( may_insert ) :
			# insert ignore into mapnames (mapname) values( mapname )
			query = 'INSERT IGNORE INTO %s ( mapname ) VALUES( %%s )' % table_Mapnames.tablename
			values = ( mapname, )
			cursor.execute( query, values )
			
			self.connection.commit()
			
		# now fetch the id
		query = 'SELECT id FROM %s WHERE mapname=%%s' % table_Mapnames.tablename
		values = ( mapname, )
		cursor.execute( query, values )
		r = cursor.fetchone()
		if( r ) :
			_id = r[0]
		else :
			_id = 0
			
		return _id
	
	def GetGametypeId(self, cursor, gametype):
		gametypeId = table_Gametypes.table.GetCertainID(cursor, gametype)
		
		# if we created new gametype
		# FIXME: we necessarily dont need to commit
		self.connection.commit()
		
		return gametypeId
	
	
	"""
	AddMatch
	Saves the whole Match structure to database
	"""
	def AddMatch(self, cursor, m, uuid):
		# we re-created the cursor and our tables need it
		table_Weapons.table.cursor = cursor
		table_Awards.table.cursor = cursor
		
		# TODO: precache weapons and awards
		# { name: id }
		cacheWeapons = {}
		cacheAwards = {}
		
		# sessionID -> matchplayer
		sid_matchplayer = {0:0}

		# Create the matchresult and fetch it's id
		query = """
				INSERT INTO %s
				(created, updated, server_id, gametype_id, `uuid`, instagib, teamgame, map_id,
				timelimit, scorelimit, gamedir, matchtime, utctime, demo_filename, winner_team, winner_player)
				VALUES (NOW(), NOW(), %%s, %%s, %%s, %%s, %%s, %%s, %%s, %%s, %%s, %%s, %%s, %%s, 0, 0)
				""" % table_MatchResults.tablename
		values = (	m.serverId, m.gameTypeId, uuid, m.instaGib, m.teamGame, m.mapId, m.timeLimit,
					m.scoreLimit, m.gamedir, m.timePlayed, datetime.datetime.utcnow(), m.demoFilename )
		
		cursor.execute( query, values )
		
		matchId = self.getid(cursor)
		
		# store all teams
		winnerTeam = 0
		if( m.teamGame ) :
			for team in m.teams.itervalues() :
				# Create the team object to database
				query = '''
						INSERT INTO %s
						(matchresult_id, name, score)
						VALUES(%%s, %%s, %%s)
						''' % table_MatchTeams.tablename
				values = (matchId, team.name, team.score)
				cursor.execute( query, values )
				_id = self.getid(cursor)
				team.teamId = _id
				if( team.index == m.winnerTeam ) :
					winnerTeam = _id
		
		# Store all the players
		winnerPlayer = 0			
		for player in m.players :
			teamId = 0
			if( m.teamGame and player.team in m.teams ) :
				teamId = m.teams[player.team].teamId
		
			query = """
					INSERT INTO %s
					(player_id, matchresult_id, matchteam_id, name, score, frags, deaths,
					teamkills, suicides, numrounds, ga_taken, ya_taken, ra_taken, mh_taken,
					uh_taken, quads_taken, shells_taken, bombs_planted, bombs_defused,
					flags_capped, matchtime, oldrating, newrating)
					VALUES (%%s, %%s, %%s, %%s, %%s, %%s, %%s, %%s, %%s, %%s, %%s, %%s, %%s,
					%%s, %%s, %%s, %%s, %%s, %%s, %%s, %%s, %%s, %%s)
					""" % table_MatchPlayers.tablename
			values = (player.uuid, matchId, teamId, player.name, player.score, player.frags,
					player.deaths, player.teamFrags, player.suicides, player.numRounds,
					player.gaTaken, player.yaTaken, player.raTaken, player.mhTaken, player.uhTaken,
					player.quadsTaken, player.shellsTaken, player.bombsPlanted, player.bombsDefused,
					player.flagsCapped, player.timePlayed, round(player.rating, 2), round(player.newRating, 2))
			
			cursor.execute( query, values )
			_id = self.getid(cursor)
			sid_matchplayer[player.sessionId] = _id
			if( player == m.winnerPlayer ) :
				winnerPlayer = _id
			
			# dont save weapons or awards for unregistered players
			# (except for our testing purposes)
			if( not config.alpha_phase and ( player.uuid == 0 or player.uuid < 0 ) ) :
				continue
			
			#### WEAPONS
			for weapon in player.weapons :
				if( weapon.name in cacheWeapons ) :
					wid = cacheWeapons[ weapon.name ]
				else :
					wid = table_Weapons.table.GetCertainID(cursor, weapon.name)
					cacheWeapons[ weapon.name ] = wid
					
				if( wid != 0 ) :
					query = """
							INSERT INTO %s
							(player_id, matchresult_id, weapon_id, 
							shots_strong, hits_strong, dmg_strong, frags_strong, acc_strong,
							shots_weak, hits_weak, dmg_weak, frags_weak, acc_weak)
							VALUES ( %%s, %%s, %%s, %%s, %%s, %%s, %%s, %%s, %%s, %%s, %%s, %%s, %%s ) 
							""" % table_MatchWeapons.tablename
					values = (player.uuid, matchId, wid,
							weapon.strongShots, weapon.strongHits, weapon.strongDmg, weapon.strongFrags, weapon.strongAcc,
							weapon.weakShots, weapon.weakHits, weapon.weakDmg, weapon.weakFrags, weapon.weakAcc)
					
					cursor.execute( query, values )
					
				else :
					self.wmm.log( "    - Invalid Weapon %s" % weapon.name)
					
			#### AWARDS
			for award in player.awards :
				if( award.name in cacheAwards ) :
					aid = cacheAwards[award.name]
				else :
					aid = table_Awards.table.GetCertainID(cursor, award.name)
					cacheAwards[ award.name ] = aid
					
				if( aid != 0 ) :
					query = """
							INSERT INTO %s
							(player_id, matchresult_id, award_id, count)
							VALUES( %%s, %%s, %%s, %%s )
							""" % table_MatchAwards.tablename
					values = (player.uuid, matchId, aid, award.count)
					
					cursor.execute( query, values )
				
				else :
					self.wmm.log( "    - Invalid Award %s" % award.name )
			
		# LOG FRAGS (again with the players)
		for player in m.players :
			attacker = sid_matchplayer[player.sessionId]
			for frag in player.logFrags :
				if( frag.weapon in cacheWeapons ) :
					wid = cacheWeapons[ frag.weapon ]
				else :
					wid = table_Weapons.table.GetCertainID(cursor, frag.weapon)
					cacheWeapons[ frag.weapon ] = wid
					
				victim = sid_matchplayer[frag.victim]
				query = """
						INSERT INTO %s
						(created, matchresult_id, attacker_id, victim_id, weapon_id, time)
						VALUES( NOW(), %%s, %%s, %%s, %%s, %%s )
						""" % table_MatchFrags.tablename
				values = (matchId, attacker, victim, wid, frag.time)
				
				cursor.execute( query, values )
				
		# fix the game winners
		if( winnerTeam or winnerPlayer ) :
			query = '''
					UPDATE %s
					SET winner_team=%%s, winner_player=%%s
					WHERE id=%%s
					''' % table_MatchResults.tablename
			values = ( winnerTeam, winnerPlayer, matchId )
			cursor.execute(query, values)
			
		self.connection.commit()

	####################################
	#
	#
	#
	####################################
		
	"""
	CreateTables
	Automatically creates all tables defined in given module, which should
	contain TableBase object that is parent to all declared tables.
	TableBase should declare following function
		Create(cursor, engine, charset)
	"""
	def CreateTables(self, cursor, module, engine, charset, create_to_db ):
		for name in dir(module) :
			o = getattr ( module, name )
			try:
				if ( type(o) == type and o != module.TableBase and issubclass(o, module.TableBase) ) :
					newobj = o ()
					if ( not newobj.tablename in self.tables ) :
						# print ( newobj )
						if( create_to_db ) :
							newobj.Create (cursor, engine, charset)
						self.tables[newobj.tablename] = newobj
					self.wmm.log('CreateTables created %s %s' % (name, o.table))
			except TypeError as e:
				self.wmm.log('CreateTables exception %s' % str(e))
				pass

##################################################

# This class wraps public database functions so that
# each one is wrapped inside try/except and also each
# function gets invidual cursor created from the pool
# which is destroyed automatically upon function exit

class DatabaseWrapper:
	def __init__(self, wmm, host, user, passwd, db, engine=None, charset=None):
		self.obj = DatabaseHandler(wmm, host, user, passwd, db, engine, charset)
		self.lock = threading.Lock()
		atexit.register(self._releaselock_)
	
	def _releaselock_(self):
		self.lock.release()

	def verifyconnection(self):
		numtries = 10
		while(numtries):
			try:
				cursor = self.obj.connection.cursor()
				cursor.execute('select 1')
				cursor.close()
				return True
			except:
				# Re-establish the connection
				self.obj.open()
				numtries-=1
		return False

	def __getattr__(self, name):
		attr = getattr(self.obj, name)

		def f(*args, **kwargs):
			self.lock.acquire(True)
			#self.obj.wmm.log('--> %s (%s %s) thread %d' % (name,
			#	inspect.stack()[2][3], inspect.stack()[3][3],
			#	threading.current_thread().ident))
			cursor = None
			r = None
			if(not self.verifyconnection()):
				self.obj.wmm.log('DatabaseWrapper failed to verify connection')
				self.lock.release()
				return

			try:
				cursor = self.obj.connection.cursor()
				r = attr(cursor, *args, **kwargs)
			except Exception as e:
				self.obj.wmm.log('DatabaseWrapper exception (%s) %s' % (name, str(e)))
			finally:
				if(cursor != None):
					cursor.close()
				self.lock.release()
			return r

		if(type(attr) == types.FunctionType or type(attr) == types.MethodType):
			return f
		else:
			return attr


