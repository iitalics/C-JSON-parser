#include <stdio.h>
#include <stdlib.h>
#include "json.h"




int main (int argc, char** argv)
{
	if (argc == 0) return EXIT_FAILURE;
	
	if (argc <= 1)
	{
		printf("Usage:  %s <json file>\n\n"
		       "  Try using file 'people.json'\n",
			argv[0]);
		return EXIT_SUCCESS;
	}
	
	FILE* fs = fopen(argv[1], "r");
	
	struct json_value* root = json_parse_file(fs);
	struct json_value* entry;
	
	fclose(fs);
	
	
	
	if (!json_is_list(root))
	{
		fprintf(stderr, "JSON data must be list!\n");
		return EXIT_FAILURE;
	}
	
	int i, len;
	
	for (i = 0, len = json_length(root, NULL); i < len; i++)
	{
		entry = json_get_index(root, i);
		
		printf("Entry #%d\n"
		       "   Name:  %s\n"
		       "   Age:   %d\n"
		       "\n",
			   
			i + 1,
			json_get_string(json_get(entry, "name")),
			json_get_int(json_get(entry, "age"), NULL));
	}
	
	json_free(root);
	
	return EXIT_SUCCESS;
}