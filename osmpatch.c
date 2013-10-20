#include <stdio.h>  
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <locale.h>
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

#define SEARCH_OPTIONS_CASE_SENSITIVE_DEFAULT_VALUE FALSE
#define SEARCH_OPTIONS_FULL_MATCH_DEFAULT_VALUE TRUE

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

// если no_case_sensitive=TRUE, то поиск происходит без учёта регистра,
// если full_match=TRUE, то функция возвращает 0 только в случае полного совпадения строк
// в противном случае функция возвращает 0 в случае, если в строке where_search полностью
// присутствует строка what_search
// в случае, если ничего не найдено - возвращается 1
// в случае, если произошла ошибка - возвращается -1
int str_cmp_ext(char *where_search, char *what_search, bool no_case_sensitive, bool full_match)
{
	int return_value=-1;
	wchar_t *where_search_wchar=NULL;
	wchar_t *what_search_wchar=NULL;
	where_search_wchar=(wchar_t*)malloc( (strlen(where_search)+1) * sizeof(wchar_t));
	what_search_wchar=(wchar_t*)malloc( (strlen(what_search)+1) * sizeof(wchar_t));
	if(where_search_wchar!=NULL && what_search_wchar!=NULL)
	{
		if(mbstowcs(where_search_wchar,where_search,strlen(where_search)+1)!=-1 &&
		   mbstowcs(what_search_wchar,what_search,strlen(what_search)+1)!=-1)
		   {
				return_value=wc_cmp_ext(where_search_wchar,what_search_wchar,no_case_sensitive,full_match);
		   }
		   else
				return_value=-1;
	}
	else return_value=-1;
	if(where_search_wchar)free(where_search_wchar);
	if(what_search_wchar)free(what_search_wchar);
	return return_value;
}

// search what_search in where_search (wchar_t)
int wc_cmp_ext(wchar_t *where_search, wchar_t *what_search, bool no_case_sensitive, bool full_match)
{
	int not_equal=1;
	int where_search_index=0, what_search_index=0;
	for(where_search_index=0;where_search_index<wcslen(where_search);where_search_index++)
	{
		if(*(where_search+where_search_index)==*what_search ||
			no_case_sensitive && ( towupper(*(where_search+where_search_index))==towupper(*what_search) )
			)
		{
			not_equal=0;
			for(what_search_index=0;what_search_index<wcslen(what_search);what_search_index++)
			{
				if(where_search_index+what_search_index>wcslen(where_search))
				{
					return 0;
				}
				if( (*(where_search+where_search_index+what_search_index)!=*(what_search+what_search_index) && !no_case_sensitive) ||
					no_case_sensitive && ( towupper(*(where_search+where_search_index+what_search_index) )!= towupper(*(what_search+what_search_index)) ) 
				)
				{
					not_equal=1;
					break;
				}
			}
			if(!not_equal)
			break;
		}
		else
		{
			if(full_match)
				return 1;
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

		// for wchar_t:
		setlocale(LC_ALL, "");
		setlocale(LC_NUMERIC,"C");

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
		xmlAttr * properties = NULL;
		xmlNode * cur_osm_element = NULL;
		xmlNode * cur_osm_tag = NULL;
		xmlNode * cur_node = NULL;
		xmlAttr * cur_prop = NULL;
		xmlChar * rules_str = NULL;
		xmlChar * osm_str = NULL;
		int types_to_find=TYPE_NODE|TYPE_WAYS;
		bool find_osm_element_to_process=FALSE;
		int tags_rules_equal=0;
		/// default search options for keys and values:
		bool no_case_sensitive=SEARCH_OPTIONS_CASE_SENSITIVE_DEFAULT_VALUE;
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

						// process key:
						//get search options for this key:
						no_case_sensitive=SEARCH_OPTIONS_CASE_SENSITIVE_DEFAULT_VALUE;
						full_match=SEARCH_OPTIONS_FULL_MATCH_DEFAULT_VALUE;
						cur_prop=cur_node->properties;
						while(cur_prop)
						{
							if(strcmp(cur_prop->name,"no_case_sensitive")==0)
							{
								if(strcmp(cur_prop->children->content,"yes")==0)
									no_case_sensitive=TRUE;
								else
									no_case_sensitive=FALSE;
							}
							if(strcmp(cur_prop->name,"full_match")==0)
							{
								if(strcmp(cur_prop->children->content,"yes")==0)
									full_match=TRUE;
								else
									full_match=FALSE;
							}
							cur_prop=cur_prop->next;
						}
						
						cur_prop=properties;
						while(cur_prop)
						{
							if(!strcmp(cur_prop->name,"k"))
							{
								// cmp current osm tag key with current rules key:
								if(cur_prop->children)
								{
									osm_str=cur_prop->children->content;
									break;
								}
								else
								{
									fprintf(stderr,"%s:%i: error processing XML! 'k' attr have no content!",__FILE__,__LINE__);
									return -1;
								}
							}
							cur_prop=cur_prop->next;
						}

						if(cur_node->children)
						{
							rules_str=cur_node->children->content;
						}
						else
						{
							fprintf(stderr,"%s:%i: error processing XML! 'key' tag have no content!",__FILE__,__LINE__);
							return -1;
						}
						if(rules_str!=NULL && osm_str!=NULL)
						{
							if(str_cmp_ext(osm_str,rules_str,no_case_sensitive,full_match)==0)
							{
								// find tag!
#ifdef DEBUG
								fprintf(stderr,"find tag!: Find '%s' in '%s' - now will be test value of tag...\n", rules_str, osm_str);  
#endif
							}
							else
							{
#ifdef DEBUG
								fprintf(stderr,"Not find tag!: Not find '%s' in '%s' - skip this tag\n", rules_str, osm_str);  
#endif
								cur_rules_tag=cur_rules_tag->next;
								continue;
							}
						}




						// process value
	


						cur_rules_tag=cur_rules_tag->next;
					}
					///////////////////

				}
				cur_osm_tag=cur_osm_tag->next;
			}
			cur_osm_element=cur_osm_element->next;
		}
		return 0;
}
