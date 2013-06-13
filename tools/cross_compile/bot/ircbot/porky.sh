#!/bin/sh
bot-basicbot-pluggable --nick porky --server irc.quakenet.org --channel "warsow.betatest bionicmoon4" \
--module "Auth" --module "Loader" --module "Seen" --module "DNS" --module "Infobot" --module "Vars" \
--store dummy="dummy" \
--store type="DBI" --store dsn="dbi:mysql:porkybot" --store user="porkybot" --store password="porkybotpass" --store create_index=1 \
--password porkybotpass
#--loglevel trace \


