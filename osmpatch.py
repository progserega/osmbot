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

def patch(element, rules_root):
  # Перебираем все patchset-ы:
  for patchset in rules_root:
    if patchset.tag=="patchset":
#      if len(patchset):
      process_patchset(element,patchset)

def process_patchset(element, patchset):
  # Перебираем все правила:
  for rule in patchset:
    if rule.tag=="find":
      if process_find(element,rule):
        if DEBUG:
          os.write(2,"\nfinded element!")
        patch_element_by_rule(element,patchset)

def patch_element_by_rule(element,patchset):
  # патчим элемент по заданным в patchset-е правилам:
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
          os.write(2,"\nDEBUG: patch_add_tags_to_element() add k=%s, v=%s to element" % (k.encode("utf8"),v.encode("utf8")))

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
          os.write(2,"\nDEBUG: patch_delete_tags_from_element() delete tag k=%s from element" % k.encode("utf8"))

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
    num_link=get_num_link_to_this_elem(element) 
    if num_link == -1:
      os.write(2,"\nERROR parsing OSM with element: %s", element.tag)
      sys.exit(1)
    if num_link == 0:
      # Удаляем элемент без дочерних элементов:
      parent=element.getparent()
      parent.remove(element)
    else:
      # remove all links:
      num_link=remove_links(element)
      os.write(2,"\nremove_links(): remove %d links to removed element" % num_link)
      # remove element:
      parent=element.getparent()
      parent.remove(element)
  return

def remove_element_recurse(element):
  if DEBUG:
    os.write(2,"\nDEBUG: remove_element_recurse() element: %s" % element.tag)
  root_osm=element.getparent()
  # Удаляем дочерние элементы:
  for sub_element in element:
    if sub_element.tag=="nd":
      if "ref" in sub_element.keys():
        ref=sub_element.get("ref")
        if DEBUG:
          os.write(2,"\nDEBUG: remove_element_recurse() prepare delete subelement with id=%s from element: %s" % (ref, element.tag))
        elem=find_element_by_id(root_osm,ref,"node")
        if elem is not None:
          if DEBUG:
            os.write(2,"\nDEBUG: remove_element_recurse() remove element with id=%s, element: %s" % (ref, element.tag))
          remove_element_recurse(elem)

    elif sub_element.tag=="member":
      if "type" in sub_element.keys() and "ref" in sub_element.keys():
        ref=sub_element.get("ref")
        osm_type=sub_element.get("type")
        elem=find_element_by_id(root_osm,ref,osm_type)
        if elem is not None:
          remove_element_recurse(elem)

  # удаляем сам элемент:
  # Проверяем, что на этот элемент не больше одной ссылки (точка состоит только в одной линии, линия находится только в одном отношении):
  num_link=get_num_link_to_this_elem(element) 
  if num_link == -1:
    os.write(2,"ERROR parsing OSM with element: %s"%element.tag)
    sys.exit(1)
  if num_link == 0:
    parent=element.getparent()
    parent.remove(element)
  else:
    if DEBUG:
      os.write(2,"\nDEBUG: remove_element_recurse(): element have more 1 link - try remove all links before: %s" % element.tag)
    # remove all links:
    num_link=remove_links(element)
    os.write(2,"\nremove_links(): remove %d links to removed element" % num_link)
    # remove element:
    parent=element.getparent()
    parent.remove(element)
  return

def remove_links(element):
  global nodes
  global ways
  num=0

  if DEBUG:
    print("DEBUG: proccess element=%s, element.tag=%s" % (element.get("id"),element.tag))

  if element.tag=="way":
    if DEBUG:
      print("DEBUG: proccess way_id=%s" % element.get("id"))
    for elem in element:
      if elem.tag=="nd":
        if "ref" in elem.keys():
          element_id=elem.get("ref")
          if DEBUG:
            print("DEBUG: proccess way_id=%s, nd_ref=%s" % (element.get("id"),element_id))
          if element_id not in nodes:
            # broken link!
            print("BROKEN LINK found, way_id=%s, nd-ref=%s" % (element.get("id"), element_id))
            print("remove it!")
            element.remove(elem)
            num+=1
  elif element.tag=="relation":
    for elem in element:
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

def get_num_link_to_this_elem(element):
  num_link=0
  element_id=0
  osm_type=element.tag

  if "id" in element.keys():
    element_id=element.get("id")
  else:
     return -1

  root_osm=element.getparent()
  if root_osm == None:
    return -1

  for sub_element in root_osm:
    if sub_element.tag=="relation":
      for elem in sub_element:
        if elem.tag=="member":
          if "type" in elem.keys() and "ref" in elem.keys():
            if elem.get("type") == osm_type:
              if elem.get("ref") == element_id:
                num_link+=1
    if sub_element.tag=="way" and osm_type=="node":
      for elem in sub_element:
        if elem.tag=="nd":
          if "ref" in elem.keys():
            if elem.get("ref") == element_id:
              num_link+=1
  return num_link

def find_element_by_id(root_osm,osm_id,osm_type):
  # ищем элемент с начала OSM-xml:
  for osm_elem in root_osm:
    if osm_elem.tag==osm_type:
      if "id" in osm_elem.keys():
        if osm_elem.get("id")==osm_id:
          return osm_elem

def process_find(element, rule):
  if DEBUG:
    os.write(2,"\nDEBUG: process_find")
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
    if find_rule.tag=="osm_id":
      print("id:",find_rule.attrib["id"])
      if element.attrib["id"] == find_rule.attrib["id"]:
        # идентификаторы совпадают, считаем, что нашли, проверяем следующие правила:
        continue
      else:
        # id объекта не совпадает с искомым
        return False
      
    if find_rule.tag=="tag":
      # берём атрибут 'skip' (пропуск элемента)
      skip=False
      if "skip" in find_rule.keys():
        if find_rule.get("skip").lower()=="yes":
          skip=True

      find_flag=find_rule_tag_in_osm_element(element, find_rule)
      if find_flag == True and skip == True:
        # если теги найдены, но установлен параметр пропуска таких значений, то пропускаем элемент:
        return False
      elif find_flag == False and skip == False:
        # Данная тег-значение не найдена в элементе OSM, поэтому
        # считаем, что данный элемент OSM не удовлетворяет данному правилу поиска:
        return False
      else:
        # в иных случаях - условие верно и элемент нам подходит, но нужно проверить последующие условия - 
        # продолжаем цикл
        pass
  # Считаем, что данный элемент OSM соответствует всем искомым правилам:
  return True


def find_rule_tag_in_osm_element(element, tags):
  if DEBUG:
    os.write(2,"\nDEBUG: find_rule_tag_in_osm_element")
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
      os.write(2,"\nDEBUG: find_rule_tag_in_osm_element(): osm_tag: %s" % osm_tag.tag)
    # Для каждого тега:
    if osm_tag.tag=="tag":
      # Берём имя тега:
      src_text=osm_tag.get('k')
      if DEBUG:
        os.write(2,"\nDEBUG: find_rule_tag_in_osm_element(): k=%s" % src_text)
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
    elif param_key=="case_sensitive":
      if tag.get(param_key).lower()=="no":
        opt["case_sensitive"]=False
    elif param_key=="regex":
      if tag.get(param_key).lower()=="yes":
        opt["regex"]=True
    elif param_key=="math":
      opt["math"]=tag.get(param_key).lower()
  if DEBUG:
    os.write(2,"\nDEBUG: get_rule_tag_param(): return opt")
    print("\nDEBUG: get_rule_tag_param(): return opt", opt)
  return opt

# Проверяем значение на соответствие правилу поиска:
def check_param_by_rule(src_text, rule_text, opt):
  if DEBUG:
    os.write(2,"\nfind '%s' in '%s'" % (rule_text.encode("utf8"), src_text.encode("utf8")))
  if opt["regex"]:
    if re.search(rule_text, src_text) is not None:
      if DEBUG:
        os.write(2,"\nsrc_text='%s' соответствует регулярному выражению: '%s'" % (src_text.encode("utf8"),rule_text.encode("utf8")))
      return True
    else:
      return False
  elif opt["math"] !="no":
    if opt["math"]=="lt":
      if float(src_text) < float(rule_text):
        return True
      else:
        return False
    elif opt["math"]=="gt":
      if float(src_text) > float(rule_text):
        return True
      else:
        return False
  if opt["case_sensitive"]==False:
    src_text=src_text.lower() 
    rule_text=rule_text.lower() 
  if opt["full_match"]:
    if src_text==rule_text:
      return True
    else:
      return False
  else:
    if rule_text in src_text:
      return True
    else:
      return False
  # Не соответствует:
  return False


def print_help():
  os.write(2,"""
This is parsing OSM programm. 
  This programm change input file by rules file and save result to output file. 
  Use:            
    %(script_name)s -r rules.xml -i input.osm -o out.osm 

options: 
  -r file - file with xml-rules 
  -i file - input file with osm 
  -o file - output file with osm, where programm save result 
  -d - debug output
  -h - this help
need 3 parametr: rules-file, input and output files.
Use -h for help.
exit!
""" % {"script_name":sys.argv[0]})

def parse_opts():
  inputfile = ''
  outputfile = ''
  try:
    opts, args = getopt.getopt(sys.argv[1:],"hdr:i:o:",["help","debug","rules=","infile=","outfile="])
  except getopt.GetoptError as err:
    os.write(2, str(err) ) # will print something like "option -a not recognized"
    print_help()
    sys.exit(2)

  for opt, arg in opts:
    if opt in ("-h", "--help"):
      print_help()
      sys.exit()
    elif opt in ("-i", "--infile"):
      global in_file
      in_file = arg
    elif opt in ("-o", "--outfile"):
      global out_file
      out_file = arg
    elif opt in ("-r", "--rules"):
      global rules_file 
      rules_file = arg
    elif opt in ("-d", "--debug"):
      global DEBUG 
      DEBUG = True

#################  Main  ##################
nodes={}
ways={}
relations={}

in_file=''
out_file=''
rules_file=''
DEBUG=False

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
#    if DEBUG:
#      print ("DEBUG: node.keys: ", node.keys())
    nodes[node.get("id")]=node
  if node.tag=="way":
#    if DEBUG:
#      print ("DEBUG: node.keys: ", node.keys())
    ways[node.get("id")]=node
  if node.tag=="relation":
#    if DEBUG:
#      print ("DEBUG: node.keys: ", node.keys())
    relations[node.get("id")]=node

# Проверяем последовательно все элементы:
for node_id in nodes:
  patch(nodes[node_id], rules_root)
for way_id in ways:
  patch(ways[way_id], rules_root)
for relation_id in relations:
  patch(relations[relation_id], rules_root)








#if DEBUG:
#  print (etree.tostring(osmpatch,pretty_print=True, encoding='unicode'))

string=etree.tostring(osm, xml_declaration=True, encoding='UTF-8', pretty_print=True )
f=open(out_file,"w+")
f.write(string)
f.close

#print ("nodes", nodes)
#for i in nodes:
#  for key in nodes[i].keys():
#    print (key,"=",nodes[i].get(key))


