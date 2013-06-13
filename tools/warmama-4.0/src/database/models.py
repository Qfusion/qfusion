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
#########################

# WMM imports
# import wmlib

##########################

_alltables = {}

# TODO: remove and put to database
# create tables inheriting from TableObject from
# given module with given cursor
# (refactor this -> userdb ?
# also input the engine which can be appended to Create parameters
def CreateTables (cursor, module, engine=None, charset=None):

	# _alltables = {}
	
	for name in dir(module) :
		o = getattr ( module, name )
		try:
			if ( o != TableBase and issubclass(o, TableBase) ) :
				newobj = o ()
				if ( not newobj.tablename in _alltables ) :
					# print ( newobj )
					newobj.Create (cursor, engine, charset)
					_alltables[newobj.tablename] = newobj
		except TypeError :
			pass
			
#####################

# Base object for tables
class TableBase (object):
	
	tablename = ''
	schema = ''
	table = None
	
	INVALID_ID = 0
	
	def __init__ (self):
		# TableBase.table = self
		self.cursor = None

	# add engine here (and charset?)
	def Create (self, cursor, engine="InnoDB", charset="utf8_general_ci"):
		self.cursor = cursor
		s = "CREATE TABLE IF NOT EXISTS %s (%s)" % ( self.tablename, self.schema )
		if ( engine ) :
			s += " ENGINE=%s" % engine
		if ( charset ) :
			s += " DEFAULT CHARSET=%s" % charset
		cursor.execute ( s )
	
	#########################
	#
	# 	SQL helpers
	#
	#########################
	
	# s is the columns and values (col,col) VALUES(%s,%s)
	def insert (self, s, args):
		cmd = "INSERT INTO %s %s" % ( self.tablename, s )
		return self.cursor.execute ( cmd, args )
		
	# like insert, but return the id column of newly inserted row
	def insertid (self, s, args ):
		cmd = "INSERT INTO %s %s" % ( self.tablename, s )
		self.cursor.execute ( cmd, args )
		self.cursor.execute ( 'SELECT LAST_INSERT_ID()' )
		r = self.cursor.fetchone()
		if ( r ) :
			return r[0]
		
		return self.INVALID_ID 
	
	# s defines the columns you want as name,name,.. or *
	def select (self, s):
		cmd = "SELECT %s FROM %s" % ( s, self.tablename )
		self.cursor.execute ( cmd )
		try :
			r = self.cursor.fetchall ()
			return r
		except:
			return None
	
	# args is tuple as in DB-API normally
	# you dont need to write WHERE
	# s is the column values col,col,col or *
	def select2 (self, s, where, args ):
		cmd = "SELECT %s FROM %s WHERE %s" % ( s, self.tablename, where )
		self.cursor.execute ( cmd, args )
		try :
			r = self.cursor.fetchall ()
			return r
		except:
			return None 
		
	# s is the column values col,col,col or *
	def selectid (self, s, id ) :
		cmd = "SELECT %s FROM %s WHERE id=%%s" % (s, self.tablename )
		self.cursor.execute ( cmd, (id,) )
		try :
			r = self.cursor.fetchall ()
			return r
		except:
			return None
	
	# args is tuple as in DB-API normally
	# you dont need to write WHERE
	# s is the column values col,col,col or *
	def update(self, s, where, args):
		cmd = "UPDATE %s SET %s WHERE %s" % (self.tablename, s, where)
		return self.cursor.execute( cmd, args )


######################################

class table_PlayerStats (TableBase):
	
	tablename = 'player_stats'
	schema = """
	id INTEGER(11) NOT NULL AUTO_INCREMENT,
	player_id INTEGER(11),
	created DATETIME NOT NULL,
	updated DATETIME NOT NULL,
	gametype_id INTEGER(11),
	wins INTEGER,
	losses INTEGER,
	quits INTEGER,
	rating DECIMAL(8,2),
	deviation DECIMAL(8,2),
	PRIMARY KEY(id),
	UNIQUE KEY player_gametype(player_id,gametype_id),
	KEY updated(updated)
	"""
	table = None
	
	def __init__ (self):
		TableBase.__init__(self)
		table_PlayerStats.table = self
		
class table_PlayerAchievements (TableBase):
	
	tablename = 'player_achievements'
	schema = """
	id INTEGER(11) NOT NULL AUTO_INCREMENT,
	player_id INTEGER(11),
	created DATETIME NOT NULL,
	updated DATETIME NOT NULL,
	achievement_id INTEGER(11),
	PRIMARY KEY(id),
	KEY player_id(player_id)
	"""
	table = None
	
	def __init__ (self):
		TableBase.__init__(self)
		table_PlayerAchievements.table = self
		
		
################################################
		
# ch : Duels and DA's now come up as non-teamgames
# from the gameserver
# FIXME: if unregistered player wins a non-team game, the 
# winner_player in matchresults will be 0 (INVALID_ID)
# TODO: add scorelimit
class table_MatchResults (TableBase):
	
	tablename = 'match_results'
	schema = """
	id INTEGER(11) NOT NULL AUTO_INCREMENT,
	created DATETIME NOT NULL,
	updated DATETIME NOT NULL,
	server_id INTEGER(11),
	gametype_id INTEGER(11),
	`uuid` CHAR(36),
	instagib TINYINT(1),
	teamgame TINYINT(1),
	map_id INTEGER(11),
	timelimit INTEGER,
	scorelimit INTEGER,
	gamedir VARCHAR(64),
	matchtime INTEGER,
	utctime DATETIME NOT NULL,
	winner_team INTEGER(11),
	winner_player INTEGER(11),
	demo_filename VARCHAR(128) NOT NULL DEFAULT '',
	PRIMARY KEY(id),
	KEY server_id(server_id),
	KEY gametype_id(gametype_id),
	KEY created(created),
	UNIQUE KEY `uuid`(`uuid`),
	KEY winner_team(winner_team),
	KEY winner_player(winner_player)
	"""
	table = None
	
	def __init__ (self):
		TableBase.__init__(self)
		table_MatchResults.table = self
		
# TODO: add dmg_given, dmg_taken, health/armor_taken?
class table_MatchPlayers (TableBase):

	tablename = 'match_players'
	schema = """
	id INTEGER(11) NOT NULL AUTO_INCREMENT,
	player_id INTEGER(11),
	matchresult_id INTEGER(11),
	matchteam_id INTEGER(11),
	name VARCHAR(64),
	score INTEGER,
	frags INTEGER,
	deaths INTEGER,
	teamkills INTEGER,
	suicides INTEGER,
	numrounds INTEGER,
	ga_taken INTEGER,
	ya_taken INTEGER,
	ra_taken INTEGER,
	mh_taken INTEGER,
	uh_taken INTEGER,
	quads_taken INTEGER,
	shells_taken INTEGER,
	bombs_planted INTEGER,
	bombs_defused INTEGER,
	flags_capped INTEGER,
	matchtime INTEGER,	
	oldrating DECIMAL(8,2),
	newrating DECIMAL(8,2),
	PRIMARY KEY(id),
	KEY player_id(player_id),
	KEY matchresult_id(matchresult_id),
	KEY matchteam_id(matchteam_id)
	"""	
	table = None
	
	def __init__ (self):
		TableBase.__init__(self)
		table_MatchPlayers.table = self
		
class table_MatchTeams (TableBase):
	
	tablename = 'match_teams'
	schema = """
	id INTEGER(11) NOT NULL AUTO_INCREMENT,
	matchresult_id INTEGER(11),
	name VARCHAR(64),
	score INTEGER,
	PRIMARY KEY(id),
	KEY matchresult_id(matchresult_id)
	"""
	table = None
	
	def __init__ (self):
		TableBase.__init__(self)
		table_MatchTeams.table = self
		
class table_MatchAwards (TableBase):
	
	tablename = 'match_awards'
	schema = """
	id INTEGER(11) NOT NULL AUTO_INCREMENT,
	player_id INTEGER(11),
	matchresult_id INTEGER(11),
	award_id INTEGER(11),
	count INTEGER,
	PRIMARY KEY(id),
	KEY player_id(player_id),
	KEY matchresult_id(matchresult_id),
	KEY award_id(award_id)
	"""
	table = None
	
	def __init__ (self):
		TableBase.__init__(self)
		table_MatchAwards.table = self
		
class table_MatchWeapons (TableBase):
	
	tablename = 'match_weapons'
	schema = """
	id INTEGER(11) NOT NULL AUTO_INCREMENT,
	player_id INTEGER(11),
	matchresult_id INTEGER(11),
	weapon_id INTEGER(11),
	shots_strong INTEGER,
	hits_strong INTEGER,
	dmg_strong INTEGER,
	frags_strong INTEGER,
	acc_strong DECIMAL(5,2),
	shots_weak INTEGER,
	hits_weak INTEGER,
	dmg_weak INTEGER,
	frags_weak INTEGER,
	acc_weak DECIMAL(5,2),
	PRIMARY KEY(id),
	KEY player_id(player_id),
	KEY matchresult_id(matchresult_id),
	KEY weapon_id(weapon_id)
	"""
	table = None
	def __init__ (self):
		TableBase.__init__(self)
		table_MatchWeapons.table = self
		
class table_MatchFrags( TableBase ):
	
	tablename = 'frag_log'
	schema = '''
	id INTEGER(10) NOT NULL AUTO_INCREMENT,
	created DATETIME NOT NULL,
	matchresult_id INTEGER(11),
	attacker_id INTEGER(11),
	victim_id INTEGER(11),
	weapon_id INTEGER(11),
	time INTEGER(11),
	PRIMARY KEY(id),
	KEY matchresult_id (matchresult_id),
	KEY attacker_id (attacker_id),
	KEY victim_id (victim_id),
	KEY weapon_id (weapon_id)
	'''
	table = None
	
	def __init__(self):
		TableBase.__init__(self)
		table_MatchFrags.table = self
		
#####################################

# TODO: cache table rows
class table_Weapons (TableBase):
	
	tablename = 'weapons'
	schema = """
	id INTEGER(11) NOT NULL AUTO_INCREMENT,
	name CHAR(2),
	fullname VARCHAR(16),
	PRIMARY KEY (id),
	UNIQUE KEY name(name)
	"""
	table = None
	
	weapnames = { 	"gb" : "Gunblade", 
					"mg" : "Machinegun", 
					"rg" : "Riotgun",
					"gl" : "Grenade Launcher", 
					"rl" : "Rocket Launcher",
					"pg" : "Plasmagun",
					"lg" : "Lasergun",
					"eb" : "Electrobolt",
					"ig" : "Instagun" }
		
	def __init__ (self):
		TableBase.__init__(self)
		table_Weapons.table = self
		
	# this returns id for given weapname
	# if its not in the table, create it
	# and return the new ID
	def GetCertainID (self, cursor, name):
		q = "LOCK TABLES %s WRITE" % self.tablename
		cursor.execute ( q )
		
		q = "SELECT id FROM %s WHERE name=%%s LIMIT 1" % self.tablename
		
		cursor.execute ( q, (name) )
		if ( cursor.rowcount > 0 ) :
			r = cursor.fetchone();
		else :
			# we have to add this one, try to figure out
			# known qualified fullnames
			fullname = ''
			if ( name in self.weapnames ) :
				fullname = self.weapnames[name]
			else :
				fullname = name
			q = "INSERT INTO %s (name,fullname) VALUES(%%s,%%s)" % self.tablename
			cursor.execute ( q, (name, fullname))
			cursor.execute ( "SELECT LAST_INSERT_ID()")
			r = cursor.fetchone()
		
		q = "UNLOCK TABLES"
		cursor.execute ( q )
		
		if ( r != None ) :
			return r[0]
		
		return 0
	

# TODO: tag name as unique and precreate the
# fields before filling players from match
# TODO: cache table rows	 
class table_Awards (TableBase):
	
	tablename = 'awards'
	schema = """
	id INTEGER(11) NOT NULL AUTO_INCREMENT,
	name VARCHAR(64),
	PRIMARY KEY (id),
	UNIQUE KEY name(name)
	"""
	table = None
	
	def __init__ (self):
		TableBase.__init__(self)
		table_Awards.table = self
		
		
	# this returns id for given weapname
	# if its not in the table, create it
	# and return the new ID
	def GetCertainID (self, cursor, name):
		q = "LOCK TABLES %s WRITE" % self.tablename
		cursor.execute ( q )
		
		q = "SELECT id FROM %s WHERE name=%%s LIMIT 1" % self.tablename
		
		cursor.execute ( q, (name) )
		if ( cursor.rowcount > 0 ) :
			r = cursor.fetchone()
		else :
			# not in the table, we have to add this one
			q = "INSERT INTO %s (name) VALUES(%%s)" % self.tablename
			cursor.execute ( q, (name) )
			cursor.execute ( "SELECT LAST_INSERT_ID()")
			r = cursor.fetchone()
		
		q = "UNLOCK TABLES"
		cursor.execute ( q )
		
		if ( r != None ) :
			return r[0]
		
		return 0
	

# TODO: internal fields for name of gamaward
# and the required amount of them to achieve
# given achievement
# TODO: cache table rows
class table_Achievements (TableBase):
	
	tablename = 'achievements'
	schema = """
	id INTEGER(11) NOT NULL AUTO_INCREMENT,
	name VARCHAR(64),
	description VARCHAR(128),
	numgotten INTEGER,
	PRIMARY KEY (id)
	"""
	table = None
	
	def __init__ (self):
		TableBase.__init__(self)
		table_Achievements.table = self
		
# used for RACE now, TODO: use for MatchResult too
class table_Mapnames(TableBase):
	tablename = 'mapnames'
	schema = """
	id INTEGER(11) NOT NULL AUTO_INCREMENT,
	mapname VARCHAR(64),
	PRIMARY KEY(id),
	UNIQUE(mapname)
	"""
	table = None
	
	def __init__(self):
		TableBase.__init__(self)
		table_Mapnames.table = self

class table_RaceSectors(TableBase):
	tablename = 'race_sectors'
	schema = """
	id INTEGER(11) NOT NULL AUTO_INCREMENT,
	created DATETIME NOT NULL,
	run_id INTEGER(11),
	sector INTEGER,
	time INTEGER(11),
	PRIMARY KEY(id),
	KEY run_id(run_id),
	KEY time(time)
	"""
	table = None
	
	def __init__(self):
		TableBase.__init__(self)
		table_RaceSectors.table = self
		
class table_RaceRuns(TableBase):
	
	tablename = 'race_runs'
	schema = """
	id INTEGER(11) NOT NULL AUTO_INCREMENT,
	created DATETIME NOT NULL,
	map_id INTEGER(11),
	server_id INTEGER(11),
	player_id INTEGER(11),
	utctime DATETIME NOT NULL,
	PRIMARY KEY (id),
	KEY map_id(map_id),
	KEY server_id(server_id),
	KEY player_id(player_id),
	KEY created(created)
	"""
	table = None
	
	def __init__(self):
		TableBase.__init__(self)
		table_RaceRuns.table = self
		
# TODO: tag name as unique and precreate the
# fields before filling players from match
# TODO: cache table rows
class table_Gametypes (TableBase):
	
	tablename = 'gametypes'
	schema = """
	id INTEGER(11) NOT NULL AUTO_INCREMENT,
	name VARCHAR(16),
	description VARCHAR(32),
	PRIMARY KEY (id),
	UNIQUE KEY name(name)
	"""
	table = None
	
	gametypenames = {	'dm':'Deathmatch',
						'ffa': 'Free For All',
						'duel' : 'Duel',
						'tdm' : 'Team Deathmatch',
						'ctf' : 'Capture The Flag',
						'race' : 'Race',
						'ca' : 'Clan Arena',
						'bomb' : 'Bomb & Defuse',
						'ctftactics' : 'Capture The Flag Tactics',
						'headhunt' : 'Headhunt',
						'tdo' : 'Team Domination',
						'da' : 'Duel Arena',
					}
	
	def __init__ (self):
		TableBase.__init__(self)
		table_Gametypes.table = self
		
	# this returns id for given weapname
	# if its not in the table, create it
	# and return the new ID
	def GetCertainID (self, cursor, name):
		q = "LOCK TABLES %s WRITE" % self.tablename
		cursor.execute ( q )
		
		q = "SELECT id FROM %s WHERE name=%%s LIMIT 1" % self.tablename
		cursor.execute ( q, (name) )
		if ( cursor.rowcount > 0 ) :
			r = cursor.fetchone()
		else :
			# we have to add this one, try to figure out
			# known qualified fullnames
			fullname = ''
			if ( name in self.gametypenames ) :
				fullname = self.gametypenames[name]
			else :
				fullname = name
			q = "INSERT INTO %s (name,description) VALUES(%%s,%%s)" % self.tablename
			cursor.execute ( q, (name, fullname))
			cursor.execute ( "SELECT LAST_INSERT_ID()")
			r = cursor.fetchone()
		
		q = "UNLOCK TABLES"
		cursor.execute ( q )
		
		if ( r != None ) :
			return r[0]
		
		return 0
	

##############################
		
# vs. old USER_PLAYER table
class table_Players (TableBase):
	
	tablename = 'players'
	schema = """
	id INTEGER(11) NOT NULL AUTO_INCREMENT,
	created DATETIME NOT NULL,
	updated DATETIME NOT NULL,
	login VARCHAR(64),
	nickname VARCHAR(64),
	ip VARCHAR(22),
	ipv6 VARCHAR(54),
	location CHAR(2),
	banned TINYINT(1),
	PRIMARY KEY (id),
	UNIQUE KEY login(login),
	KEY location(location)
	"""
	table = None
	
	def __init__ (self):
		TableBase.__init__(self)
		table_Players.table = self
		
	# returns the UID of the new user
	def AddNew (self, login, secretkey, nickname, ip, location):
		
		self.insert ( """
			(login, secretkey, nickname, ip, location, created, updated)
			VALUES (%s, %s, %s, %s, %s, NOW(), NOW())
			""", (login, secretkey, nickname, ip, location) )
				
		self.cursor.execute ( "SELECT LAST_INSERT_ID()" )
		row = self.cursor.fetchone()
		if ( row ) :
			# print ( "Created new player with id %d" % row[0] )
			return row[0]
		
		return None
	
class table_Servers (table_Players):
	
	tablename = 'servers'
	schema = """
	id INTEGER(11) NOT NULL AUTO_INCREMENT,
	created DATETIME NOT NULL,
	updated DATETIME NOT NULL,
	login VARCHAR(64),
	regip VARCHAR(22),
	regipv6 VARCHAR(54),
	hostname VARCHAR(64),
	ip VARCHAR(22),
	ipv6 VARCHAR(54),
	location CHAR(2),
	banned TINYINT(1),
	demos_baseurl VARCHAR(128) NOT NULL DEFAULT '',
	PRIMARY KEY (id),
	UNIQUE KEY login(login),
	KEY location(location),
	KEY ip(ip),
	KEY regip(regip)
	"""
	table = None
	
	def __init__ (self):
		TableBase.__init__(self)
		table_Servers.table = self
		
########################################

# NOTE THAT SESSION-ID'S ARE 10 LENGTH
# thats because we send them to gameserver that holds
# them as signed 32-bit integers

class table_SessionsServer( TableBase ):
	
	tablename = 'sessions_server'
	schema = '''
	id INTEGER(10) NOT NULL AUTO_INCREMENT,
	created DATETIME NOT NULL,
	updated DATETIME NOT NULL,
	user_id INTEGER(11),
	ip VARCHAR(22),
	ipv6 VARCHAR(54),
	digest VARCHAR(32),
	port INTEGER,
	next_match_uuid CHAR(36) NULL,
	PRIMARY KEY(id),
	KEY created(created),
	KEY ip(ip),
	KEY user_id(user_id),
	UNIQUE KEY next_match_uuid(next_match_uuid)
	'''
	table = None
	
	def __init__ (self):
		TableBase.__init__(self)
		table_SessionsServer.table = self

class table_SessionsPlayer( TableBase ):
	
	tablename = 'sessions_player'
	schema = '''
	id INTEGER(10) NOT NULL AUTO_INCREMENT,
	created DATETIME NOT NULL,
	updated DATETIME NOT NULL,
	user_id INTEGER(11),
	ip VARCHAR(22),
	ipv6 VARCHAR(54),
	digest VARCHAR(32),
	ticket_id INTEGER(10),
	ticket_server INTEGER(10),
	ticket_expiration DATETIME,
	server_session INTEGER(10),
	purgable TINYINT(1),
	PRIMARY KEY(id),
	KEY user_id(user_id),
	KEY ticket_id(ticket_id),
	KEY ticket_server(ticket_server),
	KEY server_session(server_session),
	KEY created(created)
	'''
	table = None
	
	def __init__ (self):
		TableBase.__init__(self)
		table_SessionsPlayer.table = self

class table_PurgePlayers( TableBase ):
	
	tablename = 'purge_players'
	schema = '''
	id INTEGER(11) NOT NULL AUTO_INCREMENT,
	created DATETIME NOT NULL,
	updated DATETIME NOT NULL,
	session_id INTEGER(10),
	player_id INTEGER(11),
	server_session INTEGER(10),
	PRIMARY KEY(id),
	KEY session_id(session_id),
	KEY player_id(player_id),
	KEY server_session(server_session)
	'''
	table = None
	
	def __init__ (self):
		TableBase.__init__(self)
		table_PurgePlayers.table = self
		
class table_LoginPlayer( TableBase ):
	
	tablename = 'login_players'
	schema = '''
	id INTEGER(10) NOT NULL AUTO_INCREMENT,
	created DATETIME NOT NULL,
	login VARCHAR(64),
	ready TINYINT(1),
	valid TINYINT(1),
	profile_url VARCHAR(255) DEFAULT NULL,
	profile_url_rml VARCHAR(255) DEFAULT NULL,	
	PRIMARY KEY(id)
	'''
	table = None
	
	def __init__(self):
		TableBase.__init__(self)
		table_LoginPlayer.table = self
	
########################################

if __name__ == '__main__' :
	
	import config
	import MySQLdb
	import sys
	
	engine = config.db_engine
	charset = config.db_charset
	
	connection = MySQLdb.connect ( host = config.db_host,
									user = config.db_user,
									passwd = config.db_passwd,
									db = config.db_name )
	
	cursor = connection.cursor ()
	
	CreateTables (cursor, sys.modules[__name__], engine, charset)
