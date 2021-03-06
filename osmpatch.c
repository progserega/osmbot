#include <stdio.h>  
#include <unistd.h>
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

#define SEARCH_OPTIONS_CASE_SENSITIVE_DEFAULT_VALUE TRUE
#define SEARCH_OPTIONS_FULL_MATCH_DEFAULT_VALUE TRUE
#define SEARCH_OPTIONS_SKIP_ELEMNT_DEFAULT_VALUE FALSE

enum{
	RETURN_VALUE_ERROR=-1,
	RETURN_VALUE_FIND_SUCCESS=0,
	RETURN_VALUE_SUCCESS=0,
	RETURN_VALUE_FIND_FALSE=1
};

enum{
	TYPE_NODE = 1,
	TYPE_WAYS = 2,
	TYPE_RELATION = 4
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

// files for processing:
char     *rules_file = NULL;  
char     *osm_in_file = NULL;  
char     *osm_out_file = NULL;  

int process_patchsets(xmlNode * a_node, xmlNode * rules);  
int process_rules(xmlNode * a_node,xmlNode * rules);

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

// если case_sensitive=FALSE, то поиск происходит без учёта регистра,
// если full_match=TRUE, то функция возвращает 0 только в случае полного совпадения строк
// в противном случае функция возвращает 0 в случае, если в строке where_search полностью
// присутствует строка what_search
// в случае, если ничего не найдено - возвращается 1
// в случае, если произошла ошибка - возвращается -1
int str_cmp_ext(char *where_search, char *what_search, bool case_sensitive, bool full_match)
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
				return_value=wc_cmp_ext(where_search_wchar,what_search_wchar,case_sensitive,full_match);
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
int wc_cmp_ext(wchar_t *where_search, wchar_t *what_search, bool case_sensitive, bool full_match)
{
	int not_equal=1;
	int where_search_index=0, what_search_index=0;

	if(full_match==TRUE && wcslen(where_search)!=wcslen(what_search))
		return 1;

	for(where_search_index=0;where_search_index<wcslen(where_search);where_search_index++)
	{
		if(*(where_search+where_search_index)==*what_search ||
			case_sensitive==FALSE && ( towupper(*(where_search+where_search_index))==towupper(*what_search) )
			)
		{
			not_equal=0;
			for(what_search_index=0;what_search_index<wcslen(what_search);what_search_index++)
			{
				if(where_search_index+what_search_index>wcslen(where_search))
				{
					return 0;
				}
				if( (*(where_search+where_search_index+what_search_index)!=*(what_search+what_search_index) && case_sensitive==TRUE) ||
					case_sensitive==FALSE && ( towupper(*(where_search+where_search_index+what_search_index) )!= towupper(*(what_search+what_search_index)) ) 
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
main(int argc,char**argv)  
{  

		xmlDoc         *rules = NULL;  
		xmlDoc         *osm = NULL;  
		xmlNode        *rules_root_element = NULL;  
		xmlNode        *osm_root_element = NULL;  
		int exit_status=0;

		// for wchar_t:
		setlocale(LC_ALL, "");
		setlocale(LC_NUMERIC,"C");

		if(process_command_options(argc,argv)!=0)
		{
			fprintf(stderr, "need 3 parametr: rules-file, input and output files.\nUse -h for help.\nexit!\n");
			return -1;
		}
		if (!osm_in_file||!osm_out_file||!rules_file)
		{
			fprintf(stderr, "need 3 parametr: rules-file, input and output files.\nUse -h for help.\nexit!\n");
			return -1;
		}


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
		osm = xmlReadFile(osm_in_file, NULL, 0);  
		if (osm == NULL)  
		{  
				fprintf(stderr,"%s:%i: error: could not parse file %s\n", __FILE__,__LINE__, osm_in_file);  
				exit_status=1;
				goto exit;
		}  
		else  
		{  
				osm_root_element = xmlDocGetRootElement(osm);  
				if(!osm_root_element)
				{
					fprintf(stderr,"%s:%i: error: can not get root xml element from osm-file: %s\n", __FILE__,__LINE__, osm_in_file);  
					exit_status=1;
					goto exit;
				}
		}  
		// process each element from osm, test it by tags from rules and patch osm by rules:
		if(!process_patchsets(osm_root_element,rules_root_element))
			xmlSaveFileEnc(osm_out_file, osm, "UTF-8");
		else
		{
			fprintf(stderr,"%s:%i: error: process rules! Dont write out xml file '%s'\n", __FILE__,__LINE__,osm_out_file);  
			exit_status=-1;
		}


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

int process_patchsets(xmlNode * osm, xmlNode * rules)  
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
			if(process_rules(first_osm, cur_patchset->children))
			{
#ifdef DEBUG
				fprintf(stderr,"%s:%i: Error process_rules()!\n",__FILE__,__LINE__);
#endif
				return -1;
			}
			cur_patchset=cur_patchset->next;
		}
	}
	else
	{
		fprintf(stderr,"%s:%i: Can not find tag <osmpatch>!\n",__FILE__,__LINE__);
		return -1;
	}
	return 0;
} 

int process_rules(xmlNode * osm, xmlNode *rules)
{
	// process <find>
	xmlNode * find_node = NULL;
	xmlNode * cur_rules_tag = NULL;
	xmlNode * type = NULL;
	xmlAttr * properties = NULL;
	xmlNode * cur_osm_element = NULL;
	int types_to_find=TYPE_NODE|TYPE_WAYS|TYPE_RELATION;
	bool find_osm_element_to_process=FALSE;
	bool skip_osm_element=FALSE;
	int tags_rules_equal=0;
	int tags_rules_num=0;

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
					else if(strcmp(properties->name,"ways")==0)
					{
						if(strcmp(properties->children->content,"yes")==0)
						{
							types_to_find|=TYPE_WAYS;
						}
						else
							types_to_find^=TYPE_WAYS;
					}
					else if(strcmp(properties->name,"relation")==0)
					{
						if(strcmp(properties->children->content,"yes")==0)
						{
							types_to_find|=TYPE_RELATION;
						}
						else
							types_to_find^=TYPE_RELATION;
					}
				}
				properties=properties->next;
			}
			if(!types_to_find)
			{
				fprintf(stderr,"%s:%i: No type of osm element is selected for search! Exit!\n",__FILE__,__LINE__);
				return RETURN_VALUE_ERROR;
			}
#ifdef DEBUG
			fprintf(stderr,"%s:%i: Found type! Type=%i\n",__FILE__,__LINE__,types_to_find);
#endif
		}
	}
	else
	{
		fprintf(stderr,"%s:%i: Can not find tag <find>!\n",__FILE__,__LINE__);
		return RETURN_VALUE_ERROR;
	}

	// Process each element in osm-xml (nodes, ways ...)
	cur_osm_element=osm->children;
	while(cur_osm_element)
	{
		find_osm_element_to_process=0;
		// check current OSM element for searched types:
		do
		{
			if(types_to_find&TYPE_NODE)
			{
				if(strcmp(cur_osm_element->name,"node")==0)
				{
					find_osm_element_to_process=1;
					break;
				}
			}
			if(types_to_find&TYPE_WAYS)
			{
				if(strcmp(cur_osm_element->name,"way")==0)
				{
					find_osm_element_to_process=1;
					break;
				}
			}
			if(types_to_find&TYPE_RELATION)
			{
				if(strcmp(cur_osm_element->name,"relation")==0)
				{
					find_osm_element_to_process=1;
					break;
				}
			}
		}while(0);
		if(!find_osm_element_to_process)
		{
			cur_osm_element=cur_osm_element->next;
			continue;
		}
		// process current OSM element:
		//


		// test each rules for current osm element:
		//
		// flag to test success all find rules:
		skip_osm_element=FALSE;
		cur_rules_tag=find_node->children;
		// cmp current osm-element with each find-rules:
		while(cur_rules_tag=find_tag(cur_rules_tag,"tag"))
		{
			// process current rules tag:

			switch(process_osm_tags_by_current_rule(cur_osm_element, cur_rules_tag))
			{
				case RETURN_VALUE_FIND_SUCCESS:
					// success find current rule in tags of current OSM-element
					// so, test next rules
					break;
				case RETURN_VALUE_FIND_FALSE:
					// not find current rule in tags of current OSM-element
					// so, skip this OSM-element:
					skip_osm_element=TRUE;
					break;
				default:
					// error
					return RETURN_VALUE_ERROR;
					break;
			}
			if(skip_osm_element)
			{
				// in this OSM-element we do not find current rule - skip this OSM-element:
				break;
			}
			cur_rules_tag=cur_rules_tag->next;
		}
		if(!skip_osm_element)
		{
			// current OSM-element success tested by current patchset->find ruleses
			// now we can patch thist element by "add,delete,replace" releses in this
			// patchset:
			//
			if(patch_osm_element_by_patchset(cur_osm_element,rules)==RETURN_VALUE_ERROR)
			{
				return RETURN_VALUE_ERROR;
			}
		}
		cur_osm_element=cur_osm_element->next;
	}
	return RETURN_VALUE_FIND_SUCCESS;
}

int process_osm_tags_by_current_rule(xmlNode* osm_element, xmlNode* rules_tag)
{

	/// default search options for keys and values:
	bool search_options_skip_element=SEARCH_OPTIONS_SKIP_ELEMNT_DEFAULT_VALUE;
	bool search_options_case_sensitive=SEARCH_OPTIONS_CASE_SENSITIVE_DEFAULT_VALUE;
	bool search_options_full_match=SEARCH_OPTIONS_FULL_MATCH_DEFAULT_VALUE;
	xmlAttr * properties = NULL;
	xmlAttr * cur_prop = NULL;
	xmlNode * cur_osm_tag = NULL;
	xmlNode * cur_node = NULL;
	xmlChar * rules_str = NULL;
	xmlChar * osm_str = NULL;

	// get search options for this rules tag:
	cur_prop=rules_tag->properties;
	while(cur_prop)
	{
		if(strcmp(cur_prop->name,"skip")==0)
		{
			if(strcmp(cur_prop->children->content,"yes")==0)
				search_options_skip_element=TRUE;
			else
				search_options_skip_element=FALSE;
		}
		cur_prop=cur_prop->next;
	}


	// test each osm tags in current osm-element:
	cur_osm_tag=osm_element->children;
	while(cur_osm_tag=find_tag(cur_osm_tag,"tag"))
	{
		properties=cur_osm_tag->properties;
		if(properties)
		{
			// process key:
			cur_node=find_tag(rules_tag->children,"key");
			//get search options for this key:
			search_options_case_sensitive=SEARCH_OPTIONS_CASE_SENSITIVE_DEFAULT_VALUE;
			search_options_full_match=SEARCH_OPTIONS_FULL_MATCH_DEFAULT_VALUE;
			cur_prop=cur_node->properties;
			while(cur_prop)
			{
				if(strcmp(cur_prop->name,"case_sensitive")==0)
				{
					if(strcmp(cur_prop->children->content,"yes")==0)
						search_options_case_sensitive=TRUE;
					else
						search_options_case_sensitive=FALSE;
				}
				if(strcmp(cur_prop->name,"full_match")==0)
				{
					if(strcmp(cur_prop->children->content,"yes")==0)
						search_options_full_match=TRUE;
					else
						search_options_full_match=FALSE;
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
						return RETURN_VALUE_ERROR;
					}
				}
				cur_prop=cur_prop->next;
			}
			if(osm_str==NULL)
			{
				
#ifdef DEBUG
				fprintf(stderr,"%s:%i: node have not 'k' value - skip this node\n",__FILE__,__LINE__, rules_str, osm_str);  
#endif
				cur_osm_tag=cur_osm_tag->next;
				continue;
			}

			if(cur_node->children)
			{
				rules_str=cur_node->children->content;
			}
			else
			{
				fprintf(stderr,"%s:%i: error processing XML! 'key' tag have no content!",__FILE__,__LINE__);
				return RETURN_VALUE_ERROR;
			}
			if(rules_str!=NULL)
			{
				if(str_cmp_ext(osm_str,rules_str,search_options_case_sensitive,search_options_full_match)==0)
				{
					// find tag!
#ifdef DEBUG
					fprintf(stderr,"%s:%i: Find '%s' in '%s' - now will be test value of tag...\n",__FILE__,__LINE__, rules_str, osm_str);  
#endif
					// then go test value tag...
				}
				else
				{
#ifdef DEBUG
					fprintf(stderr,"%s:%i: Not find '%s' in '%s' - skip this tag\n",__FILE__,__LINE__, rules_str, osm_str);  
#endif
					// we do not find rules tag in this OSM-tag - go to test next OSM-tag...
					cur_osm_tag=cur_osm_tag->next;
					continue;
				}
			}
			else
			{
				fprintf(stderr,"%s:%i: error processing XML! 'rules_str' == NULL\n",__FILE__,__LINE__);
				return RETURN_VALUE_ERROR;
			}

			// now - key is valid, test value:
			// process value
			cur_node=find_tag(rules_tag->children,"value");
			if(cur_node==NULL)
			{
				// in rules tag is only <key>, not <value>. 
				// This mean that all tags osm with 'k'==<key> in rules - is 'success find'	
				// find tag!
#ifdef DEBUG
				fprintf(stderr,"%s:%i: No <value> tag - then all value is selected as success by this rule.\n",__FILE__,__LINE__);  
#endif
				if(search_options_skip_element)
				{
					// skip this OSM element, by 'skip=yes' in rules tag:
#ifdef DEBUG
					fprintf(stderr,"%s:%i: Skip element, becouse found rules tag with 'skip=yes' attr. Value last tag in OSM='%s'\n",__FILE__,__LINE__, osm_str);  
#endif
					return RETURN_VALUE_FIND_FALSE;
					break;
				}
				else
				{
					// current OSM tag is success by current rule, so return "success_find"
#ifdef DEBUG
					fprintf(stderr,"%s:%i: Succes 'find rules'\n",__FILE__,__LINE__);  
#endif
					return RETURN_VALUE_FIND_SUCCESS;
				}

			}
			
			//get search options for this value:
			search_options_case_sensitive=SEARCH_OPTIONS_CASE_SENSITIVE_DEFAULT_VALUE;
			search_options_full_match=SEARCH_OPTIONS_FULL_MATCH_DEFAULT_VALUE;
			cur_prop=cur_node->properties;
			while(cur_prop)
			{
				if(strcmp(cur_prop->name,"case_sensitive")==0)
				{
					if(strcmp(cur_prop->children->content,"yes")==0)
						search_options_case_sensitive=TRUE;
					else
						search_options_case_sensitive=FALSE;
				}
				if(strcmp(cur_prop->name,"full_match")==0)
				{
					if(strcmp(cur_prop->children->content,"yes")==0)
						search_options_full_match=TRUE;
					else
						search_options_full_match=FALSE;
				}
				cur_prop=cur_prop->next;
			}
			
			cur_prop=properties;
			while(cur_prop)
			{
				if(!strcmp(cur_prop->name,"v"))
				{
					// cmp current osm tag key with current rules key:
					if(cur_prop->children)
					{
						osm_str=cur_prop->children->content;
						break;
					}
					else
					{
						fprintf(stderr,"%s:%i: error processing XML! 'v' attr have no content!",__FILE__,__LINE__);
						return RETURN_VALUE_ERROR;
					}
				}
				cur_prop=cur_prop->next;
			}
			if(osm_str==NULL)
			{
				
#ifdef DEBUG
				fprintf(stderr,"%s:%i: node have not 'v' value - skip this node\n",__FILE__,__LINE__, rules_str, osm_str);  
#endif
				cur_osm_tag=cur_osm_tag->next;
				continue;
			}

			if(cur_node->children)
			{
				rules_str=cur_node->children->content;
			}
			else
			{
				fprintf(stderr,"%s:%i: error processing XML! 'value' tag have no content!",__FILE__,__LINE__);
				return RETURN_VALUE_ERROR;
			}
			if(rules_str!=NULL)
			{
				if(str_cmp_ext(osm_str,rules_str,search_options_case_sensitive,search_options_full_match)==0)
				{
					// find tag!
#ifdef DEBUG
					fprintf(stderr,"%s:%i: Find value '%s' in '%s'\n",__FILE__,__LINE__, rules_str, osm_str);  
#endif
					if(search_options_skip_element)
					{
						// skip this OSM element, by 'skip=yes' in rules tag:
#ifdef DEBUG
						fprintf(stderr,"%s:%i: Skip element, becouse found rules tag with 'skip=yes' attr. Value last tag in OSM='%s'\n",__FILE__,__LINE__, osm_str);  
#endif
						return RETURN_VALUE_FIND_FALSE;
					}
					else
					{
#ifdef DEBUG
						fprintf(stderr,"%s:%i: Succes 'find rules'\n",__FILE__,__LINE__ );  
#endif	
						return RETURN_VALUE_FIND_SUCCESS;
					}
				}
				else
				{
					// not find value
					if(search_options_skip_element)
					{
						// not need skip this element
#ifdef DEBUG
						fprintf(stderr,"%s:%i: Succes 'find rules' (with skip=yes options in rules tag). Realy this value is not finded and it is good.\n",__FILE__,__LINE__);  
#endif
						return RETURN_VALUE_FIND_SUCCESS;
					}
					else
					{
						// skip this OSM element, by 'skip=yes' in rules tag:
#ifdef DEBUG
						fprintf(stderr,"%s:%i: Not find value '%s' in '%s' - skip this osm element!\n",__FILE__,__LINE__, rules_str, osm_str);  
#endif
						return RETURN_VALUE_FIND_FALSE;
					}
				}
			}
			else
			{
				fprintf(stderr,"%s:%i: error processing XML! 'rules_str' == NULL\n",__FILE__,__LINE__);
				return RETURN_VALUE_ERROR;
			}
		}
		cur_osm_tag=cur_osm_tag->next;
	}
	if(search_options_skip_element==TRUE)
	{
		// not find this tag, but it is success, becouse this tag must be exclude:
		return RETURN_VALUE_FIND_SUCCESS;
	}
	else
	{
		// not find this rule and it is false:
		return RETURN_VALUE_FIND_FALSE;
	}
}

int patch_osm_element_by_patchset(xmlNode* osm_element, xmlNode* rule)
{
	xmlNode * cur_node = NULL;
	xmlNode * cur_rule = NULL;
	xmlAttr * cur_prop = NULL;
	xmlChar * id_str = NULL;
	xmlChar * key_str = NULL;
	xmlChar * value_str = NULL;
	int return_value = RETURN_VALUE_ERROR;

	// Full equal osm element!
	cur_prop=osm_element->properties;
	while(cur_prop)
	{
		if(!strcmp(cur_prop->name,"id"))
		{
			// cmp current osm tag key with current rules key:
			if(cur_prop->children)
			{
				id_str=cur_prop->children->content;
				break;
			}
			else
			{
				fprintf(stderr,"%s:%i: error processing XML! 'id' attr have no content!",__FILE__,__LINE__);
				return -1;
			}
		}
		cur_prop=cur_prop->next;
	}
	fprintf(stdout,"\nFull equal OSM-element! Start patch this element! Element type=%s Element id=%s element_url=browse/%s/%s/history",osm_element->name, id_str,osm_element->name,id_str);

	// process <add> rules:
	cur_rule=find_tag(rule,"add");
	if(cur_rule!=NULL)
	{
		cur_rule=cur_rule->children;
		while(cur_rule=find_tag(cur_rule,"tag"))
		{
			// get key and value for current new tag:
			cur_prop=cur_rule->properties;
			
			while(cur_prop)
			{
				if(!strcmp(cur_prop->name,"k"))
				{
					// cmp current osm tag key with current rules key:
					if(cur_prop->children)
					{
						key_str=cur_prop->children->content;
					}
					else
					{
						fprintf(stderr,"%s:%i: error processing XML! 'k' attr have no content!",__FILE__,__LINE__);
						return RETURN_VALUE_ERROR;
					}
				}
				else if(!strcmp(cur_prop->name,"v"))
				{
					// cmp current osm tag key with current rules key:
					if(cur_prop->children)
					{
						value_str=cur_prop->children->content;
						break;
					}
					else
					{
						fprintf(stderr,"%s:%i: error processing XML! 'v' attr have no content!",__FILE__,__LINE__);
						return RETURN_VALUE_ERROR;
					}
				}

				cur_prop=cur_prop->next;
			}
			if(key_str==NULL||value_str==NULL)
			{
				fprintf(stderr,"%s:%i: error processing XML! Not find 'k' or 'v' attr in <add> rule!\n",__FILE__,__LINE__);
				return RETURN_VALUE_ERROR;
			}
			else
			{
				// before add tag - remove tag with such key, if it is exist:
				if(delete_tag_from_osm(osm_element,key_str)==RETURN_VALUE_ERROR)
				{
					// error only if error delete, if not delete becouse not find - it not error
					return RETURN_VALUE_ERROR;
				}
				if(add_tag_to_osm(osm_element,key_str,value_str)!=RETURN_VALUE_SUCCESS)
					return RETURN_VALUE_ERROR;
				else
					return_value=RETURN_VALUE_SUCCESS;
			}
			// next tag:
			cur_rule=cur_rule->next;
		}

	}
	
	// process <delete> rules:
	cur_rule=find_tag(rule,"delete");
	if(cur_rule!=NULL)
	{
		cur_rule=cur_rule->children;
		while(cur_rule=find_tag(cur_rule,"tag"))
		{
			// get key and value for current new tag:
			cur_prop=cur_rule->properties;
			
			while(cur_prop)
			{
				if(!strcmp(cur_prop->name,"k"))
				{
					// cmp current osm tag key with current rules key:
					if(cur_prop->children)
					{
						key_str=cur_prop->children->content;
					}
					else
					{
						fprintf(stderr,"%s:%i: error processing XML! 'k' attr have no content!",__FILE__,__LINE__);
						return RETURN_VALUE_ERROR;
					}
				}
				cur_prop=cur_prop->next;
			}
			if(key_str==NULL)
			{
				fprintf(stderr,"%s:%i: error processing XML! Not find 'k' attr in <delete> rule!\n",__FILE__,__LINE__);
				return RETURN_VALUE_ERROR;
			}
			else
			{
				// if such key not found (nothing deleting) - it is not error
				if(delete_tag_from_osm(osm_element,key_str)==RETURN_VALUE_ERROR)
					return RETURN_VALUE_ERROR;
				else
					return_value=RETURN_VALUE_SUCCESS;
			}
			// next tag:
			cur_rule=cur_rule->next;
		}
	}
	return return_value;
}


int delete_tag_from_osm(xmlNode* osm_element, xmlChar* key_str)
{
	xmlNode *cur_osm_tag=NULL;
	xmlNode *tag_to_delete=NULL;
	xmlAttr *properties=NULL;
	xmlAttr * cur_prop = NULL;
	xmlChar *osm_str=NULL;
	int return_status=RETURN_VALUE_FIND_FALSE;

	if(key_str==NULL||osm_element==NULL)
	{
		fprintf(stderr,"%s:%i: error input parameters!\n",__FILE__,__LINE__);  
		return RETURN_VALUE_ERROR;
	}

	// find tag in OSM-element with <tag k="key" ../>

	// test each osm tags in current osm-element:
	cur_osm_tag=osm_element->children;
	while(cur_osm_tag=find_tag(cur_osm_tag,"tag"))
	{
		properties=cur_osm_tag->properties;
		if(properties)
		{
			// process key:
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
						return RETURN_VALUE_ERROR;
					}
				}
				cur_prop=cur_prop->next;
			}
			if(osm_str==NULL)
			{
				
#ifdef DEBUG
				fprintf(stderr,"%s:%i: node have not 'k' value - skip this node\n",__FILE__,__LINE__ );  
#endif
				cur_osm_tag=cur_osm_tag->next;
				continue;
			}
		}

		// cmp current 'k' attr with "key"
		if(str_cmp_ext(osm_str,key_str,FALSE,TRUE)==0)
		{
			// find tag!
#ifdef DEBUG
			fprintf(stderr,"%s:%i: Delete existing <tag k='%s' ... />\n",__FILE__,__LINE__, osm_str );  
#endif
			// delete element:
			tag_to_delete=cur_osm_tag;
			
	/*		// previous -> next
			if(cur_osm_tag->prev)cur_osm_tag->prev->next=cur_osm_tag->next;
			// next -> previous
			if(cur_osm_tag->next)cur_osm_tag->next->prev=cur_osm_tag->prev;
			xmlFreeNode(cur_osm_tag);
*/
			xmlUnlinkNode(tag_to_delete);
			xmlFreeNode(tag_to_delete);

			return RETURN_VALUE_SUCCESS;
		}
	
		// search in next tag:
		cur_osm_tag=cur_osm_tag->next;
	}
	return RETURN_VALUE_FIND_FALSE;
}

int add_tag_to_osm(xmlNode* osm_element, xmlChar* key_str, xmlChar* value_str)
{
	xmlNode * new_node = NULL;
	xmlAttr * new_attr = NULL;

	if(key_str==NULL||value_str==NULL||osm_element==NULL)
	{
		fprintf(stderr,"%s:%i: error input parameters!\n",__FILE__,__LINE__ );  
		return RETURN_VALUE_ERROR;
	}
#ifdef DEBUG
	fprintf(stderr,"%s:%i: Add new <tag k='%s' v='%s' />\n",__FILE__,__LINE__, key_str, value_str );  
#endif

	// write new tag:
	new_node =	xmlNewChild(osm_element, NULL, BAD_CAST "tag", NULL);
	if(new_node==NULL)
	{
		fprintf(stderr,"%s:%i: error xmlNewChild()!\n",__FILE__,__LINE__ );  
		return RETURN_VALUE_ERROR;
	}
		
	if(xmlNewProp(new_node, BAD_CAST "k", key_str)==NULL)
	{
		// new_node free as part of all xml-dom
		fprintf(stderr,"%s:%i: error xmlNewProp()!\n",__FILE__,__LINE__ );  
		return RETURN_VALUE_ERROR;
	}
	if(xmlNewProp(new_node, BAD_CAST "v", value_str)==NULL)
	{
		// new prop from previouse step free as part of all xml-dom
		fprintf(stderr,"%s:%i: error xmlNewProp()!\n",__FILE__,__LINE__ );  
		return RETURN_VALUE_ERROR;
	}
	return RETURN_VALUE_SUCCESS;
}

int process_command_options(int argc, char **argv)
{
	int aflag = 0;
	int bflag = 0;
	char *cvalue = NULL;
	int index;
	int c;
	opterr = 0;
	while ((c = getopt (argc, argv, "hi:o:r:")) != -1)
	{
		switch (c)
		{
				case 'i':
						osm_in_file=optarg;
						break;
				case 'o':
						osm_out_file=optarg;
						break;
				case 'r':
						rules_file=optarg;
						break;
				case 'h':
						// help
						fprintf (stderr, "This is parsing OSM programm. \n\
	This programm change input file by rules file and save result to output file. \n\
	Use: \
		%s -r rules.xml -i input.osm -o out.osm \n\
		 \n\
		options: \n\
			-r file - file with xml-rules \n\
			-i file - input file with osm \n\
			-o file - output file with osm, where programm save result \n\
			-h - this help\n", argv[0]);
						break;
				case '?':
						if (isprint (optopt))
								fprintf (stderr, "Unknown option: '-%c'.\n", optopt);
						else
								fprintf (stderr, "Unknown option character: '\\x%x'.\n", optopt);
						return 1;
				default:
						return 1;
		}
	}
	return 0;
}
