#!/usr/bin/env python2.7
#-*- coding:utf-8 -*-

"""
Created on 15.2.2011
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
"""

###################
#
# Imports

# WMM library
import config
import skills
import session.users
import warmama

# Standard library
import math
import json
import datetime

###################
#
# Constants
def safeint(s) : return warmama.safeint(s)
def safebool(s) : return warmama.safebool(s)

###################
#
# Globals

# hmm.. pls FIXME someday.
g_weaponNames = [ "gb", "mg", "rg", "gl", "rl", "pg", "lg", "eb", "ig",
				"gb", "mg", "rg", "gl", "rl", "pg", "lg", "eb", "ig"
				]

def getWeaponName( idx ):
	if( idx < 0 or idx >= len( g_weaponNames ) ) :
		return ""
	
	return g_weaponNames[ idx ]

###################
#
# Helpers

###################
#
# Classes

class MatchFrag(object):
	
	def __init__(self):
		self.victim = 0
		self.weapon = 0
		self.time = 0
		
class MatchWeapon(object):
	
	def __init__(self, name=""):
		self.name = name
		self.strongHits = 0
		self.strongShots = 0
		self.strongAcc = 0
		self.strongDmg = 0
		self.strongFrags = 0
		self.weakHits = 0
		self.weakShots = 0
		self.weakAcc = 0
		self.weakDmg = 0
		self.weakFrags = 0
		self.weaponId = 0
		
	def CalculateAccuracies(self):
		# FIXME: few issues here, like gb showing hits with zero shots
		
		# (hits > 0 ? ((hits) == (shots) ? 100 : (min( (int)( floor( ( 100.0f*(hits) ) / ( (float)(shots) ) + 0.5f ) ), 99 ))) : 0);
		if( self.strongHits > 0 ) :
			if( self.strongHits >= self.strongShots ) :
				self.strongAcc = 100
			else :
				self.strongAcc = min(math.floor( (100.0*self.strongHits) / self.strongShots + 0.5), 99)
		else:
			self.strongAcc = 0
			
		if( self.weakHits > 0 ) :
			if( self.weakHits >= self.weakShots ) :
				self.weakAcc = 100
			else :
				self.weakAcc = min(math.floor( (100.0*self.weakHits) / self.weakShots + 0.5), 99)
		else:
			self.weakAcc = 0
		
	"""
	ValidateFields
	returns False if any field is None
	"""
	def ValidateFields(self):
		if( self.strongHits == None ) : return "strongHits"
		if( self.strongShots == None ) : return "strongShots"
		if( self.strongAcc == None ) : return "strongAcc"
		if( self.strongDmg == None ) : return "strongDmg"
		if( self.strongFrags == None ) : return "strongFrags"
		if( self.weakHits == None ) : return "weakHits"
		if( self.weakShots == None ) : return "weakShots"
		if( self.weakAcc == None ) : return "weakAcc"
		if( self.weakDmg == None ) : return "weakDmg"
		if( self.weakFrags == None ) : return "weakFrags"

		return None

########################

class MatchAward(object):
	
	def __init__(self, name="", count=0):
		self.name = name
		self.count = count
		self.awardId = 0

	"""
	ValidateFields
	returns False if any field is None
	"""
	def ValidateFields(self):
		if( self.count == None ) : return "count"
		
		return None
		
########################

class MatchPlayer(object):
	
	def __init__(self):
		
		# stuff that we need to use much
		self.name = ''
		self.score = 0
		self.timePlayed = 0
		self.final = False
		
		# -1 loss, 0 quit, 1 win
		self.outCome = 0
		
		self.team = -1
		self.teamId = 0
		self.frags = 0
		self.deaths = 0
		self.suicides = 0
		self.numRounds = 0
		self.teamFrags = 0
		self.dmgGiven = 0
		self.dmgTaken = 0
		self.healthTaken = 0
		self.armorTaken = 0
		# new pickup counts
		self.gaTaken = 0
		self.yaTaken = 0
		self.raTaken = 0
		self.mhTaken = 0
		self.uhTaken = 0
		self.quadsTaken = 0
		self.shellsTaken = 0
		self.bombsPlanted = 0
		self.bombsDefused = 0
		self.flagsCapped = 0
		
		self.weapons = []
		self.awards	= []
		self.logFrags =[]
		
		self.sessionId = 0
		
		# pulled from DB or cached Player
		self.uuid = 0
		self.rating = skills.DEFAULT_RATING
		self.deviation = skills.DEFAULT_DEVIATION
		self.stats = None
		
		# stuff written by skills
		self.rank = 0			# position on scoreboard (may be shared positions) (0 best)
		self.newRating = skills.DEFAULT_RATING		# calculated new rating
		self.newDeviation = skills.DEFAULT_DEVIATION	# calculated new deviation

	"""
	ValidateFields
	returns False if any field is None
	"""
	def ValidateFields(self):
		
		if( self.name == None ) : return "name"
		if( self.score == None ) : return "score" 
		if( self.timePlayed == None ) : return "timePlayed"
		if( self.final == None ) : return "final"
		
		# -1 loss, 0 quit, 1 win
		if( self.outCome == None ) : return "outCome"
		
		if( self.team  == None ) : return "team"
		if( self.frags == None ) : return "frags"
		if( self.deaths == None ) : return "deaths"
		if( self.suicides == None ) : return "suicides"
		if( self.numRounds == None ) : return "numrounds"
		if( self.teamFrags == None ) : return "teamFrags"
		if( self.dmgGiven == None ) : return "dmgGiven"
		if( self.dmgTaken == None ) : return "dmgTaken"
		if( self.healthTaken == None ) : return "healthTaken"
		if( self.armorTaken == None ) : return "armorTaken"
		
		for weap in self.weapons :
			s = weap.ValidateFields()
			if( s != None ) :
				return "weap.%s" % s
			
		for award in self.awards :
			s = award.ValidateFields()
			if( s != None ) :
				return "award.%s" % s
			
		return None
	
########################

class MatchTeam(object):
	
	def __init__(self, score=0, name='', index=0):
		self.score = score
		self.name = name
		self.index = index
		self.teamId = 0
		
	def ValidateFields(self):
		if( self.score == None ) : return "score"
		if( self.name == None ) : return "name"
		if( self.index == None ) : return "index"
		
		return None
	
########################

class MatchRacerun(object):
	
	def __init__(self, sessionId, timestamp, times):
		self.sessionId = sessionId
		self.timestamp = timestamp
		self.times = times
		
		self.uuid = 0
		
	def ValidateFields(self):
		if( self.sessionId == None ) : return "sessionId"
		if( self.times == None ) : return "times"

		return None
	
########################

class Match(object):
	
	def __init__(self, serverId=0):
		
		self.serverId = serverId
		
		self.gameType = ''
		self.gameTypeId = 0
		self.hostName = ''
		self.teamGame = False
		self.mapName = ''
		self.mapId = 0
		self.timePlayed = 0
		self.timeLimit = 0
		self.scoreLimit = 0
		self.instaGib = False
		self.raceGame = False
		self.timestamp = 0
		self.gamedir = ''
		self.demoFilename = ''

		# matchId		# temporary match_id ?
		
		self.teams = {}			# MatchTeam [team.index]
		self.winnerTeam = ''	# teamname
		self.winnerTeamId = 0
		
		self.players = []		# MatchPlayer
		self.winnerPlayer = None	# pointer to winning MatchPlayer
		self.winnerPlayerId = 0
		
		self.raceRuns = []		# MatchRacerun

	"""
	ValidateFields
	returns False if any field is None
	"""
	def ValidateFields(self):
		if( self.gameType == None ) : return "gameType"
		if( self.hostName == None ) : return "hostName"
		if( self.teamGame == None ) : return "teamGame"
		if( self.mapName == None ) : return "mapName"
		if( self.timePlayed == None ) : return "timePlayed"
		if( self.timeLimit == None ) : return "timeLimit"
		if( self.scoreLimit == None ) : return "scoreLimit"
		if( self.instaGib == None ) : return "instaGib"
		if( self.raceGame == None ) : return "raceGame"
		if( self.timestamp == None ) : return "timestamp"
		if( self.gamedir == None ) : return "gamedir"
		if( self.demoFilename == None ) : return "demoFilename"
		
		for team in self.teams.itervalues() :
			s = team.ValidateFields()
			if( s != None ) :
				return "team.%s" % s
			
		for player in self.players :
			s = player.ValidateFields()
			if( s != None ) :
				return "player.%s" % s
			
		for run in self.raceRuns :
			s = run.ValidateFields()
			if( s != None ) :
				return "racerun.%s" % s
			
		return None

#################################################

"""
MatchHandler receives matchresults and stores them and player statistics
through DatabaseHandler @see DatabaseHandler
"""
class MatchHandler(object):
	
	def __init__(self, mm):
		self.mm = mm
		self.db = mm.dbHandler
	
	"""
	listSkills
	Returns a list of tuples of (sessionId, rating)
	of all the registered players rating in the game
	"""
	def listSkills(self, m):
		ls = []
		
		for player in m.players :
			if( player.sessionId > 0 ) :
				ls.append( ( player.sessionId, player.newRating) )
				
		return ls
	

	"""
	prepareMatch
	checks that all fields are set, fetches missing UIDS, calculates skills
	"""
	def prepareMatch(self, m):
		
		# TODO: fix ragequitting when players are tied in duel
		# that comes out as 'tie' in the skillrating. Maybe hack
		# the scores if something like that happens?
		missing = m.ValidateFields()
		if( missing != None ) :
			self.mm.log( "ERROR: prepareMatch: Missing field in match.%s" % missing )
			return False
		
		m.gameTypeId = self.mm.dbHandler.GetGametypeId( m.gameType )
		m.mapId = self.mm.dbHandler.GetMapnameId( m.mapName )
		
		# Handle RACE specially
		if( m.raceGame or len( m.raceRuns ) > 0 ) :
		
			# turn the sessionId's into player uid's
			sids = [ run.sessionId for run in m.raceRuns ]
			sids_uuids = self.mm.sessionHandler.GetUUIDs( sids )
			
			for r in m.raceRuns :
				if( r.sessionId in sids_uuids ) :
					r.uuid = sids_uuids[r.sessionId]
				else :
					r.uuid = 0
			
			# thats about it
			return True
		
		# Fetch missing fields from db
		sids = [ player.sessionId for player in m.players ]
		sids_uuids = self.mm.sessionHandler.GetUUIDs( sids )
		self.mm.log( str( sids_uuids ) )
		
		# FIXME: theres gonna be some zero-uuids if theres anonymous players
		uuids = [ uuid for uuid in sids_uuids.itervalues() ]
		stats = self.mm.userHandler.LoadUserStats( uuids, m.gameTypeId )
		
		for player in m.players :
			# TODO: if .uuid == and .sessionId == 0 and config.alpha_phase > 0
			# create an account for this bastard
			player.uuid = sids_uuids[player.sessionId]
			stat = stats.get( player.uuid )
			player.stats = stat
			
			# fix the frag victim id's
			# for frag in player.logFrags :
			#	frag.victim = sids_uuids[frag.victim]
				
			# skill algo wants these separately
			if( stat != None ) :
				player.rating = stat.rating
				player.deviation = stat.deviation
				player.lastGame = stat.lastGame
			else :
				player.rating = skills.DEFAULT_RATING
				player.deviation = skills.DEFAULT_DEVIATION
				# FIXME: this is referenced from multiple locations
				player.lastGame = datetime.datetime.min
				
		# FIXME: do this at wsw-side?
		
		# Figure out the winners
		if( m.teamGame ) :
			bigScore = -99999999
			for team in m.teams.itervalues() :
				if( team.score > bigScore ) :
					bigScore = team.score
					m.winnerTeam = team.index
					
			if( m.winnerTeam == None ) :
				self.mm.log( " ERROR: prepareMatch: Didn't find winner team?")
				return False
			
			else :
				# teamgame winners
				for player in m.players :
					if( player.team == m.winnerTeam ) :
						# this is originally flagged as loss (-1)
						# unless player quit, in case there is no change
						player.outCome = -player.outCome
						
		else :
			bigScore = -9999999
			for player in m.players :
				# FIXED: dont care about quit players
				if( player.score > bigScore and player.outCome != 0 ) :
					bigScore = player.score
					m.winnerPlayer = player
					
			# mark winner
			if( m.winnerPlayer ) :
				m.winnerPlayer.outCome = -m.winnerPlayer.outCome
			else :
				self.mm.log("ERROR: prepareMatch: Didnt find winner player?")
				return False
			
		# calculate skills
		if( skills.CalculateSkills(m.players, m.timePlayed) == False ) :
			self.mm.log( "ERROR: CalculateSkills failed")
			return False
		
		# copy the rating/deviation's back into the Stats structure
		# also add up the win/losses/quits
		for player in m.players :
			if( player.stats ) :
				player.stats.rating = player.newRating
				player.stats.deviation = player.newDeviation
				if( player.outCome > 0 ) :
					player.stats.wins += 1
				elif( player.outCome < 0 ) :
					player.stats.losses += 1
				else :
					player.stats.quits += 1
				
		return True
		
		
	"""
	populateMatch
	parse report string and push it to given match object (m)
	"""
	def populateMatch(self, m, report_string):
		
		if len( report_string ) > 0:
			report = json.loads( report_string, "ascii" )
			if report is not None:
				# TODO: check its structure
				#       It must be a map containing:
				#			- "match": a map of key (string) -> value (string, boolean or number)
				#			- 2 arrays of maps, "teams" and "players"

				# FIXME: check that the score are integers
				if( not "match" in report ) :
					return "No match element in report"
				
				match_elem = report["match"]
				
				# check some important elements
				if( not "teamgame" in match_elem ) :
					return "No teamgame value in match"
				teamGame = safebool(match_elem.get("teamgame"))
				
				# we only expect teams in final report
				if( not "teams" in report and teamGame == True ) :
					return "No teams section in match"
				if( not "players" in report and not "runs" in report ) :
					return "No players section in match"
				
				# start parsing
				gameType = match_elem.get("gametype")
				if( gameType ) :
					m.gameType = gameType.encode("ascii")
				m.mapName = match_elem.get("map")
				m.hostName = match_elem.get("hostname")
				m.timePlayed = safeint(match_elem.get("timeplayed"))
				if(m.timePlayed <= 66):
					return 'Match less or equal than 66 seconds'
				m.timeLimit = safeint(match_elem.get("timelimit"))
				m.scoreLimit = safeint(match_elem.get("scorelimit"))
				m.instaGib = safebool(match_elem.get("instagib"))
				m.teamGame = safebool(match_elem.get("teamgame"))
				m.raceGame = safebool(match_elem.get("racegame"))
				m.timestamp = safeint(match_elem.get("timestamp"))
				m.gamedir = match_elem.get("gamedir")
				m.demoFilename = match_elem.get("demo_filename")
				if (m.demoFilename == None) :
					m.demoFilename = ''

				# None if not final
				teams = report.get("teams")
				if( teams ) :
					for team in teams :
						mteam = MatchTeam(safeint(team.get("score")), team.get("name"), safeint(team.get("index")))
						if( mteam.index in m.teams ) :
							m.teams[mteam.index].merge( mteam )
						else :
							m.teams[mteam.index] = mteam
					
				players = report.get("players", [])
				for player in players :
					# Grab the UID for this player
					sessionId = safeint(player.get("sessionid"))
						
					# Create the matchplayer object
					mplayer = MatchPlayer()
					mplayer.name = player.get("name")
					if( m.teamGame ) :
						mplayer.team = safeint(player.get("team"))
					else :
						mplayer.team = ''
					mplayer.score = safeint(player.get("score"))
					mplayer.timePlayed = safeint(player.get("timeplayed"))
					
					# validate that this player actually played some
					if( mplayer.timePlayed < 1 ) :
						self.mm.log( "  - Dropping player %d because timeplayed %d < 1" % (sessionId, mplayer.timePlayed ) )
						continue
					
					mplayer.final = safeint(player.get("final"))
					# ch : theres actually no need to put these as ints, since
					# they go directly to db as strings?
					mplayer.frags = safeint(player.get("frags"))
					mplayer.deaths = safeint(player.get("deaths"))
					mplayer.suicides = safeint(player.get("suicides"))
					mplayer.teamFrags = safeint(player.get("teamfrags"))
					mplayer.numRounds = safeint(player.get("numrounds"))
					mplayer.dmgGiven = safeint(player.get("dmg_given"))
					mplayer.dmgTaken = safeint(player.get("dmg_taken"))
					mplayer.healthTaken = safeint(player.get("health_taken"))
					mplayer.armorTaken = safeint(player.get("armor_taken"))
					mplayer.gaTaken = safeint(player.get("ga_taken"))
					mplayer.yaTaken = safeint(player.get("ya_taken"))
					mplayer.raTaken = safeint(player.get("ra_taken"))
					mplayer.mhTaken = safeint(player.get("mh_taken"))
					mplayer.uhTaken = safeint(player.get("uh_taken"))
					mplayer.quadsTaken = safeint(player.get("quads_taken"))
					mplayer.shellsTaken = safeint(player.get("shells_taken"))
					mplayer.bombsPlanted = safeint(player.get("bombs_planted"))
					mplayer.bombsDefused = safeint(player.get("bombs_defused"))
					mplayer.flagsCapped = safeint(player.get("flags_capped"))
					
					mplayer.sessionId = safeint(player.get("sessionid"))
					
					if( not mplayer.final ) :
						mplayer.outCome = 0		# quit
					else :
						# mark it as a loss, it will be fixed to 1 if this player wins
						mplayer.outCome = -1
					
					# mplayer.uid = uid
					
					# TODO: general accuracy for anonymous players

					# only add these for registered players
					# unless we are in alpha-testing
					if( config.alpha_phase or ( sessionId > 0 ) ) :
						weapons = player.get("weapons")
						if( weapons ) :
							for weapname,wdef in weapons.iteritems() :
								mweap = MatchWeapon(weapname)
								# ch : just put as strings, cause they go to db directly
								# (unless we merge 2 player infos?)
								mweap.strongAcc = wdef.get("strong_acc")
								mweap.strongDmg = wdef.get("strong_dmg")
								mweap.strongFrags = wdef.get("strong_frags")
								mweap.strongHits = wdef.get("strong_hits")
								mweap.strongShots = wdef.get("strong_shots")
								
								mweap.weakAcc = wdef.get("weak_acc")
								mweap.weakDmg = wdef.get("weak_dmg")
								mweap.weakFrags = wdef.get("weak_frags")
								mweap.weakHits = wdef.get("weak_hits")
								mweap.weakShots = wdef.get("weak_shots")
								
								mweap.CalculateAccuracies()
								
								mplayer.weapons.append(mweap)
					
						awards = player.get("awards")
						if( awards ) :
							for award in awards :
								maward = MatchAward(award.get("name"), award.get("count"))
								# fetch the db uid
								mplayer.awards.append(maward)
				
						logFrags = player.get( "log_frags" )
						if( logFrags ) :
							for frag in logFrags :
								mfrag = MatchFrag()
								mfrag.victim = safeint( frag.get( "victim" ) )
								mfrag.time = safeint( frag.get( "time" ) )
								# FIXME:
								mfrag.weapon = getWeaponName( safeint( frag.get( "weapon" ) ) )
								
								mplayer.logFrags.append( mfrag )
								
					m.players.append( mplayer )
				
				runs = report.get( "runs", [] )
				for run in runs :
					sessionId = safeint( run.get( "session_id" ) )
					timestamp = safeint( run.get( "timestamp" ) )
					times = run.get( "times" )	# this should be a list of int?
					# DEBUG
					for i in xrange( len(times) ) :
						if( not isinstance( times[i], (int, long) ) ) :
							self.mm.log("** times is typeof %s" % type( times[i] ) )
							times[i] = int( times[i] )
							
					mrun = MatchRacerun( sessionId, timestamp, times )
					m.raceRuns.append( mrun )
					
			else :
				return "Failed to parse JSON"
		
		else :
			return "Empty report string"
		

	"""
	AddReport
	Can be called from both UserHandler (PlayerLeft) or from MatchHandler itself
	If match associated with server_id doesnt exist, its created. If final=True,
	skills are calculated and match is stored to db. Also the match is destroyed
	from the list.
	"""
	def AddReport(self, server_id, report, uuid):
		self.mm.log( "AddReport from server %s, uuid %s" % (server_id, uuid) )

		m = Match( server_id )
		
		err = self.populateMatch( m, report )
			
		if( err == None ) :
			if( self.prepareMatch( m ) == True ) :
				# Store the match* objects
				if( m.raceGame ) :
					for run in m.raceRuns :
						if( run.uuid != 0 or config.alpha_phase ) :
							self.db.AddRaceRun( m.serverId, run.uuid, m.mapId, run.times, m.timestamp - run.timestamp )
					
					self.mm.log( "  - MatchReport (RACE) succesfully saved to database")
					return ( m.gameType, [] )
				
				else :
					self.db.AddMatch( m, uuid )
					
					# FIXME
					# let the userhandler know that players statistics have changed
					self.mm.userHandler.UpdatePlayers(m)
											
					self.mm.log( "  - MatchReport succesfully saved to database")
					
					return ( m.gameType, self.listSkills( m ) )
					
			else :
				self.mm.log( "ERROR: Match.AddReport: failed to validate match" )

		else :
			self.mm.log( "ERROR: Match.AddReport: %s" % err )
			
		# return empty gametype and empty list of skills
		return ( "", [])
		
