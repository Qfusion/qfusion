#!/usr/bin/env python2
#-*- coding:utf-8 -*-

"""
Created on 27.3.2011
@author: hc
"""

###################
#
# Imports

import os
import web
import config
import warmama
from warmama import safeint


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

class index :
	def GET(self):
		return 'Hello World! (GET)'
	def POST(self):
		return 'Hello World! (POST)'

class slogin :
	def POST(self):
		input = web.input()
		port = safeint( input.get( 'port', '0' ) )
		authkey = input.get('authkey', '')
		hostname = input.get('hostname', '')
		demos_baseurl = input.get('demos_baseurl', '')
		
		r = warmama.warmama.ServerLogin(authkey, web.ctx.ip, port, hostname, demos_baseurl)	
		web.header('Content-Type', 'application/json')
		return r
	
	def GET(self):
		return self.POST()
	
class slogout :
	def POST(self):
		input = web.input()
		ssession = safeint( input.get( 'ssession', '0' ) )
		
		r = warmama.warmama.ServerLogout(ssession, web.ctx.ip)
		web.header('Content-Type', 'application/json')
		return r
	
	def GET(self):
		return self.POST()

class scc :
	def POST(self) :
		input = web.input()
		ssession = safeint( input.get( 'ssession', '0' ) )
		csession = safeint( input.get( 'csession', '0' ) )
		cticket = safeint( input.get( 'cticket', '0' ) )
		cip = input.get( 'cip', '' )
		
		r = warmama.warmama.ServerClientConnect(ssession, csession, cticket, cip)
		web.header('Content-Type', 'application/json')
		return r
	
	def GET(self):
		return self.POST()
	
class scd :
	def POST(self):
		input = web.input()
		ssession = safeint( input.get( 'ssession', '0' ) )
		csession = safeint( input.get( 'csession', '0' ) )
		gameon = safeint( input.get( 'gameon', '0' ) )
		
		r = warmama.warmama.ServerClientDisconnect(ssession, csession, gameon)
		web.header('Content-Type', 'application/json')
		return r
	
	def GET(self):
		return self.POST()
	
class shb :
	def POST(self):
		input = web.input()
		ssession = safeint( input.get( 'ssession', '0' ) )
		
		r = warmama.warmama.Heartbeat(ssession, web.ctx.ip, 'server')
		web.header('Content-Type', 'application/json')
		return r
	
class smr :
	def POST(self):
		input = web.input()
		
		ssession = safeint( input.get( 'ssession', '0' ) )
		report = input.get( 'data', '' )
		
		r = warmama.warmama.MatchReport(ssession, report, web.ctx.ip)
		web.header('Content-Type', 'application/json')
		return r

class smuuid :
	def POST(self):
		input = web.input()
		ssession = safeint( input.get( 'ssession', '0' ) )
		
		r = warmama.warmama.MatchUUID(ssession, web.ctx.ip)
		web.header('Content-Type', 'application/json')
		return r

# JUST PUTTING THIS IN HERE.. RAW DATA ACCESS IN POST
# def POST(self) :
# 	data = web.data()

### client requests
class clogin :
	def POST(self):
		input = web.input()
		
		login = input.get( 'login', '' ).strip(' \t\n\r')
		pw = input.get( 'passwd', '' ).strip(' \t\n\r')
		handle = safeint( input.get( 'handle', '' ) )
		
		r = warmama.warmama.ClientLogin(login, pw, handle, web.ctx.ip)
		web.header('Content-Type', 'application/json')
		return r
	
	def GET(self):
		return self.POST()

class clogout :
	def POST(self):
		input = web.input()
		csession = safeint( input.get( 'csession', '0' ) )
		
		r = warmama.warmama.ClientLogout(csession, web.ctx.ip)
		web.header('Content-Type', 'application/json')
		return r
	
	def GET(self):
		return self.POST()
	
class ccc :
	def POST(self):
		input = web.input()
		csession = safeint( input.get( 'csession', '0' ) )
		saddr = input.get( 'saddr', '' )
		
		r = warmama.warmama.ClientConnect(csession, saddr)
		web.header('Content-Type', 'application/json')
		return r

class chb :
	def POST(self):
		input = web.input()
		csession = safeint( input.get( 'csession', '0' ) )
		
		r = warmama.warmama.Heartbeat(csession, web.ctx.ip, 'client')
		web.header('Content-Type', 'application/json')
		return r
	
#####################

class auth :
	def POST(self):
		input = web.input()
		
		handle = safeint( input.get( 'handle', '0' ) )
		secret = input.get( 'digest', '' )
		valid = safeint( input.get( 'valid', '0') )
		profile_url = input.get( 'profile_url', '' ).strip(' \t\n\r')
		profile_url_rml = input.get( 'profile_url_rml', '' ).strip(' \t\n\r')

		r = warmama.warmama.ClientAuthenticate(handle, secret, valid, profile_url, profile_url_rml)
		web.header('Content-Type', 'text/plain')
		return r
	
	def GET(self):
		return self.POST()
	
#####################

urls = (
	# server
	'/slogin', 'slogin',
	'/slogout', 'slogout',
	'/scc', 'scc',
	'/scd', 'scd',
	'/smr', 'smr',
	'/shb', 'shb',
	'/smuuid', 'smuuid',
	
	# client
	'/clogin', 'clogin',
	'/clogout', 'clogout',
	'/ccc', 'ccc',
	'/chb', 'chb',
	
	'/auth', 'auth'
)

if config.cgi_mode == 'local' :
	app = web.application(urls, globals())
elif config.cgi_mode == 'wsgi' :
	app = web.application(urls, globals(), autoreload=False)
elif config.cgi_mode == 'fcgi' :
	# something's wrong with FCGI here..
	app = web.application(urls, globals())
	web.wsgi.runwsgi = lambda func, addr=None: web.wsgi.runfcgi(func, addr)
	
if __name__ == "__main__":
	app.run()
elif config.cgi_mode == 'wsgi' :
	# wsgi
	application = app.wsgifunc()
