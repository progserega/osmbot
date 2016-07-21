#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
from lxml import etree
import sys
import getopt
import re
import os

#DEBUG=False
DEBUG=True

def check_links(element):
	global nodes
	global ways
	num=0

	if element.tag="ways":
		for elem in element:
			if elem.tag=="nd":
				if "ref" in elem.keys():
					element_id=elem.get("ref")
					if element_id not in nodes:
						# broken link!
						print("BROKEN LINK found, way_id=%s, nd-ref=%s" % (element.get("id"), element_id))
						print("remove it!")
						element.remove(elem)
						num+=1
	else if element.tag="relation":
		for elem in sub_element:
			if elem.tag=="member":
				if "type" in elem.keys() and "ref" in elem.keys():
					if elem.get("type") == "way":
						element_id=elem.get("ref")
						if element_id not in ways:
							print("BROKEN LINK found, relation=%s, way-ref=%s" % (element.get("id"), element_id))
							print("remove it!")
							element.remove(elem)
							num+=1
					if elem.get("type") == "node":
						element_id=elem.get("ref")
						if element_id not in nodes:
							print("BROKEN LINK found, relation=%s, node-ref=%s" % (element.get("id"), element_id))
							print("remove it!")
							element.remove(elem)
							num+=1
	return num

#################  Main  ##################
nodes={}
ways={}
relations={}

in_file=''
out_file=''
rules_file=''

parse_opts()
if in_file=='' or out_file=='' or rules_file=='':
	print_help()
	sys.exit(2)

osm = etree.parse(in_file)
osm_root = osm.getroot()
#print (etree.tostring(osm_root,pretty_print=True, encoding='unicode'))

rules = etree.parse(rules_file)
rules_root = rules.getroot()
#print (etree.tostring(rules_root,pretty_print=True, encoding='unicode'))

for node in osm_root:
	if DEBUG:
		print node.tag
	if node.tag=="bounds":
		continue
	# Формируем списки точек, линий, отношений:
	if node.tag=="node":
#		if DEBUG:
#			print ("DEBUG: node.keys: ", node.keys())
		nodes[node.get("id")]=node
	if node.tag=="way":
#		if DEBUG:
#			print ("DEBUG: node.keys: ", node.keys())
		ways[node.get("id")]=node
	if node.tag=="relation":
#		if DEBUG:
#			print ("DEBUG: node.keys: ", node.keys())
		relations[node.get("id")]=node

# Проверяем последовательно все линки:
for way_id in ways:
	num=check_links(ways[way_id])
	print("Найдено и исправленно %d ссылок в ways"%num)
for relation_id in relations:
	num=check_links(relations[relation_id])
	print("Найдено и исправленно %d ссылок в relations"%num)


#if DEBUG:
#	print (etree.tostring(osmpatch,pretty_print=True, encoding='unicode'))

string=etree.tostring(osm, xml_declaration=True, encoding='UTF-8', pretty_print=True )
f=open(out_file,"w+")
f.write(string)
f.close

#print ("nodes", nodes)
#for i in nodes:
#	for key in nodes[i].keys():
#		print (key,"=",nodes[i].get(key))


