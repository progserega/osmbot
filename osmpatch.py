#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
from lxml import etree
import sys
import re

#DEBUG=False
DEBUG=True

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
			if process_find(element,rule):
				if DEBUG:
					print("Найден элемент")
				patch_element_by_rule(element,patchset)

def patch_element_by_rule(element,patchset):
	# TODO патчим элемент по заданным в patchset-е правилам:
	for rule in patchset:
		if rule.tag=="add":
			patch_add_tags_to_element(element,rule)
		elif rule.tag=="delete":
			patch_delete_tags_from_element(element,rule)
		elif rule.tag=="delete_element":
			patch_delete_element(element,rule)
	return

def patch_add_tags_to_element(element,rule):
	# добавляем теги к элементу:
	for tag in rule:
		# Для каждого тега "tag" в правиле "add":
		if tag.tag=="tag":
			# Берём ключи:
			if "k" in tag.keys() and "v" in tag.keys():
				k=tag.get("k")
				v=tag.get("v")
				# Добавляем соответствующие теги в "element":
				if DEBUG:
					print ("DEBUG: patch_add_tags_to_element() add k=%s, v=%s to element" % (k,v))

				# Ищем, есть ли такие теги в этом элементе:
				update=False
				for osm_tag in element:
					if osm_tag.tag=="tag":
						if "k" in osm_tag.keys():
							if osm_tag.get("k")==k:
								update=True
								osm_tag.set("v",v)
								break
				if not update:
					# Создаём новую ветку xml в виде тега tag и соответствующих атрибутов:
					new_tag=etree.Element("tag") 
					new_tag.set("k", k)
					new_tag.set("v", v)
					# добавляем эту ветку в основное дерево:
					element.append(new_tag)
	return

def patch_delete_tags_from_element(element,rule):
	# удаляем теги у элемента:
	for tag in rule:
		# Для каждого тега "tag" в правиле "add":
		if tag.tag=="tag":
			# Берём ключи:
			if "k" in tag.keys():
				k=tag.get("k")
				# удаляем соответствующие теги из "element":
				if DEBUG:
					print ("DEBUG: patch_delete_tags_from_element() delete tag k=%s from element" % k)

				# Ищем, есть ли такие теги в этом элементе:
				for osm_tag in element:
					if osm_tag.tag=="tag":
						if "k" in osm_tag.keys():
							if osm_tag.get("k")==k:
								element.remove(osm_tag)
								break
	return

def patch_delete_element(element,rule):
	# удаляем элемент:
	recursive=False
	if "recursive" in rule.keys():
		if rule.get("recursive").lower()=="yes":
			recursive=True
	if recursive:
	 	remove_element_recurse(element)
	else:
		# Удаляем элемент без дочерних элементов:
		parent=element.getparent()
		parent.remove(element)
	return

def remove_element_recurse(element):
	if DEBUG:
		print("DEBUG: remove_element_recurse() element", element)
	# Удаляем дочерние элементы:
	for sub_element in element:
		if sub_element.tag=="nd":
			if "ref" in sub_element.keys():
				ref=sub_element.get("ref")
				root_osm=element.getparent()
				print("root_osm", root_osm)
				elem=find_element_by_id(root_osm,ref)
				if elem is not None:
					if DEBUG:
						print("DEBUG: remove_element_recurse() remove element with id=%s" % ref, element)
					root_osm.remove(element)

		elif sub_element.tag=="way":
			if "ref" in sub_element.keys():
				ref=sub_element.get("ref")
				elem=find_element_by_id(root_osm,ref)
				if elem is not None:
					remove_element_recurse(element)

		elif sub_element.tag=="relation":
			if "ref" in sub_element.keys():
				ref=sub_element.get("ref")
				elem=find_element_by_id(root_osm,ref)
				if elem is not None:
					remove_element_recurse(element)

	# удаляем сам элемент:
	parent=element.getparent()
	parent.remove(element)
	return

def find_element_by_id(root_osm,osm_id):
	# ищем элемент с начала OSM-xml:
	for osm_elem in root_osm:
		if osm_elem.tag=="node":
			if "id" in osm_elem.keys():
				if osm_elem.get("id")==osm_id:
					return osm_elem

def process_find(element, rule):
	if DEBUG:
		print("DEBUG: process_find")
	find_ways=True
	find_nodes=True
	find_relations=True
	# Ищем настройки, если есть:
	for find_rule in rule:
		if find_rule.tag=="type":
			# берём параметры поиска:
			for key in find_rule.keys():
				if key=="nodes":
					if find_rule.get(key).lower()=="no":
						find_nodes=False
				if key=="ways":
					if find_rule.get(key).lower()=="no":
						find_ways=False
				if key=="relations":
					if find_rule.get(key).lower()=="no":
						find_relations=False
	if element.tag=="node" and not find_nodes:
		# Пропуск элемента:
		return False
	if element.tag=="way" and not find_ways:
		# Пропуск элемента:
		return False
	if element.tag=="relation" and not find_relations:
		# Пропуск элемента:
		return False

	# Перебираем все соответствия:
	tags=0
	success=0
	for find_rule in rule:
		if find_rule.tag=="tag":
			if not find_rule_tag_in_osm_element(element, find_rule):
				# Данная тег-значение не найдена в элементе OSM, поэтому
				# считаем, что данный элемент OSM не удовлетворяет данному правилу поиска:
				return False
	# Считаем, что данный элемент OSM соответствует всем искомым правилам:
	return True


def find_rule_tag_in_osm_element(element, tags):
	if DEBUG:
		print("DEBUG: find_rule_tag_in_osm_element")
	# По умолчанию считаем, что ищем только имя тега OSM, без его значения:
	find_value=False
	# Берём параметры поиска:
	for tag in tags:
		if tag.tag=="key":
			# Берём параметры:
			key_opt=get_rule_tag_param(tag)
			# Берём искомое значение:
			key_rule_text=tag.text
		if tag.tag=="value":
			find_value=True
			# Берём параметры:
			val_opt=get_rule_tag_param(tag)
			# Берём искомое значение:
			val_rule_text=tag.text

	# Перебираем все теги в OSM-элементе:
	for osm_tag in element:
		if DEBUG:
			print("DEBUG: find_rule_tag_in_osm_element(): osm_tag", osm_tag.tag)
		# Для каждого тега:
		if osm_tag.tag=="tag":
			# Берём имя тега:
			src_text=osm_tag.get('k')
			if DEBUG:
				print("DEBUG: find_rule_tag_in_osm_element(): k=%s" % src_text)
			# Проверяем имя тега OSM на соответствие искомому тегу правил:
			if check_param_by_rule(src_text, key_rule_text, key_opt):
				# Если тег совпал с искомым, проверяем, если нужно значние тега:
				if find_value:
					src_text=osm_tag.get('v')
					# Проверяем значение тега OSM на соответствие значению искомого тега правил:
					if check_param_by_rule(src_text, val_rule_text, val_opt):
						# совпало, значит сообщаем об успехе:
						return True
				else:
					# Т.к. значение искать не нужно, то сообщаем об успехе:
					return True
	# Не нашли теги в элементе:
	return False
						

# Берём настройки поиска для тега правил:
def get_rule_tag_param(tag):
	opt={}
	opt["regex"]=False
	opt["math"]="no"
	opt["case_sensitive"]=True
	opt["full_match"]=True

	# Берём параметры:
	for param_key in tag.keys():
		if param_key=="full_match":
			if tag.get(param_key).lower()=="no":
				opt["full_match"]=False
		if param_key=="regex":
			if tag.get(param_key).lower()=="yes":
				opt["regex"]=True
		if param_key=="case_sensitive":
			if tag.get(param_key).lower()=="no":
				opt["case_sensitive"]=False
		if param_key=="math":
			opt["math"]=tag.get(param_key).lower()
	if DEBUG:
		print("DEBUG: get_rule_tag_param(): return opt", opt)
	return opt

# Проверяем значение на соответствие правилу поиска:
def check_param_by_rule(src_text, rule_text, opt):
	if DEBUG:
		print("find '%s' in '%s'" % (rule_text.encode("utf8"), src_text.encode("utf8")))
	if opt["regex"]:
		if re.search(rule_text, src_text) is not None:
			if DEBUG:
				print("src_text='%s' соответствует регулярному выражению: '%s'" % (src_text,rule_text))
			return True
	elif opt["math"] !="no":
		if opt["math"]=="lt":
			if float(src_text) < float(rule_text):
				return True
		elif opt["math"]=="gt":
			if float(src_text) > float(rule_text):
				return True
	if opt["case_sensitive"]:
		src_text=src_text.lower() 
		rule_text=rule_text.lower() 
	if opt["full_match"]:
		if src_text==rule_text:
			return True
	else:
		if rule_text in src_text:
			return True
	# Не соответствует:
	return False

	

# ================= main =================
nodes={}
ways={}
relations={}

if len(sys.argv) < 4:
	print("Необходимо три аргумента - входной файл OSM, файл правил и выходной файл OSM")
	print("Например:")
	print("	%s in.osm rules.xml out.xml" % sys.argv[0])
	sys.exit(1)

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
		if DEBUG:
			print ("DEBUG: node.keys: ", node.keys())
		nodes[node.get("id")]=node
	if node.tag=="way":
		if DEBUG:
			print ("DEBUG: node.keys: ", node.keys())
		ways[node.get("id")]=node
	if node.tag=="relation":
		if DEBUG:
			print ("DEBUG: node.keys: ", node.keys())
		relations[node.get("id")]=node

# Проверяем последовательно все элементы:
for node_id in nodes:
	patch(nodes[node_id], rules_root)
for way_id in ways:
	patch(ways[way_id], rules_root)
for relation_id in relations:
	patch(relations[relation_id], rules_root)








#if DEBUG:
#	print (etree.tostring(osmpatch,pretty_print=True, encoding='unicode'))

string=etree.tostring(osm, xml_declaration=True, encoding='UTF-8', pretty_print=True )
f=open(sys.argv[3],"w+")
f.write(string)
f.close

#print ("nodes", nodes)
#for i in nodes:
#	for key in nodes[i].keys():
#		print (key,"=",nodes[i].get(key))


