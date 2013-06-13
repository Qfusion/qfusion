#!/usr/bin/env python2.7

'''
Created on 8.5.2011
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

import math
import datetime
import operator

# starting rating for all players (rangeless +-0)
DEFAULT_RATING = 0.0

# starting uncertainity for all players
DEFAULT_DEVIATION = 1.0
MAX_DEVIATION = 1.0
MIN_DEVIATION = 0.1		# MAKE SURE THIS IS NOT 0.0
DEVIATION_SCALE = 0.1	# deviation multiply scale (totally arbitrary)

# default normalizator for skill differences
DEFAULT_T = 400.0
# maximum gain one can get from 1 game
BETA = DEFAULT_T/math.sqrt(2)

# certainity time period in days
DEFAULT_PERIOD_DAYS = 30.0

# minimum gametime to account player to calculations (in secs)
DEFAULT_MIN_TIME = 60

"""
skills require list of player objects that have following member variables:

	score
	rating
	deviation
	timePlayed
	
	name			# for debug printing
	lastGame		# datetime.datetime object of last game played
	
skills will read those and write to these variables

	rank			# position in game #n
	newRating
	newDeviation
	
TODO: add uncertainity by including number of games played within T time,
and number of games expected to be played in T time, to make the results "certain"
"""

# Debug print stuff
DEBUG_PRINT = False
def dprint(fmt, args=None):
	if( DEBUG_PRINT ) :
		if( args ) :
			print( fmt % args )
		else :
			print( fmt )

# Abstract base distribution object
class Distribution(object):
	def __init__(self):
		pass
	
	def cdf(self, x):
		return 0.0
	
	def pdf(self, x):
		return 0.0
	

# Normal distribution (Gaussian - Trueskill(tm))
class NormalDistribution(Distribution):
	def __init__(self):
		pass
	
	# the most simple cdf for normal distribution
	# y = 1.0 - 0.5 * exp( -1.2 * ( x ** 1.3 ) )
	# 0.5 * ( 1.0 + sign(x)*y )
	
	# closer approximation from
	# http://www.wilmott.com/pdfs/090721_west.pdf
	def cdf(self, x): 
		cumnorm = 0.0
		sign = 1.0
		if x < 0.0 :
			sign = -1.0
		x = abs(x)
		if( x > 37.0 ) :
			cumnorm = 0.0
		else :
			e = math.exp( -x ** 2.0 * 0.5 )
			if( x < 7.07106781186547 ) :
				build = 3.52624965998911e-02 * x + 0.700383064443688
				build = build * x + 6.37396220353165
				build = build * x + 33.912866078383
				build = build * x + 112.079291497871
				build = build * x + 221.213596169931
				build = build * x + 220.206867912376
				cumnorm = e * build
				build = 8.8388347683184e-02
				build = build * x + 16.064177579207
				build = build * x + 86.7807322029461
				build = build * x + 296.564248779674
				build = build * x + 637.333633378831
				build = build * x + 793.826512519948
				build = build * x + 440.413735824752
				cumnorm /= build
			else :
				build = x + 0.65
				build = x + 4 / build
				build = x + 3 / build
				build = x + 2 / build
				build = x + 1 / build
				cumnorm = e / build / 2.506628274631
		if sign > 0 :
			cumnorm = 1 - cumnorm
		
		return cumnorm

	# x should be x / T
	# returns a value from curve 0-0.4
	def pdf(self, x):
		return math.exp( -x **2 / 2 ) / math.sqrt(2*math.pi)

# Logistic distribution (Fermi - ELO)
class LogisticDistribution(Distribution):
	def __init__(self):
		pass
	
	# if you scale x by 1.6666666, you get pretty similar curve to
	# normal distribution
	# x should be x / T
	def cdf(self, x):
		x *= 1.666666666
		# this is what ELO uses by replacing e with 10 and s with 400
		return 1.0 / ( 1.0 + math.exp( -x ) )
	
	# if you scale x by 1.666666 you get pretty similar curve to
	# normal distribution
	# # x should be x / T
	# returns a value from curve 0-0.25
	def pdf(self, x):
		x *= 1.66666666
		e = math.exp( -x )
		return e / ( ( 1.0 + e )**2.0 ) # * 1.6
	
########################

# the choice is yours
# dist = NormalDistribution()
dist = LogisticDistribution()

########################


#################################

"""
CH Algorithm #1

1) 	Calculate expectation (Ei) on how player (Pi) fairs in a game
	by averaging the probabilites against every other player (Pj)
	
		Ei = avg( Probability( Pi, Pj) )
		
2)	Generate an array of points given per rank (position based on scores/time)
	So that last place gets 0, first places gets 1.0, growing linearly.
	Ties are handled so that tied positions points is the average of all
	the positions contained in the tied position.
	Ex. For 3 players tied #4, points for that rank is
	
		points #4 = (points #4 + points #5 + points # 6) / 3
		
	All the positions after that get their points normally from their
	positions, i.e #7 gets points from #7, not #5
	
3)	Players result (Ri) in the game is then the points from their rank.
	Gain to skill is then the difference of result and expectation multiplied
	by gain factor
	
		gain = ( Ri - Ei ) * MAX_GAIN 
"""
class CHSkill1( object ) :
	
	def __init__(self, players, timePlayed):
		self.players = players
		self.pointsPerRank = []
		self.expectedScores = []
		self.numPlayers = len(players)
		self.timePlayed = timePlayed
		
	# calculate how well each player would fair in the game
	def getExpected( self ):
		
		size = self.numPlayers
		scale = 1.0 / float(size-1)
		self.expectedScores = range(size)
		
		# reduce calculations by storing head-on probabilites
		# to matrix
		ematrix = range(size*size)
		
		# generate the expected matrix
		for i in xrange(size-1):
			for j in xrange(i+1, size):
				p1 = self.players[i]
				p2 = self.players[j]
				
				x = p1.rating - p2.rating
				# ignore the deviation for now
				# d = p1.deviation + p2.deviation
				d = 0.0
				e = dist.cdf( x / (DEFAULT_T + d) )
				ematrix[i*size+j] = e
				ematrix[j*size+i] = 1.0-e
				
			# dprint( ematrix[i*size:(i+1)*size] )
			
		# The overall expectation is the average of
		# probabilities against every other player
		for i in xrange(size) :
			cumulative = 0.0
			for j in xrange(size) :
				if( i == j ) :
					continue
				
				cumulative += ematrix[i*size+j]
				
			self.expectedScores[i] = cumulative * scale
	
	# Gives all positions points, handles tied positions by giving that position
	# the average values (tied rank #4 would get (#4 + #5)/2)
	# TODO: combine with rankPlayers
	def getPointsPerRank( self ):
		
		# We need a copy of the players, sorted by rank
		p = list( self.players )
		p.sort( key = lambda x : x.rank, reverse = True )
		
		# points are given 0-1, 0 for last and 1 for the winner
		rawValues = [ (i/float(self.numPlayers-1)) for i in xrange(self.numPlayers) ]
		
		# scaled values according to ties
		self.pointsPerRank = [ 0.0 for i in xrange(self.numPlayers) ]
		
		# tiecounts
		tiecounts = [0 for i in xrange(self.numPlayers)]
		
		for i in xrange(self.numPlayers) :
			rank = p[i].rank
			tiecounts[rank] += 1
			self.pointsPerRank[rank] += rawValues[i]
			
		# normalize the values
		for i in xrange(self.numPlayers) :
			if ( tiecounts[i] > 1 ) :
				self.pointsPerRank[i] /= float(tiecounts[i])
			

	# rank players by score/time
	# (if we find another way to sneak timePlayed in, just rank
	#  players by score itself)
	def rankPlayers(self):
		
		# create a copy and sort by scores/time
		ps = list(self.players)
		
		# timePlayed < 1 is handled in MatchHandler
		ps.sort( key = lambda x : x.score/float(x.timePlayed), reverse = True )
		
		lastScore = ps[0].score/float(ps[0].timePlayed)
		
		rank = 0
		for i in xrange(len(ps)) :
			score = ps[i].score / float(ps[i].timePlayed)
			if( score != lastScore ) :
				lastScore = score
				rank += 1
			ps[i].rank = rank

	
	def calculateDeviations(self):
		
		now = datetime.datetime.now()
		
		for player in self.players :
			td = now - player.lastGame
			# python 2.6.5 woes
			# total_seconds = (td.microseconds + (td.seconds + td.days * 24 * 3600) * 10**6) / 10**6
			total_seconds = operator.__truediv__((td.microseconds + (td.seconds + td.days * 24 * 3600) * 10**6), 10**6)
			hours = total_seconds / 3600.0
			hours = min( hours, 24.0*DEFAULT_PERIOD_DAYS )
			
			# Ca = Ca * (1.0/0.1)^(Ta/(24*30))
			player.deviation = player.deviation * math.pow( MAX_DEVIATION / MIN_DEVIATION, hours / (24.0*DEFAULT_PERIOD_DAYS) )
			player.deviation = min( MAX_DEVIATION, max( MIN_DEVIATION, player.deviation ) )
			
	# Main entry point
	def CalculateSkills(self):
		
		self.numPlayers = len( self.players )
		
		# recalculate deviations?
		self.calculateDeviations()
			
		# give our players a ranking
		self.rankPlayers()
		
		# Get our points-per-rank array
		self.getPointsPerRank()
		
		# Estimate how players fair in the game
		self.getExpected()
		
		# now write our new rankings
		for i in xrange(self.numPlayers) :
			player = self.players[i]
			e = self.expectedScores[i]
			p = self.pointsPerRank[player.rank]
			
			gain = p - e
			
			# rating is relative to uncertainity and BETA (which is relative to T)
			# uncertainity moves depending on the expectance of the outcome,
			# it grows when player plays unexpectedly and shrinks when outcome is within expectations
			
			# skill v1.
			# player.newRating = player.rating + gain * MAX_GAIN
			# player.newDeviation = player.deviation
			
			# skill v2.
			player.newRating = player.rating + gain * player.deviation * BETA
			
			# this creates a grow/shrink treshold of 0.5
			d = player.deviation * ( (1 - DEVIATION_SCALE) + DEVIATION_SCALE * 2.0 * abs( gain ) )
			player.newDeviation = min( MAX_DEVIATION, max( MIN_DEVIATION, d ) )
			
		dprint( "SKILLS expectations" )
		dprint( "SKILLS %s", self.expectedScores )
		dprint( "SKILLS points per rank")
		dprint( "SKILLS %s", self.pointsPerRank )
		dprint( "SKILLS ratings")
		if( DEBUG_PRINT ) :
			dprint("\tname\trank\tgain")
			for p in self.players :
				dprint("\t%s\t%i\t%s", (p.name, p.rank, p.newRating - p.rating))
		
"""
CalculateSkills
"""
def CalculateSkills( players, timePlayed ):
	
	cplayers = []
	
	# Make sure we have a flat array of players
	if( not isinstance(players, list) ) :
		if( isinstance(players, dict) ) :
			cplayers = [i for i in players.itervalues()]
		else :
			dprint("    not a dict or list")
			return False
	else :
		cplayers = players
		
	if( len(cplayers) < 2 ) :
		dprint("    too few players")
		return False
	
	# Make sure we have all the necessary fields
	for player in cplayers :
		try :
			player.__getattribute__('score')
			player.__getattribute__('rating')
			player.__getattribute__('deviation')
			player.__getattribute__('timePlayed')
		except:
			dprint("    missing key")
			return False

	# TODO: remove players that didnt play enough
	
	skill = CHSkill1(cplayers, timePlayed)
	skill.CalculateSkills()
	
	return True
	
