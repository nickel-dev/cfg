#ifndef INCLUDE_CFG_H
#define INCLUDE_CFG_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef enum cfg_type cfg_type_t;
typedef struct cfg_value cfg_value_t;
typedef struct cfg_list cfg_list_t;
typedef struct cfg_variable cfg_variable_t;
typedef struct cfg_section cfg_section_t;
typedef struct cfg_data cfg_data_t;

enum cfg_type {
	CFG_INT,
	CFG_FLOAT,
	CFG_STRING,
	CFG_BOOL,
	CFG_LIST
};

struct cfg_value {
	cfg_type_t type;
	union {
		void *value;
		int32_t value_int;
		double value_float;
		char *value_string;
		uint8_t value_bool;
		cfg_list_t *value_list;
	};
};

struct cfg_list {
	uint32_t count;
	cfg_value_t *values;
};

struct cfg_variable {
	char *name;
	cfg_value_t value;
};

struct cfg_section {
	char *name;
	uint32_t count;
	cfg_variable_t *variables;
};

struct cfg_data {
	uint32_t count;
	cfg_section_t *sections;
};

// Sections
cfg_section_t *cfg_section_add(cfg_data_t *data, char *name, uint32_t index);
cfg_section_t *cfg_section_get(cfg_data_t *data, char *name);
int64_t cfg_section_index(cfg_data_t *data, char *name);
uint32_t cfg_section_pointer_index(cfg_data_t *data, cfg_section_t *section);
void cfg_section_remove(cfg_data_t *data, uint32_t index);

// Lists
cfg_list_t *cfg_list_create();
void cfg_list_delete(cfg_list_t *list);
cfg_value_t *cfg_list_add(cfg_list_t *list, cfg_value_t value, uint32_t index);
void cfg_list_remove(cfg_list_t *list, uint32_t index);

// Variables
cfg_variable_t *cfg_variable_add(cfg_section_t *section, char *name, cfg_value_t value, uint32_t index);
cfg_variable_t *cfg_variable_get(cfg_section_t *section, char *name);
int64_t cfg_variable_index(cfg_section_t *section, char *name);
uint32_t cfg_variable_pointer_index(cfg_section_t *section, cfg_variable_t *variable);
void cfg_variable_remove(cfg_section_t *section, uint32_t index);

// Data
char *cfg_data_write(cfg_data_t data);
cfg_data_t cfg_data_read(char *source);
uint8_t cfg_data_write_file(char *path, cfg_data_t data);
cfg_data_t cfg_data_read_file(char *path);
void cfg_data_free(cfg_data_t *data);

#ifdef CFG_IMPLEMENTATION

// Internal data structures
typedef enum _cfg_token_type _cfg_token_type_t;
typedef struct _cfg_token _cfg_token_t;

enum _cfg_token_type {
	_CFG_TOKEN_ROOT,
	_CFG_TOKEN_IDENTIFIER, // Everything that is not recognized is an identifier
	_CFG_TOKEN_WHITESPACE,
	_CFG_TOKEN_STRING,
	_CFG_TOKEN_QUOTATION,
	_CFG_TOKEN_SECTION,
	_CFG_TOKEN_SECTION_BEGIN,
	_CFG_TOKEN_SECTION_END,
	_CFG_TOKEN_COMMENT,
	_CFG_TOKEN_INT,
	_CFG_TOKEN_FLOAT,
	_CFG_TOKEN_LIST_BEGIN,
	_CFG_TOKEN_LIST_END,
	_CFG_TOKEN_ASSIGN
};

struct _cfg_token {
	char *string;
	uint32_t type;
	_cfg_token_t *next;
};

// Internal functions
static uint64_t _cfg_hash(char *string) {
	uint64_t hash = 14695981039346656037ULL; // FNV offset basis
	int32_t c;
	while ((c = *string++)) {
		hash ^= (uint64_t)c;
		hash *= 1099511628211ULL; // FNV prime
	}
	return hash;
}

static inline char *_cfg_bool_string(int32_t n) {
	return (n ? "true" : "false");
}

static char *_cfg_copy_string(char *string) {
	uint64_t l = strlen(string);
	char *buffer = malloc(l + 1);
	memcpy(buffer, string, l);
	buffer[l] = 0;
	return buffer;
}

static char *_cfg_string_append(char *string, char *format, ...) {
    va_list args;
    
    // Get the size of existing string (handle NULL case)
    uint32_t size = string ? strlen(string) : 0;
    
    // First vsnprintf to get required size
    va_start(args, format);
    int32_t args_size = vsnprintf(NULL, 0, format, args);
    va_end(args);
    
    if (args_size < 0) {
        // Format error
        return string;
    }
    
    // Reallocate (handle initial NULL string)
    char *new_string = realloc(string, size + args_size + 1);
    if (!new_string) {
        // realloc failed
        free(string);
        return NULL;
    }
    
    // Format the new content
    va_start(args, format);
    vsnprintf(new_string + size, args_size + 1, format, args);
    va_end(args);
    
    return new_string;
}

static char *_cfg_string_append_char(char *string, char c) {
    if (string == NULL) {
        char *new_string = malloc(2);
        if (new_string == NULL) return NULL;
        new_string[0] = c;
        new_string[1] = '\0';
        return new_string;
    }
    
    size_t len = strlen(string);
    char *temp = realloc(string, len + 2);
    if (temp == NULL) {
        return string;
    }
    
    temp[len] = c;
    temp[len + 1] = '\0';
    return temp;
}

static char *_cfg_value_write(char *buffer, cfg_value_t value) {
	switch (value.type) {
	case CFG_INT:
		buffer = _cfg_string_append(buffer, "%lu", value.value_int);
		break;
	case CFG_FLOAT:
		buffer = _cfg_string_append(buffer, "%f", value.value_float);
		break;
	case CFG_STRING:
		buffer = _cfg_string_append(buffer, "\"%s\"", value.value_string);
		break;
	case CFG_BOOL:
		buffer = _cfg_string_append(buffer, "%s", _cfg_bool_string(value.value_bool));
		break;
	case CFG_LIST:
		buffer = _cfg_string_append(buffer, "(");
		for (uint32_t i = 0; i < value.value_list->count; ++i) {
			buffer = _cfg_value_write(buffer, value.value_list->values[i]);
			if (i < value.value_list->count - 1)
				buffer = _cfg_string_append(buffer, " ");
		}
		buffer = _cfg_string_append(buffer, ")");
		break;
	default:
		break;
	}
	return buffer;
}

static uint32_t _cfg_token_type(char c) {
	static uint32_t ignore = 0;
	if (ignore) {
		if (ignore == _CFG_TOKEN_STRING && c == '"') {
			ignore = 0;
			return _CFG_TOKEN_QUOTATION;
		} if (ignore == _CFG_TOKEN_SECTION && c == ']') {
			ignore = 0;
			return _CFG_TOKEN_SECTION_END;
		} else if (ignore == _CFG_TOKEN_COMMENT && c == '\n')
			ignore = 0;
		return ignore;
	}

	if (c <= ' ')
		return _CFG_TOKEN_WHITESPACE;

	if ((c >= '0' && c <= '9') || c == '-')
		return _CFG_TOKEN_INT;

	switch (c) {
	case '"':
		ignore = _CFG_TOKEN_STRING;
		return _CFG_TOKEN_QUOTATION;
	case '[':
		ignore = _CFG_TOKEN_SECTION;
		return _CFG_TOKEN_SECTION_BEGIN;
	case '#':
		ignore = _CFG_TOKEN_COMMENT;
		return _CFG_TOKEN_COMMENT;
	case '.':
		return _CFG_TOKEN_FLOAT;
	case '(':
		return _CFG_TOKEN_LIST_BEGIN;
	case ')':
		return _CFG_TOKEN_LIST_END;
	case '=':
		return _CFG_TOKEN_ASSIGN;
	default:
		return _CFG_TOKEN_IDENTIFIER;
	}
}

static _cfg_token_t *_cfg_tokenize(char *source) {
	_cfg_token_t *full = calloc(1, sizeof(_cfg_token_t)), *root = full;
	full->type = _CFG_TOKEN_ROOT;
	uint8_t split = 0;

	for (uint64_t i = 0; i < strlen(source); ++i) {
		char c = source[i];
		uint32_t type = _cfg_token_type(c);

		if (type == _CFG_TOKEN_WHITESPACE ||
		    type == _CFG_TOKEN_QUOTATION ||
		    type == _CFG_TOKEN_COMMENT) {
		    split = 1;
			continue;
		}

		if (!split &&
		   (type == full->type && (type != _CFG_TOKEN_LIST_BEGIN &&
		                           type != _CFG_TOKEN_LIST_END &&
		                           type != _CFG_TOKEN_ASSIGN))) {
			full->string = _cfg_string_append_char(full->string, c);
		} else if (!split && ((full->type == _CFG_TOKEN_INT && type == _CFG_TOKEN_FLOAT) ||
		                      (full->type == _CFG_TOKEN_FLOAT && type == _CFG_TOKEN_INT))) {
			full->string = _cfg_string_append_char(full->string, c);
			full->type = _CFG_TOKEN_FLOAT;
		} else {
			full->next = calloc(1, sizeof(_cfg_token_t));
			full = full->next;
			full->type = type;
			full->string = _cfg_string_append_char(full->string, c);
		}

		split = 0;
	}

	return root;
}

static cfg_value_t _cfg_token_value(_cfg_token_t **token) {
	cfg_value_t value = { 0 };
	const uint64_t true_hash = _cfg_hash("true");
	const uint64_t false_hash = _cfg_hash("false");
	if (*token) {
		switch ((*token)->type) {
		case _CFG_TOKEN_STRING:
			value = (cfg_value_t) { CFG_STRING, { .value_string = (*token)->string } };
			break;
		case _CFG_TOKEN_INT:
			value = (cfg_value_t) { CFG_INT, { .value_int = atoi((*token)->string) } };
			break;
		case _CFG_TOKEN_FLOAT:
			value = (cfg_value_t) { CFG_FLOAT, { .value_float = atof((*token)->string) } };
			break;
		case _CFG_TOKEN_IDENTIFIER:
			uint64_t hash = _cfg_hash((*token)->string);
			if (hash == true_hash)
				value = (cfg_value_t) { CFG_BOOL, { .value_bool = 1 } };
			else if (hash == false_hash)
				value = (cfg_value_t) { CFG_BOOL, { .value_bool = 0 } };
			break;
		case _CFG_TOKEN_LIST_BEGIN:
			value = (cfg_value_t) { CFG_LIST, { .value_list = cfg_list_create() } };
			while ((*token)->next != NULL) {
				*token = (*token)->next;
				if ((*token)->type == _CFG_TOKEN_LIST_END)
					break;
				cfg_list_add(value.value_list, _cfg_token_value(token), value.value_list->count);
			}
			break;
		default:
			break;
		}
	}
	return value;
}

// Sections
cfg_section_t *cfg_section_add(cfg_data_t *data, char *name, uint32_t index) {
	if (index > data->count)
		index = data->count;
	++data->count;

	data->sections = realloc(data->sections, sizeof(cfg_section_t) * data->count);
	if (index < data->count - 1)
		memcpy(&data->sections[index + 1], &data->sections[index], sizeof(cfg_section_t) * (data->count - index - 1));

	cfg_section_t *curr = &data->sections[index];

	if (name)
		curr->name = _cfg_copy_string(name);
	else
		curr->name = _cfg_string_append(NULL, "section%u", data->count);
	curr->count = 0;
	curr->variables = NULL;

	return curr;
}

cfg_section_t *cfg_section_get(cfg_data_t *data, char *name) {
	uint64_t hash = _cfg_hash(name);
	for (uint32_t i = 0; i < data->count; ++i) {
		uint64_t h = _cfg_hash(data->sections[i].name);
		if (hash == h)
			return &data->sections[i];
	}
	return NULL;
}

int64_t cfg_section_index(cfg_data_t *data, char *name) {
	uint64_t hash = _cfg_hash(name);
	for (uint32_t i = 0; i < data->count; ++i) {
		uint64_t h = _cfg_hash(data->sections[i].name);
		if (hash == h)
			return i;
	}
	return -1;
}

uint32_t cfg_section_pointer_index(cfg_data_t *data, cfg_section_t *section) {
	return data->sections - section;
}

void cfg_section_remove(cfg_data_t *data, uint32_t index) {
	if (index > data->count - 1)
		return;
	--data->count;

	cfg_section_t *curr = &data->sections[index];

	if (curr->name)
		free(curr->name);
	if (curr->variables) {
		for (uint32_t i = 0; i < curr->count; ++i)
			cfg_variable_remove(curr, i);
		free(curr->variables);
	}

	if (index < data->count)
		memcpy(curr, &data->sections[index + 1], sizeof(cfg_section_t) * (data->count - index));

	data->sections = realloc(data->sections, sizeof(cfg_section_t) * data->count);
}

// Lists
cfg_list_t *cfg_list_create() {
	cfg_list_t *list = malloc(sizeof(cfg_list_t));
	list->count = 0;
	list->values = NULL;
	return list;
}

void cfg_list_delete(cfg_list_t *list) {
	if (list->values) {
		for (uint32_t i = 0; i < list->count; ++i) {
			if (list->values[i].type == CFG_STRING && list->values[i].value_string)
				free(list->values[i].value_string);
			else if (list->values[i].type == CFG_LIST && list->values[i].value_list)
				cfg_list_delete(list->values[i].value_list);
		}
		free(list->values);
	}
	free(list);
}

cfg_value_t *cfg_list_add(cfg_list_t *list, cfg_value_t value, uint32_t index) {
	if (index > list->count)
		index = list->count;
	++list->count;
	
	list->values = realloc(list->values, sizeof(cfg_value_t) * list->count);
	if (index < list->count - 1)
		memcpy(&list->values[index + 1], &list->values[index], sizeof(cfg_value_t) * (list->count - index - 1));

	cfg_value_t *curr = &list->values[index];
	memcpy(curr, &value, sizeof(cfg_value_t));
	if (value.type == CFG_STRING) {
		if (value.value_string)
			curr->value_string = _cfg_copy_string(value.value_string);
		else
			curr->value_string = "";
	}

	return curr;
}

void cfg_list_remove(cfg_list_t *list, uint32_t index) {
	if (index > list->count - 1)
		return;
	--list->count;

	cfg_value_t *curr = &list->values[index];

	if (curr->type == CFG_STRING && curr->value_string)
		free(curr->value_string);
	else if (curr->type == CFG_LIST && curr->value_list)
		cfg_list_delete(curr->value_list);

	if (index < list->count)
		memcpy(curr, &list->values[index + 1], sizeof(cfg_value_t) * (list->count - index));

	list->values = realloc(list->values, sizeof(cfg_value_t) * list->count);
}

// Variables
cfg_variable_t *cfg_variable_add(cfg_section_t *section, char *name, cfg_value_t value, uint32_t index) {
	if (index > section->count)
		index = section->count;
	++section->count;
	
	section->variables = realloc(section->variables, sizeof(cfg_variable_t) * section->count);
	if (index < section->count - 1)
		memcpy(&section->variables[index + 1], &section->variables[index], sizeof(cfg_variable_t) * (section->count - index - 1));

	cfg_variable_t *curr = &section->variables[index];

	if (name)
		curr->name = _cfg_copy_string(name);
	else
		curr->name = _cfg_string_append(NULL, "var%u", section->count);

	curr->value = value;
	if (value.type == CFG_STRING) {
		if (value.value_string)
			curr->value.value_string = _cfg_copy_string(value.value_string);
		else
			curr->value.value_string = "";
	}

	return curr;
}

cfg_variable_t *cfg_variable_get(cfg_section_t *section, char *name) {
	uint64_t hash = _cfg_hash(name);
	for (uint32_t i = 0; i < section->count; ++i) {
		uint64_t h = _cfg_hash(section->variables[i].name);
		if (hash == h)
			return &section->variables[i];
	}
	return NULL;
}

int64_t cfg_variable_index(cfg_section_t *section, char *name) {
	uint64_t hash = _cfg_hash(name);
	for (uint32_t i = 0; i < section->count; ++i) {
		uint64_t h = _cfg_hash(section->variables[i].name);
		if (hash == h)
			return i;
	}
	return -1;
}

uint32_t cfg_variable_pointer_index(cfg_section_t *section, cfg_variable_t *variable) {
	return section->variables - variable;
}

void cfg_variable_remove(cfg_section_t *section, uint32_t index) {
	if (index > section->count - 1)
		return;
	--section->count;

	cfg_variable_t *curr = &section->variables[index];

	if (curr->name)
		free(curr->name);

	if (curr->value.type == CFG_STRING && curr->value.value_string)
		free(curr->value.value_string);
	else if (curr->value.type == CFG_LIST && curr->value.value_list)
		cfg_list_delete(curr->value.value_list);

	if (index < section->count)
		memcpy(curr, &section->variables[index + 1], sizeof(cfg_variable_t) * (section->count - index));

	section->variables = realloc(section->variables, sizeof(cfg_variable_t) * section->count);
}

// Data
char *cfg_data_write(cfg_data_t data) {
	char *buffer = NULL;
	for (uint32_t s = 0; s < data.count; ++s) {
		cfg_section_t *section = &data.sections[s];
		buffer = _cfg_string_append(buffer, "[%s]\n", section->name);
		for (uint32_t c = 0; c < section->count; ++c) {
			cfg_variable_t *variable = &section->variables[c];
			buffer = _cfg_string_append(buffer, "%s = ", variable->name);
			buffer = _cfg_value_write(buffer, variable->value);
			buffer = _cfg_string_append(buffer, "\n");
		}
		buffer = _cfg_string_append(buffer, "\n");
	}
	return buffer;
}

cfg_data_t cfg_data_read(char *source) {
	cfg_data_t data = { 0 };
	_cfg_token_t *root_token = _cfg_tokenize(source);

	// Parse the tokens
	cfg_section_t *section = NULL;
	for (_cfg_token_t *prev_token = NULL, *token = root_token; token != NULL; prev_token = token, token = token->next) {
		switch (token->type) {
		case _CFG_TOKEN_SECTION_END:
			if (prev_token && prev_token->type == _CFG_TOKEN_SECTION_BEGIN)
				section = cfg_section_add(&data, NULL, data.count);
			break;
		case _CFG_TOKEN_SECTION:
			section = cfg_section_add(&data, token->string, data.count);
			break;
		case _CFG_TOKEN_ASSIGN: {
			if (!section)
				section = cfg_section_add(&data, NULL, data.count);
			char *name = NULL;
			if (prev_token && prev_token->type == _CFG_TOKEN_IDENTIFIER)
				name = prev_token->string;
			cfg_value_t value = _cfg_token_value(&token->next);
			cfg_variable_add(section, name, value, section->count);
			break;
		}
		default:
			break;
		}
	}

	// Free the tokens
	for (_cfg_token_t *token = root_token; token != NULL; token = token->next) {
		_cfg_token_t *t = token;
		if (t) {
			token = token->next;
			if (t->string)
				free(t->string);
			free(t);
		}
	}

	return data;
}

uint8_t cfg_data_write_file(char *path, cfg_data_t data) {
	char *string = cfg_data_write(data);
	FILE *f = fopen(path, "w");
	if (!f)
		return 0;
	fprintf(f, string);
	fclose(f);
	free(string);
	return 1;
}

cfg_data_t cfg_data_read_file(char *path) {
	cfg_data_t data = { 0 };

	FILE *f = fopen(path, "rb");
	if (!f)
		return data;

	fseek(f, 0, SEEK_END);
	uint64_t fsize = ftell(f);
	fseek(f, 0, SEEK_SET);

	char *source = malloc(fsize + 1);
	fread(source, fsize, 1, f);
	fclose(f);
	
	source[fsize] = 0;
	data = cfg_data_read(source);

	free(source);
	return data;
}

void cfg_data_free(cfg_data_t *data) {
	for (uint32_t i = 0; i < data->count; ++i)
		cfg_section_remove(data, i);
}

#endif // CFG_IMPLEMENTATION

#endif // INCLUDE_CFG_H
