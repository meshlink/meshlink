/*
    conf.c -- configuration code
    Copyright (C) 2014 Guus Sliepen <guus@meshlink.io>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "system.h"
#include <assert.h>

#include "splay_tree.h"
#include "connection.h"
#include "conf.h"
#include "list.h"
#include "logger.h"
#include "meshlink_internal.h"
#include "netutl.h"             /* for str2address */
#include "protocol.h"
#include "utils.h"              /* for cp */
#include "xalloc.h"

static int config_compare(const config_t *a, const config_t *b) {
	int result;

	result = strcasecmp(a->variable, b->variable);

	if(result)
		return result;

	result = a->line - b->line;

	if(result)
		return result;
	else
		return a->file ? strcmp(a->file, b->file) : 0;
}

void init_configuration(splay_tree_t **config_tree) {
	*config_tree = splay_alloc_tree((splay_compare_t) config_compare, (splay_action_t) free_config);
}

void exit_configuration(splay_tree_t **config_tree) {
	if(*config_tree)
		splay_delete_tree(*config_tree);
	*config_tree = NULL;
}

config_t *new_config(void) {
	return xzalloc(sizeof(config_t));
}

void free_config(config_t *cfg) {
	if(cfg->variable)
		free(cfg->variable);

	if(cfg->value)
		free(cfg->value);

	if(cfg->file)
		free(cfg->file);

	free(cfg);
}

void config_add(splay_tree_t *config_tree, config_t *cfg) {
	splay_insert(config_tree, cfg);
}

config_t *lookup_config(splay_tree_t *config_tree, char *variable) {
	config_t cfg, *found;

	cfg.variable = variable;
	cfg.file = NULL;
	cfg.line = 0;

	found = splay_search_closest_greater(config_tree, &cfg);

	if(!found)
		return NULL;

	if(strcasecmp(found->variable, variable))
		return NULL;

	return found;
}

config_t *lookup_config_next(splay_tree_t *config_tree, const config_t *cfg) {
	splay_node_t *node;
	config_t *found;

	node = splay_search_node(config_tree, cfg);

	if(node) {
		if(node->next) {
			found = node->next->data;

			if(!strcasecmp(found->variable, cfg->variable))
				return found;
		}
	}

	return NULL;
}

uint32_t collect_config(list_t *entry_list, splay_tree_t *config_tree, const char *key) {
    if(!entry_list) {
        meshlink_errno = MESHLINK_EINVAL;
        return 0;
    }

    uint32_t count = 0;
    struct config_t *cfg = lookup_config(config_tree, key);
    if( cfg ) {
        do {
    		++count;
            list_insert_tail( entry_list, cfg );
            cfg = lookup_config_next( config_tree, cfg );
        } while( cfg );
    }
    return count;
}

bool get_config_bool(const config_t *cfg, bool *result) {
	if(!cfg)
		return false;

	if(!strcasecmp(cfg->value, "yes")) {
		*result = true;
		return true;
	} else if(!strcasecmp(cfg->value, "no")) {
		*result = false;
		return true;
	}

	logger(NULL, MESHLINK_ERROR, "\"yes\" or \"no\" expected for configuration variable %s in %s line %d",
		   cfg->variable, cfg->file, cfg->line);

	return false;
}

bool get_config_int(const config_t *cfg, int *result) {
	if(!cfg)
		return false;

	if(sscanf(cfg->value, "%d", result) == 1)
		return true;

	logger(NULL, MESHLINK_ERROR, "Integer expected for configuration variable %s in %s line %d",
		   cfg->variable, cfg->file, cfg->line);

	return false;
}

bool set_config_int(config_t *cfg, int val)
{
	if(!cfg)
		return false;

	char val_str[1024];
	snprintf(val_str, sizeof(val_str), "%d", val);

	if(cfg->value)
		free(cfg->value);

	cfg->value = xstrdup(val_str);

	return true;
}

bool get_config_string(const config_t *cfg, char **result) {
	if(!cfg)
		return false;

	*result = xstrdup(cfg->value);

	return true;
}

bool set_config_string(config_t *cfg, const char* val)
{
	if(!cfg)
		return false;

	if(cfg->value)
		free(cfg->value);

	cfg->value = xstrdup(val);

	return true;
}

bool get_config_address(const config_t *cfg, struct addrinfo **result) {
	struct addrinfo *ai;

	if(!cfg)
		return false;

	ai = str2addrinfo(cfg->value, NULL, 0);

	if(ai) {
		*result = ai;
		return true;
	}

	logger(NULL, MESHLINK_ERROR, "Hostname or IP address expected for configuration variable %s in %s line %d",
		   cfg->variable, cfg->file, cfg->line);

	return false;
}

/*
  Read exactly one line and strip the trailing newline if any.
*/
static char *readline(FILE * fp, char *buf, size_t buflen) {
	char *newline = NULL;
	char *p;

	if(feof(fp))
		return NULL;

	p = fgets(buf, buflen, fp);

	if(!p)
		return NULL;

	newline = strchr(p, '\n');

	if(!newline)
		return buf;

	/* kill newline and carriage return if necessary */
	*newline = '\0';
	if(newline > p && newline[-1] == '\r')
		newline[-1] = '\0';

	return buf;
}

config_t *parse_config_line(char *line, const char *fname, int lineno) {
	config_t *cfg;
	int len;
	char *variable, *value, *eol;
	variable = value = line;

	eol = line + strlen(line);
	while(strchr("\t ", *--eol))
		*eol = '\0';

	len = strcspn(value, "\t =");
	value += len;
	value += strspn(value, "\t ");
	if(*value == '=') {
		value++;
		value += strspn(value, "\t ");
	}
	variable[len] = '\0';

	if(!*value) {
		const char err[] = "No value for variable";
		logger(NULL, MESHLINK_ERROR, "%s `%s' on line %d while reading config file %s",
			err, variable, lineno, fname);
		return NULL;
	}

	cfg = new_config();
	cfg->variable = xstrdup(variable);
	cfg->value = xstrdup(value);
	cfg->file = xstrdup(fname);
	cfg->line = lineno;

	return cfg;
}

/*
  Parse a configuration file and put the results in the configuration tree
  starting at *base.
*/
bool read_config_file(splay_tree_t *config_tree, const char *fname) {
	FILE *fp;
	char buffer[MAX_STRING_SIZE];
	char *line;
	int lineno = 0;
	bool ignore = false;
	config_t *cfg;
	bool result = false;

	fp = fopen(fname, "rb");

	if(!fp) {
		logger(NULL, MESHLINK_ERROR, "Cannot open config file %s: %s", fname, strerror(errno));
		return false;
	}

	for(;;) {
		line = readline(fp, buffer, sizeof buffer);

		if(!line) {
			if(feof(fp))
				result = true;
			break;
		}

		lineno++;

		if(!*line || *line == '#')
			continue;

		if(ignore) {
			if(!strncmp(line, "-----END", 8))
				ignore = false;
			continue;
		}

		if(!strncmp(line, "-----BEGIN", 10)) {
			ignore = true;
			continue;
		}

		cfg = parse_config_line(line, fname, lineno);
		if (!cfg)
			break;
		config_add(config_tree, cfg);
	}

	fclose(fp);

	return result;
}

bool write_config_file(const struct splay_tree_t *config_tree, const char *fname)
{
	FILE *fp;

	fp = fopen(fname, "wb+");

	if(!fp) {
		logger(NULL, MESHLINK_ERROR, "Cannot open config file %s: %s", fname, strerror(errno));
		return false;
	}

	for splay_each(config_t, cnf, config_tree)
	{
		if(fwrite(cnf->variable, sizeof(char), strlen(cnf->variable), fp) < strlen(cnf->variable))
			goto error;

		if(fwrite(" = ", sizeof(char), 3, fp) < 3)
			goto error;

		if(fwrite(cnf->value, sizeof(char), strlen(cnf->value), fp) < strlen(cnf->value))
			goto error;

		if(fwrite("\n", sizeof(char), 1, fp) < 1)
			goto error;
	}

	fclose(fp);
	return true;

error:
	logger(NULL, MESHLINK_ERROR, "Cannot write to config file %s: %s", fname, strerror(errno));
	fclose(fp);
	return false;
}

bool read_server_config(meshlink_handle_t *mesh) {
	char filename[PATH_MAX];
	bool x;

	snprintf(filename, PATH_MAX,"%s" SLASH "meshlink.conf", mesh->confbase);
	errno = 0;
	x = read_config_file(mesh->config, filename);

	if(!x && errno)
		logger(mesh, MESHLINK_ERROR, "Failed to read `%s': %s", filename, strerror(errno));

	return x;
}

bool read_host_config(meshlink_handle_t *mesh, splay_tree_t *config_tree, const char *name) {
	char filename[PATH_MAX];
	bool x;

	snprintf(filename,PATH_MAX, "%s" SLASH "hosts" SLASH "%s", mesh->confbase, name);
	x = read_config_file(config_tree, filename);

	return x;
}

bool write_host_config(struct meshlink_handle *mesh, const struct splay_tree_t *config_tree, const char *name)
{
	char filename[PATH_MAX];

	snprintf(filename,PATH_MAX, "%s" SLASH "hosts" SLASH "%s", mesh->confbase, name);
	return write_config_file(config_tree, filename);
}

bool modify_config_file(struct meshlink_handle *mesh, const char *name, const char *key, const char *value, bool replace) {
	assert(mesh && name && key && (replace || value));

	char filename[PATH_MAX];
	char tmpname[PATH_MAX];
	bool error = false;

	snprintf(filename, sizeof filename, "%s" SLASH "hosts" SLASH "%s", mesh->confbase, name);
	snprintf(tmpname, sizeof tmpname, "%s.tmp", filename);

	FILE *fr = fopen(filename, "r");

	if(!fr) {
		logger(mesh, MESHLINK_ERROR, "Cannot open config file %s: %s", filename, strerror(errno));
		return false;
	}

	FILE *fw = fopen(tmpname, "w");

	if(!fw) {
		logger(mesh, MESHLINK_ERROR, "Cannot open temporary file %s: %s", tmpname, strerror(errno));
		fclose(fr);
		return false;
	}

	char buf[4096];
	char *sep;
	bool found = false;

	while(readline(fr, buf, sizeof buf)) {
		if(!*buf || *buf == '#')
			goto copy;

		sep = strchr(buf, ' ');
		if(!sep)
			goto copy;

		*sep = 0;
		if(strcmp(buf, key)) {
			*sep = ' ';
			goto copy;
		}

		if(!value) {
			found = true;
			continue;
		}

		// We found the key and the value. Keep one copy around.
		if(sep[1] == '=' && sep[2] == ' ' && !strcmp(sep + 3, value)) {
			if(found)
				continue;
			found = true;
		}

		// We found the key but with a different value, delete it if wanted.
		if(!found && replace)
			continue;

		*sep = ' ';

copy:
		fprintf(fw, "%s\n", buf);
	}

	if(ferror(fr))
		error = true;

	fclose(fr);

	// Add new key/value pair if necessary
	if(!found && value)
		fprintf(fw, "%s = %s\n", key, value);

	if(ferror(fw))
		error = true;

	if(fclose(fw))
		error = true;

	// If any error occured during reading or writing, exit.
	if(error) {
		unlink(tmpname);
		return false;
	}

	// Try to atomically replace the old config file with the new one.
#ifdef HAVE_MINGW
	char bakname[PATH_MAX];
	snprintf(bakname, sizeof bakname, "%s.bak", filename);
	if(rename(filename, bakname) || rename(tmpname, filename)) {
		rename(bakname, filename);
#else
	if(rename(tmpname, filename)) {
#endif
		return false;
	} else {
#ifdef HAVE_MINGW
		unlink(bakname);
#endif
		return true;
	}
}

bool append_config_file(meshlink_handle_t *mesh, const char *name, const char *key, const char *value) {
	return modify_config_file(mesh, name, key, value, false);
}
