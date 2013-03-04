#pragma once


#include <stdbool.h>
#include <stdio.h>


enum json_type
{
	JSON_TYPE_INVALID	= 0,
	JSON_TYPE_STRING	= 1,
	JSON_TYPE_NUMBER	= 2,
	JSON_TYPE_LIST		= 4,
	JSON_TYPE_TABLE		= 8,
	JSON_TYPE_BOOLEAN	= 16,
	JSON_TYPE_NULL		= 32
};

typedef double json_number;

//#define JSON_MONITOR_MEMORY


extern struct json_value* json_parse (const char* str);
extern struct json_value* json_parse_file (FILE* fs);
extern void json_free (struct json_value* v);			// use on root value, will free all child values associated with it

extern void json_memory_status();	// does nothing if JSON_MONITOR_MEMORY is not defined


extern bool json_is_null (struct json_value* v);
extern bool json_is_numeric (struct json_value* v);
extern bool json_is_string (struct json_value* v);
extern bool json_is_list (struct json_value* v);
extern bool json_is_table (struct json_value* v);
extern bool json_is_valid (struct json_value* v);
extern enum json_type json_get_type (struct json_value* v);



// 'bool* success' tells if the operation was successful, can always be NULL
//   if you don't care

extern struct json_value* json_get_index (struct json_value* v, int index);
extern struct json_value* json_get (struct json_value* v, const char* keyname);
extern int json_length (struct json_value* v, bool* success);

extern json_number json_get_number (struct json_value* v, bool* success);
extern int json_get_int (struct json_value* v, bool* success);
extern const char* json_get_string (struct json_value* v);		// returns NULL if unsuccessful




