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
import database
import warmama

import threading
import urllib
import urllib2

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

'''
Here some thoughts about how servers login and what is needed

	- authkey
	- ip
	- cookie
	
check existing session (expiration, ip)
(uuid, ip) from user
'''
def ServerLogin(authkey, addr):
	
	# loadUser whatnot
	
	# session.GetExistingSession
	
	# overwrite attribs to Session
	# return Session
	
	# else
	# session.NewSession( user )

	return None


'''
ClientLogin

client logins are 2-step
	a) client requests a login and receives a handle to a login process
	b) client does requests with this new handle until it receives
		validation

MM after getting authrequest from client, creates a userlogin element to database,
initiates async POST request to the auth-server and returns a handle to this process
to the client.
When auth-server has (in)validated the request, it sends a POST request back to MM
which marks the userlogin element that the client has a handle to.
'''

class ClientLogin(threading.Thread):
	
	def __init__(self, mm, login, pw):
		threading.Thread.__init__(self)
		
		self.mm = mm
		self.login = login
		self.pw = pw
		
		# TODO: add digest
		self.handle = mm.dbHandler.SaveUserLogin(0, login, 0, 0, None, None)
		
		self.start()
		
	##############################
		
	def GetHandle(self):
		return self.handle
			
	###############################
	
	def run(self):
		
		try :
			# create the url object
			data = urllib.urlencode( { 	'login' : self.login,
										'passwd' : self.pw,
										'handle' : '%d' % self.handle,
										'digest' : 'something',
										'url' : config.auth_response_url 
									} )
			
			# TODO: dont write the password or anything for real!
			# print("**** CLIENTLOGIN CALLING %s %s" % ( config.getauth_url, data ) )
			# self.mm.log("**** CLIENTLOGIN CALLING %s %s" % ( config.getauth_url, data ) )
			
			req = urllib2.urlopen( config.getauth_url, data )
			response = req.read()
			
			# TODO: response will be MM_DATA_MISSING | MM_AUTH_SENT | MM_AUTH_SENDING_FAILED
			# print( "**** CLIENTLOGIN RESPONSE: %s" % response )
			self.mm.log( "**** CLIENTLOGIN RESPONSE: %s" % response )
			req.close()

		except urllib2.HTTPError as e :
			# print( "ClientLogin: Failed to fetch %s" % config.getauth_url )
			self.mm.log( "ClientLogin: Failed to fetch %s, code: %i" % (config.getauth_url, e.code) )
			pass	# this means no user credentials
		except urllib2.URLError as e :
			# print( "ClientLogin: Failed to fetch %s" % config.getauth_url )
			self.mm.log( "ClientLogin: Failed to fetch %s, reason: %s" % (config.getauth_url, e.reason) )
			pass	# this means no user credentials
		
