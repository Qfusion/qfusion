#!/usr/bin/env python2.7
#-*- coding:utf-8 -*-

'''
Created on 11.4.2011

@author: hc
'''

import web
import urllib
import urllib2

##########################

def safeint( s ):
	try :
		return int(s)
	except:
		return 0
	
##########################

class getauth(object):
	def POST(self):
		input = web.input()
		# login = input.get( 'login', '' )
		# passwd = input.get( 'passwd', '' )
		handle = input.get( 'handle', '0' )
		secret = input.get( 'digest', '' )
		
		url = input.get( 'url', '' )
		
		# create a POST request sending validation
		data = urllib.urlencode( {  'handle' : handle,
									'digest' : secret,
									'valid' : '1' } )
		
		try :
			req = urllib2.urlopen( url, data )
			req.close()
		except:
			print("getauth: failed to open url %s" % url)
		
		return ''

##########################

urls = ( '/getauth', 'getauth' )

app = web.application(urls, globals())

if __name__ == "__main__":
	app.run()

