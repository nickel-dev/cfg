#include <stdio.h>
#include <stdint.h>
#define CFG_IMPLEMENTATION
#include "cfg.h"

char *type_string(uint32_t type);
void print_value(cfg_value_t value);
void print_list(cfg_list_t *list);

int32_t main(int32_t argc, char *argv[]) {
	(void)argc;
	(void)argv;

	cfg_data_t data = cfg_data_read_file("example.cfg");

	cfg_section_t *final_section = cfg_section_get(&data, "Final Section");
	cfg_variable_t *tags = cfg_variable_get(final_section, "tags");
	print_value(tags->value);

	// Print the source
	char *source = cfg_data_write(data);
	printf("\n+----------+\n"
	         "|  SOURCE  |\n"
	         "+----------+\n"
	         "\n%s\n", source);
	free(source);

	cfg_data_free(&data);
	return 0;
}

char *type_string(uint32_t type) {
	switch (type) {
		case CFG_INT:
			return "INT";
		case CFG_FLOAT:
			return "FLOAT";
		case CFG_STRING:
			return "STRING";
		case CFG_BOOL:
			return "BOOL";
		case CFG_LIST:
			return "LIST";
		default:
			return "?";
	}
}

void print_value(cfg_value_t value) {
	switch (value.type) {
		case CFG_INT:
			printf("%s\t%d\n", type_string(value.type), value.value_int);
			break;
		case CFG_FLOAT:
			printf("%s\t%f\n", type_string(value.type), value.value_float);
			break;
		case CFG_STRING:
			printf("%s\t%s\n", type_string(value.type), value.value_string);
			break;
		case CFG_BOOL:
			printf("%s\t%s\n", type_string(value.type), (value.value_bool ? "true" : "false"));
			break;
		case CFG_LIST:
			print_list(value.value_list);
			break;
		default:
			break;
	}
}

void print_list(cfg_list_t *list) {
	for (uint32_t i = 0; i < list->count; ++i)
		print_value(list->values[i]);
}
