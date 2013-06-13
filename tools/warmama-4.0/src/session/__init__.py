#!/usr/bin/env python2.7
#-*- coding:utf-8 -*-

"""
Created on 30.3.2011
@author: hc
"""

###################
#
# Imports

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

class Session(object):

	# Servers use this baseclass
		
	def __init__(self, type, exists):
		# common
		self.type = type
		self.exists = exists	# exists = ( id != 0 ) ??
		# from db
		self.id = 0
		self.timestamp = 0
		self.user_id = 0
		self.ip = ''
		self.ipv6 = ''
		self.port = 0	# not used by clients
		self.digest = 0
		self.next_match_uuid = ''
		
		# client
		# from db
		self.ticket_id = 0
		self.ticket_server = 0
		self.ticket_expiration = None
		self.server_session = 0
		self.purgable = 0
	
	def getTimestamp(self):
		return self.timestamp
	
	# SERVER PORT
	def setPort(self, port):
		self.port = port
		
	def getPort(self):
		return self.port
	
	# CLIENT TICKET
	def setTicket(self, ticket_id, ticket_server, ticket_expiration):
		self.ticket_id = ticket_id
		self.ticket_server = ticket_server
		# check for updates on this (initially set, on updates may be zero)
		if( ticket_expiration ) :
			self.ticket_expiration = ticket_expiration
		self.server_session = 0
		
	def getTicket(self):
		return (self.ticket_id, self.ticket_server, self.ticket_expiration)
	
	# CLIENT SERVER
	def setServer(self, server_session):
		# this resets the server, so do this first
		self.setTicket(0, 0, 0)
		self.server_session = server_session
		
	def getServer(self):
		return self.server_session
	
	def setAddress(self, ip, ipv6 ):
		self.ip = ip
		self.ipv6 = ipv6

class SessionHandler(object):
	
	def __init__(self, mm):
		self.mm = mm
	
	def fromdb(self, fields, type):
		# FIXME: relies too much on db-schema knowledge
		s = Session(type, True)
		s.id = fields[0]
		s.timestamp = fields[2]	# updated
		s.user_id = fields[3]
		s.ip = fields[4]
		s.ipv6 = fields[5]
		s.digest = fields[6]
		
		if( type == 'client' ) :
			s.ticket_id = fields[7]
			s.ticket_server = fields[8]
			s.ticket_expiration = fields[9]
			s.server_session = fields[10]
			s.purgable = fields[11]
			
		else :
			s.port = fields[7]
			s.next_match_uuid = fields[8]
		
		return s
	
	'''
	Load an existing session into Session object by user_id
	'''
	def LoadSession(self, uuid=None, sid=None, type=None):
		db = self.mm.dbHandler
		
		if( uuid ) :
			fields = db.GetSession( uuid=uuid, type=type )
		else :
			fields = db.GetSession( sid=sid, type=type )
			
		if( not fields ) :
			return None
		
		return self.fromdb(fields, type)
	
	'''
	Create new Session object into memory for given user
	'''
	def NewSession(self, uuid, ip, ipv6, type):
		s = Session( type, False )
		
		s.user_id = uuid
		s.ip = ip
		s.ipv6 = ipv6
		s.digest = self.mm.gen_digest(32)
		
		return s

	'''
	Save given session to database
	'''
	def SaveSession(self, s ):
		
		if( s.exists ) :
			self.mm.dbHandler.UpdateSession( s )			
		else :
			s.id = self.mm.dbHandler.SaveSession( s )
			
		return s.id

	'''
	Removes given session
	'''
	def RemoveSession( self, s ):
		
		if( s.exists ) :
			# debug
			self.mm.log("RemoveSession going down..")
			self.mm.dbHandler.RemoveSession( s )
			self.mm.log("RemoveSession done")

	'''
	Get server's session matching given address
	'''
	def ServerSessionByAddr(self, ip, ipv6, port ):
		# TODO: this should resolve bugs where old server sessions with
		# the same IP and port still floating around. So grab
		# them all and figure if you want to drop the old sessions
		# (this shouldnt even happen)
		fields = self.mm.dbHandler.SessionByAddr( ip, ipv6, port )	
		if( not fields ) :
			return None
		
		return self.fromdb(fields, type)
	
	'''
	GetUUIDs
	Translate a set of session ID's into user-ids
	returning a dictionary of { session : uuid }
	'''
	def GetUUIDs(self, sessions ):
		# FIXME: this isnt really supporting that sessions = type int
		# even though db handler supports it?
		# FIXME: also in list case, list of len 1 screws mysqldb up
		
		valid = True
		if( isinstance( sessions, list ) ) :
			# first we grab only registered sessions
			sessions_copy = []
			for s in sessions :
				if( s > 0 ) :
					sessions_copy.append( s )
			
			if( len( sessions_copy ) == 0 ) :
				valid = False
			elif( len( sessions_copy ) == 1 ) :
				sessions_copy = sessions_copy[0]
		elif( isinstance( sessions, (int, long) ) ) :
			sessions_copy = sessions
		else :
			self.mm.log( "GetUUIDs: invalid type of sessions %s" % type(sessions) )
			return {}
			
		book = None	
		if( valid ) :
			book = self.mm.dbHandler.GetUUIDs( sessions_copy )
			
		if( not book ) :
				book = {}
				
		# convert the int type back to list for common handling
		if( isinstance( sessions, (int, long) ) ) :
			sessions = [ sessions ]
			
		# fix the missing sessions into 0 UUID
		for s in sessions :
			if( not s in book ) :
				book[s] = 0
				
		return book

	def CreatePlayerSession(self, uuid, ip, ipv6 ) :
		
		s = self.LoadSession(uuid=uuid, type='client')
		if( s != None and s.getServer() != 0 ) :
			# found an old session, lets validate some stuff
			ss = self.LoadSession(sid=s.getServer(), type='server')
			if( ss == None ) :
				# some orphaned session?
				self.RemoveSession(s)
				s = None
				ss = None

		if( s == None ) :
			# create a new one
			s = self.NewSession(uuid, ip, ipv6, 'client')
		else :
			# found an old session, lets validate some stuff
			if( s.getServer() != 0 ) :
				self.mm.log( "User on server for existing session")
				return None
			
			if( s.ip != ip and s.ipv6 != ipv6 ) :
				self.mm.log( "Different IP for existing session")
				return None
			
			s.setAddress( ip, ipv6 )
			
			s.purgable = 0
			
		s.setServer( 0 )
		self.SaveSession(s)
		
		return s
