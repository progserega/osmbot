#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
from lxml import etree
import sys

#DEBUG=False
DEBUG=True

if len(sys.argv) < 3:
	print("Необходимо два аргумента - входной файл и выходной файл")
	sys.exit(1)

def add_patchset(osmpatch, name, tags):
	patchset= etree.SubElement(osmpatch, "patchset")

	# Что ищем:
	find = etree.SubElement(patchset, "find")
	type_node=etree.Element("type", nodes="totally", ways="yes", relation="yes")
	find.append(type_node)

	# key1=value1
	tag = etree.SubElement(find, "tag")
	key=etree.Element("key", full_match="yes")
	key.text="power"
	tag.append(key)
	value=etree.Element("value", full_match="yes", case_sensitive='yes')
	value.text="station"
	tag.append(value)

	# key2=value2
	tag = etree.SubElement(find, "tag")
	key=etree.Element("key", full_match="yes")
	key.text="name"
	tag.append(key)
	value=etree.Element("value", full_match="yes", case_sensitive='yes')
	value.text=name
	tag.append(value)

	# На что меняем:
	add = etree.SubElement(patchset, "add")
	for k in tags:
		key=etree.Element("tag", k=k, v=tags[k])
		add.append(key)

def patch(element, rules_root):
	# Перебираем все patchset-ы:
	for patchset in rules_root:
		if patchset.tag=="patchset":
			if len(patchset):
				process_patchset(element,patchset)

def process_patchset(element, patchset):
	# Перебираем все правила:
	for rule in patchset:
		if rule.tag=="find":
			process_find(element,rule)

def process_find(element, rule):
	find_ways=True
	find_nodes=True
	find_relations=True
	# Ищем настройки, если есть:
	for find_rule in rule:
		if find_rule.taga=="type":
			# берём параметры поиска:
			for key in find_rule.keys:
				if key=="nodes":
					if find_rule.get[key].lower()=="no":
						find_nodes=False
				if key=="ways":
					if find_rule.get[key].lower()=="no":
						find_ways=False
				if key=="relations":
					if find_rule.get[key].lower()=="no":
						find_relations=False
	if element.tag="node" and not find_nodes:
		# Пропуск элемента:
		return 0
	if element.tag="way" and not find_ways:
		# Пропуск элемента:
		return 0
	if element.tag="relation" and not find_relations:
		# Пропуск элемента:
		return 0

	# Перебираем все соответствия:
	tags=0
	success=0
	for find_rule in rule:
		if find_rule.tag=="tag":
			tags+=1
			success+=process_tag(element, find_rule)
			# FIXME

def process_tag(element, tags):
	for tag in tags:
		if tag.tag="key":
			full_match=True
			regex=False
			case_sensitive=True
			math="no"
			# Берём параметры:
			for param_key in tag.keys:
				if param_key=="full_match":
					if tag.get[param_key].lower()=="no":
						full_match=False
				if param_key=="regex":
					if tag.get[param_key].lower()=="yes":
						regex=True
				if param_key=="case_sensitive":
					if tag.get[param_key].lower()=="no":
						case_sensitive=False
				if param_key=="math":
					math=tag.get[param_key].lower()
			# Ищем в элементе OSM тег, имя которого соответствует данному key:
			for osm_tag in element:
				if len(osm_tag):
					k=osm_tag.get('k')

		if tag.tag="value":


			
											




	

# ================= main =================
nodes={}
ways={}
relations={}
# Формируем структуру патчсета:
osmpatch = etree.Element("osmpatch")

osm = etree.parse(sys.argv[1])
osm_root = osm.getroot()
#print (etree.tostring(osm_root,pretty_print=True, encoding='unicode'))

rules = etree.parse(sys.argv[2])
rules_root = rules.getroot()
#print (etree.tostring(rules_root,pretty_print=True, encoding='unicode'))

for node in osm_root:
	if DEBUG:
		print node.tag
	if node.tag=="bounds":
		continue
	# Формируем списки точек, линий, отношений:
	if node.tag=="node":
		print (node.keys())
		nodes[node.get("id")]=node
	if node.tag=="way":
		print (node.keys())
		ways[node.get("id")]=node
	if node.tag=="relation":
		print (node.keys())
		relations[node.get("id")]=node


		
	if node.tag=="old":
	
		for record in node:
			name=""
			tags={}
			for item in record:
				if DEBUG:
					print item.tag, item.text
				if item.tag==u"Подстанция":
					name=item.text
				elif item.tag==u"ДатаСреза":
					tags["power_usage_date"]=u"КДЗ %s" % item.text
					tags["power_usage_comment"]=u"С учётом перспективной нагрузки на %s" % item.text
				elif item.tag==u"РезЗамеровМВт":
					tags["power_usage_mvt_real_kdz"]=item.text.replace(",",".")
				elif item.tag==u"ДопустимаяНагрузкаМВт":
					tags["power_usage_mvt_dopustima"]=item.text.replace(",",".")
				elif item.tag==u"ЗагрузкаПСПроц":
					tags["power_usage_percent"]=item.text.replace(",",".")
				elif item.tag==u"НагрВсего":
					tags["power_usage_mvt_perspectiv_all"]=item.text.replace(",",".")
			if name=="":
				print("error! name=''")
				sys.exit(1)
			add_patchset(osmpatch,name,tags)
			
# Проверяем последовательно все элементы:
for node_id in nodes:
	patch(nodes[node_id], rules_root)









if DEBUG:
	print (etree.tostring(osmpatch,pretty_print=True, encoding='unicode'))

string=etree.tostring(osmpatch, xml_declaration=True, encoding='UTF-8', pretty_print=True )
f=open(sys.argv[2],"w+")
f.write(string)
f.close

print ("nodes", nodes)
for i in nodes:
	for key in nodes[i].keys():
		print (key,"=",nodes[i].get(key))


