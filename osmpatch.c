#include <stdio.h>  
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>

#ifndef false
#define false 0
#endif
#ifndef true
#define true 1
#endif

enum{
	GEO_NOTDEFINED,
	GEO_POINT,
	GEO_POLY,
	GEO_LINE
};
//#define POI 0
//#define POLY 1

#ifndef bool
#define bool int
#endif

struct pointList
{
	long double lat;
	long double lon;
	int id;
	bool link;
	struct pointList *next;
};

static void process_rules(xmlNode * a_node, xmlNode * rules);  
int patch_by_rules(xmlNode * a_node,xmlNode * rules);
xmlNode* find_tag(xmlNode * node, char *tag_name);

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
		process_rules(osm_root_element,rules_root_element);  

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
		xmlSaveFileEnc("out.osm", osm, "UTF-8");

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

static void  
process_rules(xmlNode * osm, xmlNode * rules)  
{  
		xmlNode *cur_osmpatch = NULL;  
		xmlNode *cur_patchset = NULL;  

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
				patch_by_rules(osm, cur_patchset->children);
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
		xmlNode * cur_tag = NULL;

		find_node=find_tag(rules,"find");
		if(find_node)
		{
#ifdef DEBUG
			fprintf(stderr,"%s:%i: Found find!\n",__FILE__,__LINE__);
#endif
			cur_tag=find_node->children;
			while(cur_tag=find_tag(cur_tag,"tag"))
			{
#ifdef DEBUG
				fprintf(stderr,"%s:%i: Found tag!\n",__FILE__,__LINE__);
#endif
				cur_tag=cur_tag->next;
			}
		}
		else
		{
			fprintf(stderr,"%s:%i: Can not find tag <find>!\n",__FILE__,__LINE__);
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
  <node id='%i' action='modify' visible='true' lat='%Lf' lon='%Lf' />\n",posList_cur->id,posList_cur->lat,posList_cur->lon);
					}
		#ifdef DEBUG
					else
						fprintf(stderr,"%s:%i: Found link!\n",__FILE__,__LINE__);
		#endif
					posList_cur=posList_cur->next;
				}
				// 2. show head way:
				fprintf(stdout,"\
		  <way id='%i' action='modify' visible='true'>\n",cur_global_id);
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
  <node id='%i' action='modify' visible='true' lat='%Lf' lon='%Lf' >\n",posList_cur->id,posList_cur->lat,posList_cur->lon);
					
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
	int lat_success=false;
	int lon_success=false;
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
	int lat_success=false;
	int lon_success=false;
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
				lon_success=true;
				*cur_coord=0;
				cur_coord=lat;
				index=0;
				continue;
			}
			else
			{
				lat_success=true;
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
			lat_success=false;
			lon_success=false;
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
					(*posList_last)->link=true;
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
