#include "json.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>





#ifdef JSON_MONITOR_MEMORY
static int num_alloc = 0,
			num_free = 0;

void* json__malloc (size_t size)
{
	num_alloc++;
	return malloc(size);
}
void json__free (void* data)
{
	num_free++;
	free(data);
}
void json_memory_status ()
{
	printf("allocated:  %d\n"
	       "    freed:  %d\n"
	       "   in use:  %d\n",
		num_alloc, num_free, num_alloc - num_free);
}
#else
#define json__malloc malloc
#define json__free free
void json_memory_status () {}
#endif




struct json_value
{
	enum json_type type;
	struct json_value* next;
	struct json_allocator* alloc;
};

static void json_value_destroy_one (struct json_value* value);


struct json_str_node
{
	char* str;
	struct json_str_node* next;
};


struct json_allocator
{
	struct json_str_node* strings;
	struct json_value* values;
	
	struct json_value nullvalue;
	struct json_value invalidvalue;
};


struct json_list_node
{
	char* key;
	struct json_value* value;
	
	struct json_list_node* next;
};





static void json_error (const char* message)
{
	fprintf(stderr, "Error while parsing JSON: %s", message);
	exit(-1);
}



/*
char* json_str_store (char* str, struct json_allocator* a)
{
	struct json_str_node* n = json__malloc(sizeof(struct json_str_node));
	n->str = str;
	n->next = a->strings;
	a->strings = n;
	
	return str;
}
*/
static char* json_str_alloc (size_t length, struct json_allocator* a)
{
	struct json_str_node* node = json__malloc(sizeof(struct json_str_node) + length);
	char* str = ((char*)node) + sizeof(struct json_str_node);
	
	node->str = str;
	node->next = a->strings;
	a->strings = node;
	
	return str;
}




static struct json_allocator* json_allocator_create ()
{
	struct json_allocator* a = json__malloc(sizeof(struct json_allocator));
	a->strings = NULL;
	a->values = NULL;
	
	a->nullvalue.type = JSON_TYPE_NULL;
	a->invalidvalue.type = JSON_TYPE_INVALID;
	
	return a;
}

static void json_allocator_add_value (struct json_value* value, struct json_allocator* a)
{
	value->next = a->values;
	a->values = value;
}

static void json_allocator_destroy (struct json_allocator* a)
{
	if (a == NULL) return;
	
	struct json_str_node* snode;
	struct json_str_node* sn;
	for (snode = a->strings; snode != NULL; snode = sn)
	{
		sn = snode->next;
		json__free(snode);
	}
	
	struct json_value* v;
	struct json_value* vn;
	for (v = a->values; v != NULL; v = vn)
	{
		vn = v->next;
		json_value_destroy_one(v);
	}
		
	json__free(a);
}






struct json__value_string
{
	struct json_value base;
	char* str;
};
struct json__value_number
{
	struct json_value base;
	json_number number;
};
struct json__value_list
{
	struct json_value base;
	
	struct json_value** items;
	int len;
};
struct json__value_table
{
	struct json_value base;
	
	struct json_value** values;
	char** keys;
	int len;
};

static void json_value_destroy_one (struct json_value* value)
{
	if (value == NULL) return;
	
	if (value->type == JSON_TYPE_LIST)
	{
		struct json__value_list* list_value = (struct json__value_list*)value;
		
		json__free(list_value->items);
	}
	else if (value->type == JSON_TYPE_TABLE)
	{
		struct json__value_table* table_value = (struct json__value_table*)value;
		
		json__free(table_value->values);
		// keys is allocated with values
	}
	
	json__free(value);
}


static void json_list_node_list (struct json__value_list* list, struct json_list_node* nodes)
{
	int len;
	struct json_list_node* node;
	struct json_list_node* next;
	
	len = 0;
	for (node = nodes; node != NULL; node = node->next)
		len++;
	
	list->items = json__malloc(sizeof(struct json_value*) * len);
	list->len = len;
	
	for (node = nodes; node != NULL; node = next)
	{
		list->items[--len] = node->value;
		
		next = node->next;
		json__free(node);
	}
}
static void json_list_node_table (struct json__value_table* table, struct json_list_node* nodes)
{
	int len;
	struct json_list_node* node;
	struct json_list_node* next;
	
	len = 0;
	for (node = nodes; node != NULL; node = node->next)
		len++;
	
	table->values = json__malloc(sizeof(struct json_value*) * 2 * len);
	table->keys = ((char**)(table->values)) + len;
	table->len = len;
	
	for (node = nodes; node != NULL; node = next)
	{
		table->values[--len] = node->value;
		table->keys[len] = node->key;
		
		next = node->next;
		json__free(node);
	}
}




static const char* str_trimleft (const char* c)
{
	while (*c && isspace(*c))
		c++;
	return c;
}
static int hex_char (char c)
{
	if (isdigit(c))
		return c - '0';
	return (tolower(c) - 'a') + 0xa;
}
static bool is_number (char c)
{
	return isdigit(c) || (c == '.') || (c == '-');
}
// returns length of escape sequence
static int json_escape_character (const char* str, char* out)
{
	switch (str[0])
	{
		case 'u':
		{
			int /*a, b, */c, d;
			
			//a = hex_char(str[1]);
			//b = hex_char(str[2]);
			
			// unicode not supported
			
			c = hex_char(str[3]);
			d = hex_char(str[4]);
			
			*out = (c << 4) + d;
			
			return 4 + 1;
		}
		case 'n':	*out = '\n'; return 1;
		case 'r':	*out = '\r'; return 1;
		case 'b':	*out = '\b'; return 1;
		case 'f':	*out = '\f'; return 1;
		case 't':	*out = '\t'; return 1;
		case '/':	*out = '/'; return 1;
		case '"':	*out = '\"'; return 1;
		case '\'':	*out = '\''; return 1;
		case '\\':	*out = '\\'; return 1;
		
		default:
			json_error("Invalid escape sequence");
			return 0;
	}
}

static char* json_parse_string (const char** str_ref, struct json_allocator* alloc)
{
	const char* str = *str_ref;
	
	if (str[0] != '"')
		json_error("String must start with \"");
	str++;
	
	int i, len;
	char esc;
	
	for (i = 0; str[i] != '"'; i++, len++)
		if (str[i] == '\\')
			i += json_escape_character(str + i + 1, &esc);
	
	char* output = json_str_alloc(len + 1, alloc);
	len = 0;
	for (i = 0; str[i] != '"'; i++, len++)
	{
		if (str[i] == '\\')
			i += json_escape_character(str + i + 1, output + len);
		else
			output[len] = str[i];
	}
	
	output[len] = '\0';
	
	*str_ref = str + i + 1;
	
	return output;
}



static struct json_value* json_parse_value (const char** str_ref, struct json_allocator* alloc);
static struct json_value* json_parse_value (const char** str_ref, struct json_allocator* alloc)
{
	const char* str = *str_ref;
	
	if (str[0] == '"')
	{
		char* out = json_parse_string(&str, alloc);
		*str_ref = str;
		
		struct json__value_string* value = json__malloc(sizeof(struct json__value_string));
		value->base.type = JSON_TYPE_STRING;
		value->base.alloc = alloc;
		value->str = out;
		
		json_allocator_add_value((struct json_value*)value, alloc);
		
		return (struct json_value*)value;
	}
	else if (is_number(str[0]))
	{
		int i;
		json_number n = 0;
		bool negative = false;
		
		if (str[0] == '-')
		{
			negative = true;
			i = 1;
		}
		else
			i = 0;
		
		for (; isdigit(str[i]); i++)
			n = (str[i] - '0') + (n * 10);
		
		if (str[i] == '.')
		{
			json_number f = 1;
			
			for (i++; isdigit(str[i]); i++)
			{
				f *= 10;
				n += ((json_number)(str[i] - '0')) / f;
			}
		}
		
		if (str[i] == 'e' || str[i] == 'E')
		{
			int r, m;
			
			i++;
			if (str[i] == '+')
				m = 1;
			else if (str[i] == '-')
				m = -1;
			else
				json_error("Expected '+' or '-' after 'e' in number");
			i++;
			
			for (r = 0; isdigit(str[i]); i++)
				r = (str[i] - '0') + (r * 10);
			
			for (; r > 0; r--)
				m *= 10;
			
			if (m < 0)
				n /= -m;
			else
				n *= m;
		}
		
		
		if (negative)
			n = -n;		// blaze it faggot
		
		struct json__value_number* value = json__malloc(sizeof(struct json__value_number));
		value->base.type = JSON_TYPE_NUMBER;
		value->base.alloc = alloc;
		value->number = n;
		
		json_allocator_add_value((struct json_value*)value, alloc);
		
		*str_ref = str + i;
		
		return (struct json_value*)value;
	}
	else if (str[0] == '[')
	{
		struct json_list_node* list = NULL;
		struct json_list_node* node;
		
		for (;;)
		{
			str = str_trimleft(str + 1);
			node = json__malloc(sizeof(struct json_list_node));
			node->value = json_parse_value(&str, alloc);
			node->next = list;
			list = node;
			
			str = str_trimleft(str);
			
			if (str[0] == ']')
				break;
			else if (str[0] != ',')
				json_error("Expected ',' or ']' after item in list");
		}
		
		struct json__value_list* value = json__malloc(sizeof(struct json__value_list));
		value->base.type = JSON_TYPE_LIST;
		value->base.alloc = alloc;
		json_list_node_list(value, list);
		
		json_allocator_add_value((struct json_value*)value, alloc);
		
		*str_ref = str + 1;
		
		return (struct json_value*)value;
	}
	else if (str[0] == '{')
	{
		struct json_list_node* list = NULL;
		struct json_list_node* node;
		
		for (;;)
		{
			str = str_trimleft(str + 1);
			
			node = json__malloc(sizeof(struct json_list_node));
			node->key = json_parse_string(&str, alloc);
			
			str = str_trimleft(str);
			if (str[0] != ':')
				json_error("Expected ':' after key name");
			
			str = str_trimleft(str + 1);
			node->value = json_parse_value(&str, alloc);
			node->next = list;
			list = node;
			
			str = str_trimleft(str);
			
			if (str[0] == '}')
				break;
			else if (str[0] != ',')
				json_error("Expected ',' or '}' after value in table");
		}
		
		struct json__value_table* value = json__malloc(sizeof(struct json__value_table));
		value->base.type = JSON_TYPE_TABLE;
		value->base.alloc = alloc;
		json_list_node_table(value, list);
		
		json_allocator_add_value((struct json_value*)value, alloc);
		
		*str_ref = str + 1;
		
		return (struct json_value*)value;
	}
	else
	{
		json_error("Invalid character");
		return NULL;
	}
}



struct json_value* json_parse (const char* str)
{
	struct json_allocator* alloc = json_allocator_create();
	str = str_trimleft(str);
	return json_parse_value(&str, alloc);
}
struct json_value* json_parse_file (FILE* fs)
{
	fseek(fs, 0, SEEK_END);
	int length = ftell(fs);
	fseek(fs, 0, SEEK_SET);
	
	char* buffer = json__malloc(length + 1);
	int i = 0;
	const int read_size = 512;
	
	while (!feof(fs))
		i += fread(buffer + i, 1, read_size, fs);
	
	buffer[i] = '\0';
	struct json_value* result = json_parse(buffer);
	json__free(buffer);
	
	return result;
}


bool json_is_null (struct json_value* v) { return v->type == JSON_TYPE_NULL; }
bool json_is_numeric (struct json_value* v) { return v->type == JSON_TYPE_NUMBER; }
bool json_is_string (struct json_value* v) { return v->type == JSON_TYPE_STRING; }
bool json_is_list (struct json_value* v) { return v->type == JSON_TYPE_LIST; }
bool json_is_table (struct json_value* v) { return v->type == JSON_TYPE_TABLE; }
bool json_is_valid (struct json_value* v) { return v->type != JSON_TYPE_INVALID; }

enum json_type json_get_type (struct json_value* v)
{
	return v->type;
}


struct json_value* json_get_index (struct json_value* v, int index)
{
	if (v->type != JSON_TYPE_LIST)
		return &v->alloc->invalidvalue;
	
	struct json__value_list* list = (struct json__value_list*)v;
	
	if (index < 0 || index >= list->len)
		return &v->alloc->nullvalue;
	
	return list->items[index];
}
struct json_value* json_get (struct json_value* v, const char* keyname)
{
	if (v->type == JSON_TYPE_LIST)
		return json_get_index(v, atoi(keyname));
	else if (v->type != JSON_TYPE_TABLE)
		return &v->alloc->invalidvalue;	
	
	
	struct json__value_table* table = (struct json__value_table*)v;
	int i;
	for (i = 0; i < table->len; i++)
		if (strcmp(table->keys[i], keyname) == 0)
			return table->values[i];
	
	return &v->alloc->nullvalue;
}


int json_length (struct json_value* v, bool* success)
{
	int result = 0;
	
	if (v->type == JSON_TYPE_STRING)
	{
		result = strlen(((struct json__value_string*)v)->str);
		
		if (success) *success = true;
	}
	else if (v->type == JSON_TYPE_LIST)
	{
		result = ((struct json__value_list*)v)->len;
		
		if (success) *success = true;
	}
	else if (success)
		*success = false;
	
	return result;
}

json_number json_get_number (struct json_value* v, bool* success)
{
	json_number result = 0;
	
	if (v->type == JSON_TYPE_NUMBER)
	{
		result = ((struct json__value_number*)v)->number;
		if (success) *success = true;
	}
	else if (v->type == JSON_TYPE_STRING)
	{
		result = atof(((struct json__value_string*)v)->str);
		if (success) *success = true;
	}
	else if (success)
		*success = false;
	
	return result;
}
int json_get_int (struct json_value* v, bool* success)
{
	return (int)json_get_number(v, success);
}
const char* json_get_string (struct json_value* v)
{
	if (v->type == JSON_TYPE_STRING)
		return ((struct json__value_string*)v)->str;
	
	return NULL;
}


void json_free (struct json_value* v)
{
	json_allocator_destroy(v->alloc);
}





