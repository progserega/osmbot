# Overview
This is a OSM-bot, which add/remove tags from node/ways by xml-rules. Writed on C and
bash.

This bot download quadrates by 0.5x0.5 (can change in options) degree of OSM map from api 
specifed osm-server to file.osm. Then it patch this osm-file.
Then upload this file to api-server. Download next quadrates, patch, opload and again,
for process full bbox, which setup in options to processed.

Rules file - contain "find rules" and "add/delete" rules. All elements in input OSM-file 
step by step processed by rules. If all "find rules" success for current OSM-element - 
to this element add tags, or delete tags, which writed in this rule.
# Dependeses:


tools: bash, grep, curl, osmosis>=0.39 http://wiki.openstreetmap.org/wiki/Osmosis

GNU/Linux (unix). It may be build on other system, but I not try.

libs: libxml2, glibc


# Build

  make

# Use

1) Set your options (username, api-server url, path to utils, log path, bbox to process by 
bot and other - see osmbot.conf_example) to config file:

  cp osmbot.conf_example osmbot.conf

  vim osmbot.conf

2) Setup rules to process OSM-data (see rules_example.xml for example and comments):
  
  cp rules_example.xml rules.xml
  
  vim rules.xml

3) Run bot:
  
  ./run_bot.sh osmbot.conf 

You may setup many osmbot.conf-files and start osmbot for other tasks. Olso, rule-file
support many "patchset", which contain other rules for other task.
