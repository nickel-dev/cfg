/*
 This is an example of a cfg config being used as a meta program to generate data structures for C.
 */

#include <stdio.h>
#include <stdint.h>
#define CFG_IMPLEMENTATION
#include "../cfg.h"

enum data_types {
	DATA_STRUCT,
	DATA_UNION,
	DATA_ENUM
};

char *data_type_string(uint32_t data_type) {
	switch (data_type) {
	case DATA_STRUCT:
		return "struct";
	case DATA_UNION:
		return "union";
	case DATA_ENUM:
	default:
		return "enum";
	}
}

int32_t main(int32_t argc, char *argv[]) {
	(void)argc;
	(void)argv;

	const uint64_t struct_hash = cfg_hash("struct");
	const uint64_t enum_hash = cfg_hash("enum");
	const uint64_t union_hash = cfg_hash("union");

	cfg_data_t data = cfg_data_read_file("structs.cfg");

	for (uint32_t s = 0; s < data.count; ++s) {
		cfg_section_t section = data.sections[s];
		uint32_t data_type = DATA_STRUCT;

		// Get the data structure type
		{
			cfg_variable_t type = section.variables[0];
			if (type.value.type != CFG_STRING) // The section has no data type defined
				continue;

			uint64_t hash = cfg_hash(type.value.value_string);
			if (hash == struct_hash)
				data_type = DATA_STRUCT;
			else if (hash == enum_hash)
				data_type = DATA_ENUM;
			else if (hash == union_hash)
				data_type = DATA_UNION;
			else
				continue;

			printf("typedef %s %s %s_t;\n%s %s {\n", data_type_string(data_type), section.name, section.name,
			                                         data_type_string(data_type), section.name);
		}

		// Starts at 1, because the 0th variable is the data structure type
		for (uint32_t v = 1; v < section.count; ++v) {
			cfg_variable_t variable = section.variables[v];
			cfg_value_t value = variable.value;

			if (data_type == DATA_STRUCT || data_type == DATA_UNION) {
				if (value.type != CFG_STRING)
					continue;
				printf("\t%s %s;\n", value.value_string, variable.name);
			} else {
				if (value.type != CFG_INT)
					continue;
				printf("\t%s = %d,\n", variable.name, value.value_int);
			}
		}

		printf("};\n\n");
	}

	cfg_data_free(&data);
	return 0;
}
