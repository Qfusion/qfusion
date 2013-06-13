#!/bin/bash

# 'start' 'restart' 'stop' 'check'
# this here controls the FCGI processes
# allows to start, stop, check
# call this once every XXX in cron with 'check' argument
# to ensure that the FCGI is running

# change this to wherever you have warmama installed
CWD="${HOME}/src/warsow/source/trunk/tools/warmama-4.0"
# port you want fcgi to run in
FCGI_ADDR="127.0.0.1:9000"
# name of the cgi script
SCRIPT="src/wcgi.py"

# matching exp, here we match all python versions
MATCH="python[0-9.]* $SCRIPT"


# you can also setup multiple processes, modify upper 2 vars to
# FCGI_ADDR1=..
# FCGI_ADDR2=..
# SCRIPT1=...
# SCRIPT2=...

#if [ $# -lt 2 ]
#then
#	echo "usage: check-cgi COMMAND SETTINGS"
#	exit
#fi

cd $CWD
export PYTHONPATH="${CWD}/src"

# we do not need this anymore, after all we are running
# game-specific cgi code anyway!!!

#export QBROWSER_SETTINGS=$2

PIDS=`pgrep -f "$MATCH"`
COUNT=`echo $PIDS | wc -w`

case $1 in 
	start)
		if [ $COUNT -gt 0 ]
		then
			echo "processes already running.. exiting"
			exit
		fi
		# start the cgi processes
		echo "starting CGI processes.."
		cgi-fcgi -start -connect $FCGI_ADDR $SCRIPT
		# cgi-fcgi -start -connect $FCGI_ADDR1 $SCRIPT1
		# cgi-fcgi -start -connect $FCGI_ADDR2 $SCRIPT2
	;;
	
	check)
		# check if processes are alive and if not, start them
		# !! modify this to match the number of processes
		if [ $COUNT -ne 1 ]
		then
			echo "CGI not running, starting it.."
			cgi-fcgi -start -connect $FCGI_ADDR $SCRIPT
			# cgi-fcgi -start -connect $FCGI_ADDR1 $SCRIPT1
			# cgi-fcgi -start -connect $FCGI_ADDR2 $SCRIPT2
		else
			echo "CGI is running well (`echo $PIDS`)"
		fi
	;;
	
	stop)
		# stop the processes
		echo "killing $COUNT number of processes (`echo $PIDS`)"
		kill `echo $PIDS`
	;;
	
	restart)
		# stop and restart

		if [ $COUNT -gt 0 ]
		then
			echo "killing $COUNT number of processes (`echo $PIDS`)"
			kill `echo $PIDS`
			# sleep for a few seconds to free the port
			sleep 2s
		fi
		echo "starting CGI processes.."
		cgi-fcgi -start -connect $FCGI_ADDR $SCRIPT
		# cgi-fcgi -start -connect $FCGI_ADDR1 $SCRIPT1
		# cgi-fcgi -start -connect $FCGI_ADDR2 $SCRIPT2
	;;
	
	pid)
		# echo the pids of these processes to screen
		echo $PIDS
	;;
	
esac
