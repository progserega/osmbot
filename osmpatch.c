#include <stdio.h>  
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#ifndef bool
#define bool int
#endif

#define SEARCH_OPTIONS_CASE_SENSITIVE_DEFAULT_VALUE false
#define SEARCH_OPTIONS_FULL_MATCH_DEFAULT_VALUE true

enum{
	TYPE_NODE = 1,
	TYPE_WAYS = 2
};
#define POI_TYPE int

struct pointList
{
	long double lat;
	long double lon;
	int id;
	bool link;
	struct pointList *next;
};

int process_rules(xmlNode * a_node, xmlNode * rules);  
int patch_by_rules(xmlNode * a_node,xmlNode * rules);

xmlNode* find_tag(xmlNode * node, char *tag_name)
{
	// process <find>
	xmlNode * cur_node = NULL;

	if(node)
	{
		for (cur_node = node; cur_node; cur_node = cur_node->next) 
		{  
			if (cur_node->type == XML_ELEMENT_NODE)
			{  
				// find <find>
				if (strcmp(cur_node->name,tag_name)==0)
				{
					return cur_node;
				}
			}
		}
	}
	return NULL;
}

int wc_cmp(wchar_t *s1, wchar_t *s2)
{
	int not_equal=1;
	int s1_index=0, s2_index=0;
	for(s1_index=0;s1_index<wcslen(s1);s1_index++)
	{
		if(*(s1+s1_index)==*s2)
		{
			not_equal=0;
			for(s2_index=0;s2_index<wcslen(s2);s2_index++)
			{
				if(s1_index+s2_index>wcslen(s1))
				{
					return 0;
				}
				if(*(s1+s1_index+s2_index)!=*(s2+s2_index))
				{
					not_equal=1;
					break;
				}
			}
			if(!not_equal)
			break;
		}
	}
	return not_equal;
}

static void parsing_Polygon(xmlNode * a_node);
int parsing_posList(xmlChar * text);
void free_posList(struct pointList *List);

struct pointList *posList=0;
int posList_num=0;
struct pointList **posList_last=&posList;
struct pointList *posList_cur=0;
int cur_global_id=-1;

int  
main(int argv,char**argc)  
{  

		xmlDoc         *rules = NULL;  
		xmlDoc         *osm = NULL;  
		xmlNode        *rules_root_element = NULL;  
		xmlNode        *osm_root_element = NULL;  
		const char     *rules_file = "rules.xml";  
		const char     *osm_file = "in.osm";  
		int exit_status=0;
		/*if (argv<3)
		{
			fprintf(stderr, "need 2 parametr - osmpatch_rules.xml and input osm-file! exit!");
			exit -1;
		}
*/
		rules = xmlReadFile(rules_file, NULL, 0);  
		if (rules == NULL)  
		{  
				fprintf(stderr,"%s:%i: error: could not parse file %s\n", __FILE__,__LINE__, rules_file);  
				exit_status=1;
				goto exit;
		}  
		else  
		{  

				rules_root_element = xmlDocGetRootElement(rules);  
				if(!rules_root_element)
				{
					fprintf(stderr,"%s:%i: error: can not get root xml element from rules-file: %s\n", __FILE__,__LINE__, rules_file);  
					exit_status=1;
					goto exit;
				}
		}  
		osm = xmlReadFile(osm_file, NULL, 0);  
		if (osm == NULL)  
		{  
				fprintf(stderr,"%s:%i: error: could not parse file %s\n", __FILE__,__LINE__, osm_file);  
				exit_status=1;
				goto exit;
		}  
		else  
		{  
				osm_root_element = xmlDocGetRootElement(osm);  
				if(!osm_root_element)
				{
					fprintf(stderr,"%s:%i: error: can not get root xml element from osm-file: %s\n", __FILE__,__LINE__, osm_file);  
					exit_status=1;
					goto exit;
				}
		}  
		// process each element from osm, test it by tags from rules:
		if(!process_rules(osm_root_element,rules_root_element))
			xmlSaveFileEnc("out.osm", osm, "UTF-8");
		else
			fprintf(stderr,"%s:%i: error: process rules! Dont write out xml\n", __FILE__,__LINE__);  


/*
/////////////////////////////////
		xmlNode *cur_node = NULL;  
		cur_node=osm->children;
		xmlNodePtr pNode = xmlNewNode(0, (xmlChar*)"newNodeName");
		xmlNodeSetContent(pNode, (xmlChar*)"content");
		//xmlAddChild(pParentNode, pNode);
		xmlAddChild(cur_node, pNode);
		//xmlDocSetRootElement(pDoc, pParentNode);
		//xmlDocSetRootElement(osm, osm_root_element);
/////////////////////////////////
		*/

exit:
		if(rules)xmlFreeDoc(rules);
		if(osm)xmlFreeDoc(osm);  


		/* 
		*          *Free the global variables that may 
		*                   *have been allocated by the parser. 
		*                            */  
		xmlCleanupParser();  

		return (exit_status);  
}  

/* Recursive function that prints the XML structure */  

int process_rules(xmlNode * osm, xmlNode * rules)  
{  
		xmlNode *cur_osmpatch = NULL;  
		xmlNode *cur_patchset = NULL;  
		xmlNode *first_osm = NULL;  

		first_osm=find_tag(osm,"osm");
		if(!first_osm)
		{
			fprintf(stderr,"%s:%i: Can not found <osm> tag in osm-file! Exit!\n",__FILE__,__LINE__);
			return -1;
		}
		cur_osmpatch=find_tag(rules,"osmpatch");
		if(cur_osmpatch)
		{
#ifdef DEBUG
			fprintf(stderr,"%s:%i: Found osmpatch!\n",__FILE__,__LINE__);
#endif
			cur_patchset=cur_osmpatch->children;
			while(cur_patchset=find_tag(cur_patchset,"patchset"))
			{
#ifdef DEBUG
				fprintf(stderr,"%s:%i: Found patchset!\n",__FILE__,__LINE__);
#endif
				// process all patchsets in rules.xml:
				if(patch_by_rules(first_osm, cur_patchset->children))
					return -1;
				cur_patchset=cur_patchset->next;
			}
		}
		else
		{
			fprintf(stderr,"%s:%i: Can not find tag <osmpatch>!\n",__FILE__,__LINE__);
		}
} 

int patch_by_rules(xmlNode * osm, xmlNode *rules)
{
		// process <find>
		xmlNode * find_node = NULL;
		xmlNode * cur_rules_tag = NULL;
		xmlNode * type = NULL;
		xmlNode * properties = NULL;
		xmlNode * cur_osm_element = NULL;
		xmlNode * cur_osm_tag = NULL;
		xmlNode * cur_node = NULL;
		xmlNode * cur_prop = NULL;
		int types_to_find=TYPE_NODE|TYPE_WAYS;
		bool find_osm_element_to_process=NULL;
		int tags_rules_equal=NULL;
		/// default search options for keys and values:
		bool case_sensitive=SEARCH_OPTIONS_CASE_SENSITIVE_DEFAULT_VALUE;
		bool full_match=SEARCH_OPTIONS_FULL_MATCH_DEFAULT_VALUE;



		find_node=find_tag(rules,"find");
		if(find_node)
		{
#ifdef DEBUG
			fprintf(stderr,"%s:%i: Found find!\n",__FILE__,__LINE__);
#endif
			// find type elements to search:
			type=find_tag(find_node->children,"type");
			if(type)
			{
				properties=type->properties;
				while(properties)
				{
					// set types:
					if(properties)
					{
						if(strcmp(properties->name,"nodes")==0)
						{
							if(strcmp(properties->children->content,"yes")==0)
							{
								types_to_find|=TYPE_NODE;
							}
							else
								types_to_find^=TYPE_NODE;
						}
						if(strcmp(properties->name,"ways")==0)
						{
							if(strcmp(properties->children->content,"yes")==0)
							{
								types_to_find|=TYPE_WAYS;
							}
							else
								types_to_find^=TYPE_WAYS;
						}
					}
					properties=properties->next;
				}
				if(!types_to_find)
				{
					fprintf(stderr,"%s:%i: No type of osm element is selected for search! Exit!\n",__FILE__,__LINE__);
					return -1;
				}
#ifdef DEBUG
				fprintf(stderr,"%s:%i: Found type! Type=%i\n",__FILE__,__LINE__,types_to_find);
#endif
			}
		}
		else
		{
			fprintf(stderr,"%s:%i: Can not find tag <find>!\n",__FILE__,__LINE__);
			return -1;
		}

		// Process each element in osm-xml (nodes, ways ...)
		cur_osm_element=osm->children;
		while(cur_osm_element)
		{
			if(types_to_find&TYPE_NODE)
			{
				if(strcmp(cur_osm_element->name,"node")==0)
					find_osm_element_to_process=1;
			}
			else if(types_to_find&TYPE_WAYS)
			{
				if(strcmp(cur_osm_element->name,"way")==0)
					find_osm_element_to_process=1;
			}
			else
			{
				fprintf(stderr,"%s:%i: No type of osm element is selected for search! Exit!\n",__FILE__,__LINE__);
				return -1;
			}
			if(!find_osm_element_to_process)
			{
				cur_osm_element=cur_osm_element->next;
				continue;
			}

			tags_rules_equal=0;
			cur_osm_tag=cur_osm_element->children;
			while(cur_osm_tag)
			{
				tags_rules_equal++;
				properties=cur_osm_tag->properties;
				if(properties)
				{
					cur_rules_tag=find_node->children;
					// cmp current osm-element with each find-rules:
					while(cur_rules_tag=find_tag(cur_rules_tag,"tag"))
					{
		#ifdef DEBUG
						fprintf(stderr,"%s:%i: Found tag!\n",__FILE__,__LINE__);
		#endif
						
						// process current rules tag:
						cur_node=find_tag(cur_rules_tag->children,"key");
						//get search options for this key:
						case_sensitive=SEARCH_OPTIONS_CASE_SENSITIVE_DEFAULT_VALUE;
						full_match=SEARCH_OPTIONS_FULL_MATCH_DEFAULT_VALUE;
						cur_prop=cur_node->properties;
						while(cur_prop)
						{
							if(strcmp(cur_prop->name,"case_sensitive")==0)
							{
								if(strcmp(cur_prop->value,"yes")==0)
									case_sensitive=true;
								else
									case_sensitive=false;
							}
							if(strcmp(cur_prop->name,"full_match")==0)
							{
								if(strcmp(cur_prop->value,"yes")==0)
									full_match=true;
								else
									full_match=false;
							}
							cur_prop=cur_prop->next;
						}
						



						cur_node=find_tag(cur_rules_tag->children,"value");
	


						cur_rules_tag=cur_rules_tag->next;
					}
					///////////////////

				}
				cur_osm_tag=cur_osm_tag->next;
			}

			cur_osm_tag=cur_osm_tag->next;
		}

/*

					name=xmlNodeGetContent(cur_node);  
				}
				else if (strcmp(cur_node->name,"description")==0)
				{
#ifdef DEBUG
					fprintf(stderr,"description: %s\n", xmlNodeGetContent(cur_node));  
#endif
					description=xmlNodeGetContent(cur_node);  
				}
				else if (strcmp(cur_node->name,"Polygon")==0)
				{
					GeoObject_type=GEO_POLY;
					parsing_Polygon(cur_node->children);
				}
				else if (strcmp(cur_node->name,"LineString")==0)
				{
					GeoObject_type=GEO_LINE;
					parsing_Polygon(cur_node->children);
				}
				else if (strcmp(cur_node->name,"Point")==0)
				{
					GeoObject_type=GEO_POINT;
					parsing_Polygon(cur_node->children);
				}
			*/
		/*
		// show current GeoObject as OSM:
		
		if(GeoObject_type==GEO_POLY||GeoObject_type==GEO_LINE)
		{
				//1. show points 
				if(!posList_num)
				{
						fprintf(stderr,"%s:%i: No coordinates for %s! Skip this object!\n",__FILE__,__LINE__,name);
						free_posList(posList);
						return -1;
				}
				posList_cur=posList;
				while(posList_cur)
				{
					if(!posList_cur->link)
					{
							fprintf(stdout,"\
  <node id='%i' action='modify' visible='TRUE' lat='%Lf' lon='%Lf' />\n",posList_cur->id,posList_cur->lat,posList_cur->lon);
					}
		#ifdef DEBUG
					else
						fprintf(stderr,"%s:%i: Found link!\n",__FILE__,__LINE__);
		#endif
					posList_cur=posList_cur->next;
				}
				// 2. show head way:
				fprintf(stdout,"\
		  <way id='%i' action='modify' visible='TRUE'>\n",cur_global_id);
				// 3. show links to nodes in current way:
				posList_cur=posList;
				while(posList_cur)
				{
					fprintf(stdout,"<nd ref='%i' />\n",posList_cur->id);
					posList_cur=posList_cur->next;
				}
				// 4. show tags:
				// TODO: add dinamical translate tags
				if(description)
					fprintf(stdout,"    <tag k='description' v='%s' />\n",description);
				if(name)
					fprintf(stdout,"    <tag k='name' v='%s' />\n",name);
				if(note)
					fprintf(stdout,"    <tag k='note' v='%s' />\n",note);
				if(operator)
					fprintf(stdout,"    <tag k='operator' v='%s' />\n",operator);
				// add type by name:
				if(!strncmp("ПС",name,sizeof("ПС")-1))
				{
						fprintf(stdout,"    <tag k='power' v='station' />\n");
				}
				else if(!strncmp("КВЛ",name,sizeof("КВЛ")-1)||!strncmp("ВЛ",name,sizeof("ВЛ")-1)||!strncmp("КЛ",name,sizeof("КЛ")-1))
				{
						fprintf(stdout,"    <tag k='power' v='line' />\n");
				}

				//<tag k='ref' v='№45' />
				// 5. close GeoObject object:
				fprintf(stdout,"  </way>\n");
				cur_global_id--;
		}
		else if(GeoObject_type==GEO_POINT)
		{
				if(posList_num<1)
				{
					fprintf(stderr,"%s:%i: No coordinates for POI - skip!\n",__FILE__,__LINE__);
					return -1;
				}
				//1. show points 
				posList_cur=posList;
				fprintf(stdout,"\
  <node id='%i' action='modify' visible='TRUE' lat='%Lf' lon='%Lf' >\n",posList_cur->id,posList_cur->lat,posList_cur->lon);
					
				// 2. show tags:
				// TODO: add dinamical translate tags
				if(description)
					fprintf(stdout,"    <tag k='description' v='%s' />\n",description);
				if(name)
					fprintf(stdout,"    <tag k='name' v='%s' />\n",name);
				if(note)
					fprintf(stdout,"    <tag k='note' v='%s' />\n",note);
				if(operator)
					fprintf(stdout,"    <tag k='operator' v='%s' />\n",operator);
				// add type by name:
				if(!strncmp("ПС",name,sizeof("ПС")-1))
				{
						fprintf(stdout,"    <tag k='power' v='station' />\n");
				}
				else if(!strncmp("КВЛ",name,sizeof("КВЛ")-1)||!strncmp("ВЛ",name,sizeof("ВЛ")-1))
				{
						fprintf(stdout,"    <tag k='power' v='line' />\n");
				}

				//<tag k='ref' v='№45' />
				// 5. close GeoObject object:
				fprintf(stdout,"  </node>\n");
		}
		// 5. free posList:
		free_posList(posList);
		*/
		return 0;
}
static void parsing_Polygon(xmlNode * a_node)
{
	xmlNode *cur_node = NULL;
	xmlChar *text = NULL;
	xmlChar *cur_text = NULL;
	xmlChar lat[255];
	xmlChar lon[255];
	xmlChar* cur_coord=NULL;
	int lat_success=FALSE;
	int lon_success=FALSE;
	int index=0;

	for (cur_node = a_node; cur_node; cur_node = cur_node->next) 
	{  
		if (cur_node->type == XML_ELEMENT_NODE)
		{
				if (strcmp(cur_node->name,"posList")==0||strcmp(cur_node->name,"pos")==0)
				{
					// Берём координаты полигона:
					text=xmlNodeGetContent(cur_node);
					if(parsing_posList(text)==-1)
					{
						fprintf(stderr,"%s:%i: Error parsing_posList()!\n",__FILE__,__LINE__);
					}
				}
				parsing_Polygon(cur_node->children);
		}
	}
}

int parsing_posList(xmlChar * text)
{
	xmlNode *cur_node = NULL;
	xmlChar *cur_text = NULL;
	xmlChar lat[255];
	xmlChar lon[255];
	xmlChar* cur_coord=NULL;
	int lat_success=FALSE;
	int lon_success=FALSE;
	int index=0;
	if(*posList_last)
	{
		fprintf(stderr,"%s:%i: Error! List not free! (posList_last!=NULL) It is memmory leak!\n",__FILE__,__LINE__);
		return -1;
	}
			
	cur_coord=lon;
	for(cur_text=text;cur_text<=text+strlen(text);cur_text++)
	{
		if(*cur_text==' '||*cur_text==0)
		{
			// переключатель на другую координату:
			if(!lon_success)
			{
				lon_success=TRUE;
				*cur_coord=0;
				cur_coord=lat;
				index=0;
				continue;
			}
			else
			{
				lat_success=TRUE;
				*cur_coord=0;
				index=0;
			}
		}
		if(!lat_success || !lon_success)
		{
			*cur_coord=(*cur_text);
			cur_coord++;
			index++;
			if(index>=255)
			{
				fprintf(stderr,"%s:%i: massiv error!\n",__FILE__,__LINE__);
				break;
			}
		}
		else
		{
			// устанавливаем переменные для выбора второй пары:
			index=0;
			cur_coord=lon;
			lat_success=FALSE;
			lon_success=FALSE;
			//cur_text--;

			// Формируем связный список из полученных точек:
			// выделяем память под элемент списка:
			*posList_last=malloc(sizeof(struct pointList));
			if(!*posList_last)
			{
				fprintf(stderr,"%s:%i: Error aalocate %i bytes!\n",__FILE__,__LINE__,sizeof(struct pointList));
				break;
			}
			memset(*posList_last,0,sizeof(struct pointList));
			posList_num++;
			// Заполняем структуру:
			sscanf(lat,"%Lf",&((*posList_last)->lat));
			sscanf(lon,"%Lf",&((*posList_last)->lon));
			(*posList_last)->id=cur_global_id;
			cur_global_id--;
			// проверяем, были ли такие же точки в данной записи (закольцованность)
			posList_cur=posList;
			while(posList_cur)
			{
				if( (*posList_last)->lat==posList_cur->lat & (*posList_last)->lon==posList_cur->lon & *posList_last !=posList_cur  )
				{	
					(*posList_last)->id=posList_cur->id;
					(*posList_last)->link=TRUE;
					// отменяем уменьшение идентификатора на копию точки:
					cur_global_id++;
#ifdef DEBUG
					fprintf(stderr,"%s:%i: Found link!\nid=%i",__FILE__,__LINE__,posList_cur->id);
#endif
					break;
					
				}
				posList_cur=posList_cur->next;
			}
#ifdef DEBUG
			fprintf(stderr,"str lat: %s\n",lat);
			fprintf(stderr,"float lat: %Lf\n",(*posList_last)->lat);
			fprintf(stderr,"str lon: %s\n",lon);
			fprintf(stderr,"float lon: %Lf\n",(*posList_last)->lon);
			fprintf(stderr,"id: %i\n",(*posList_last)->id);
#endif
			posList_last=&(*posList_last)->next;

		}
	}
	return 0;
}
void free_posList(struct pointList *List)
{
	struct pointList *tmp;
	while(List)
	{
		tmp=List;
		List=List->next;
		free(tmp);
	}
	posList=0;
	posList_num=0;
	posList_last=&posList;
	posList_cur=0;
}
