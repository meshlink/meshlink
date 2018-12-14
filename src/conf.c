/*
    econf.c -- configuration code
    Copyright (C) 2018 Guus Sliepen <guus@meshlink.io>

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

#include "conf.h"
#include "crypto.h"
#include "logger.h"
#include "meshlink_internal.h"
#include "xalloc.h"
#include "packmsg.h"

/// Generate a path to the main configuration file.
static void make_main_path(meshlink_handle_t *mesh, char *path, size_t len) {
	snprintf(path, len, "%s" SLASH "meshlink.conf", mesh->confbase);
}

/// Generate a path to a host configuration file.
static void make_host_path(meshlink_handle_t *mesh, const char *name, char *path, size_t len) {
	snprintf(path, len, "%s" SLASH "hosts" SLASH "%s", mesh->confbase, name);
}

/// Generate a path to an unused invitation file.
static void make_invitation_path(meshlink_handle_t *mesh, const char *name, char *path, size_t len) {
	snprintf(path, len, "%s" SLASH "invitations" SLASH "%s", mesh->confbase, name);
}

/// Generate a path to a used invitation file.
static void make_used_invitation_path(meshlink_handle_t *mesh, const char *name, char *path, size_t len) {
	snprintf(path, len, "%s" SLASH "invitations" SLASH "%s.used", mesh->confbase, name);
}

/// Remove a directory recursively
static void deltree(const char *dirname) {
	DIR *d = opendir(dirname);

	if(d) {
		struct dirent *ent;

		while((ent = readdir(d))) {
			if(ent->d_name[0] == '.') {
				continue;
			}

			char filename[PATH_MAX];
			snprintf(filename, sizeof(filename), "%s" SLASH "%s", dirname, ent->d_name);

			if(unlink(filename)) {
				deltree(filename);
			}
		}

		closedir(d);
	}

	rmdir(dirname);
}

/// Create a fresh configuration directory
bool config_init(meshlink_handle_t *mesh) {
	if(mkdir(mesh->confbase, 0700) && errno != EEXIST) {
		logger(mesh, MESHLINK_DEBUG, "Could not create directory %s: %s\n", mesh->confbase, strerror(errno));
		return false;
	}

	char path[PATH_MAX];

	// Remove meshlink.conf
	snprintf(path, sizeof(path), "%s" SLASH "meshlink.conf", mesh->confbase);
	unlink(path);

	// Remove any host config files
	snprintf(path, sizeof(path), "%s" SLASH "hosts", mesh->confbase);
	deltree(path);

	if(mkdir(path, 0700) && errno != EEXIST) {
		logger(mesh, MESHLINK_DEBUG, "Could not create directory %s: %s\n", path, strerror(errno));
		return false;
	}

	// Remove any invitation files
	snprintf(path, sizeof(path), "%s" SLASH "invitations", mesh->confbase);
	deltree(path);

	if(mkdir(path, 0700) && errno != EEXIST) {
		logger(mesh, MESHLINK_DEBUG, "Could not create directory %s: %s\n", path, strerror(errno));
		return false;
	}

	return true;
}

/// Wipe an existing configuration directory
bool config_destroy(const char *confbase) {
	char path[PATH_MAX];

	// Remove meshlink.conf
	snprintf(path, sizeof(path), "%s" SLASH "meshlink.conf", confbase);

	if(unlink(path)) {
		if(errno == ENOENT) {
			meshlink_errno = MESHLINK_ENOENT;
			return false;
		} else {
			logger(NULL, MESHLINK_ERROR, "Cannot delete %s: %s\n", path, strerror(errno));
			meshlink_errno = MESHLINK_ESTORAGE;
			return false;
		}
	}

	deltree(confbase);
	return true;
}

/// Check the presence of the main configuration file.
bool main_config_exists(meshlink_handle_t *mesh) {
	char path[PATH_MAX];
	make_main_path(mesh, path, sizeof(path));

	return access(path, F_OK) == 0;
}

/// Lock the main configuration file.
bool main_config_lock(meshlink_handle_t *mesh) {
	char path[PATH_MAX];
	make_main_path(mesh, path, sizeof(path));

	mesh->conffile = fopen(path, "r");

	if(!mesh->conffile) {
		logger(NULL, MESHLINK_ERROR, "Cannot not open %s: %s\n", path, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

#ifdef FD_CLOEXEC
	fcntl(fileno(mesh->conffile), F_SETFD, FD_CLOEXEC);
#endif

#ifdef HAVE_MINGW
	// TODO: use _locking()?
#else

	if(flock(fileno(mesh->conffile), LOCK_EX | LOCK_NB) != 0) {
		logger(NULL, MESHLINK_ERROR, "Cannot lock %s: %s\n", path, strerror(errno));
		fclose(mesh->conffile);
		mesh->conffile = NULL;
		meshlink_errno = MESHLINK_EBUSY;
		return false;
	}

#endif

	return true;
}

/// Unlock the main configuration file.
void main_config_unlock(meshlink_handle_t *mesh) {
	if(mesh->conffile) {
		fclose(mesh->conffile);
		mesh->conffile = NULL;
	}
}

/// Read a configuration file from a FILE handle.
bool config_read_file(meshlink_handle_t *mesh, FILE *f, config_t *config) {
	(void)mesh;
	long len;

	if(fseek(f, 0, SEEK_END) || !(len = ftell(f)) || fseek(f, 0, SEEK_SET)) {
		logger(mesh, MESHLINK_ERROR, "Cannot get config file size: %s\n", strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		fclose(f);
		return false;
	}

	uint8_t *buf = xmalloc(len);

	if(fread(buf, len, 1, f) != 1) {
		logger(mesh, MESHLINK_ERROR, "Cannot read config file: %s\n", strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		fclose(f);
		return false;
	}

	if(mesh->config_key) {
		uint8_t *decrypted = xmalloc(len);
		size_t decrypted_len = len;
		chacha_poly1305_ctx_t *ctx = chacha_poly1305_init();
		chacha_poly1305_set_key(ctx, mesh->config_key);

		if(len > 12 && chacha_poly1305_decrypt_iv96(ctx, buf, buf + 12, len - 12, decrypted, &decrypted_len)) {
			free(buf);
			config->buf = decrypted;
			config->len = decrypted_len;
			return true;
		} else {
			logger(mesh, MESHLINK_ERROR, "Cannot decrypt config file\n");
			meshlink_errno = MESHLINK_ESTORAGE;
			free(decrypted);
			free(buf);
			return false;
		}
	}

	config->buf = buf;
	config->len = len;

	return true;
}

/// Write a configuration file to a FILE handle.
bool config_write_file(meshlink_handle_t *mesh, FILE *f, const config_t *config) {
	if(mesh->config_key) {
		uint8_t buf[config->len + 16];
		size_t len = sizeof(buf);
		uint8_t seqbuf[12];
		randomize(&seqbuf, sizeof(seqbuf));
		chacha_poly1305_ctx_t *ctx = chacha_poly1305_init();
		chacha_poly1305_set_key(ctx, mesh->config_key);
		bool success = false;

		if(chacha_poly1305_encrypt_iv96(ctx, seqbuf, config->buf, config->len, buf, &len)) {
			success = fwrite(seqbuf, sizeof(seqbuf), 1, f) == 1 && fwrite(buf, len, 1, f) == 1;
		} else {
			logger(mesh, MESHLINK_ERROR, "Cannot encrypt config file\n");
			meshlink_errno = MESHLINK_ESTORAGE;
		}

		chacha_poly1305_exit(ctx);
		return success;
	}

	if(fwrite(config->buf, config->len, 1, f) != 1) {
		logger(mesh, MESHLINK_ERROR, "Cannot write config file: %s", strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	return true;
}

/// Free resources of a loaded configuration file.
void config_free(config_t *config) {
	free((uint8_t *)config->buf);
	config->buf = NULL;
	config->len = 0;
}

/// Check the presence of a host configuration file.
bool config_exists(meshlink_handle_t *mesh, const char *name) {
	char path[PATH_MAX];
	make_host_path(mesh, name, path, sizeof(path));

	return access(path, F_OK) == 0;
}

/// Read a host configuration file.
bool config_read(meshlink_handle_t *mesh, const char *name, config_t *config) {
	char path[PATH_MAX];
	make_host_path(mesh, name, path, sizeof(path));

	FILE *f = fopen(path, "r");

	if(!f) {
		logger(mesh, MESHLINK_ERROR, "Failed to open `%s': %s", path, strerror(errno));
		return false;
	}

	if(!config_read_file(mesh, f, config)) {
		logger(mesh, MESHLINK_ERROR, "Failed to read `%s': %s", path, strerror(errno));
		fclose(f);
		return false;
	}

	fclose(f);
	return true;
}

/// Write a host configuration file.
bool config_write(meshlink_handle_t *mesh, const char *name, const config_t *config) {
	char path[PATH_MAX];
	make_host_path(mesh, name, path, sizeof(path));

	FILE *f = fopen(path, "w");

	if(!f) {
		logger(mesh, MESHLINK_ERROR, "Failed to open `%s': %s", path, strerror(errno));
		return false;
	}

	if(!config_write_file(mesh, f, config)) {
		logger(mesh, MESHLINK_ERROR, "Failed to write `%s': %s", path, strerror(errno));
		fclose(f);
		return false;
	}

	fclose(f);
	return true;
}

/// Read the main configuration file.
bool main_config_read(meshlink_handle_t *mesh, config_t *config) {
	char path[PATH_MAX];
	make_main_path(mesh, path, sizeof(path));

	FILE *f = fopen(path, "r");

	if(!f) {
		logger(mesh, MESHLINK_ERROR, "Failed to open `%s': %s", path, strerror(errno));
		return false;
	}

	if(!config_read_file(mesh, f, config)) {
		logger(mesh, MESHLINK_ERROR, "Failed to read `%s': %s", path, strerror(errno));
		fclose(f);
		return false;
	}

	fclose(f);
	return true;
}

/// Write the main configuration file.
bool main_config_write(meshlink_handle_t *mesh, const config_t *config) {
	char path[PATH_MAX];
	make_main_path(mesh, path, sizeof(path));

	FILE *f = fopen(path, "w");

	if(!f) {
		logger(mesh, MESHLINK_ERROR, "Failed to open `%s': %s", path, strerror(errno));
		return false;
	}

	if(!config_write_file(mesh, f, config)) {
		logger(mesh, MESHLINK_ERROR, "Failed to write `%s': %s", path, strerror(errno));
		fclose(f);
		return false;
	}

	fclose(f);
	return true;
}

/// Read an invitation file, and immediately delete it.
bool invitation_read(meshlink_handle_t *mesh, const char *name, config_t *config) {
	char path[PATH_MAX];
	char used_path[PATH_MAX];
	make_invitation_path(mesh, name, path, sizeof(path));
	make_used_invitation_path(mesh, name, used_path, sizeof(used_path));

	// Atomically rename the invitation file
	if(rename(path, used_path)) {
		if(errno == ENOENT) {
			logger(mesh, MESHLINK_ERROR, "Peer tried to use non-existing invitation %s\n", name);
		} else {
			logger(mesh, MESHLINK_ERROR, "Error trying to rename invitation %s\n", name);
		}

		return false;
	}

	FILE *f = fopen(used_path, "r");

	if(!f) {
		logger(mesh, MESHLINK_ERROR, "Failed to open `%s': %s", path, strerror(errno));
		return false;
	}

	// Check the timestamp
	struct stat st;

	if(fstat(fileno(f), &st)) {
		logger(mesh, MESHLINK_ERROR, "Could not stat invitation file %s\n", name);
		fclose(f);
		unlink(used_path);
		return false;
	}

	if(time(NULL) > st.st_mtime + mesh->invitation_timeout) {
		logger(mesh, MESHLINK_ERROR, "Peer tried to use an outdated invitation file %s\n", name);
		fclose(f);
		unlink(used_path);
		return false;
	}

	if(!config_read_file(mesh, f, config)) {
		logger(mesh, MESHLINK_ERROR, "Failed to read `%s': %s", path, strerror(errno));
		fclose(f);
		unlink(used_path);
		return false;
	}

	fclose(f);
	unlink(used_path);
	return true;
}

/// Write an invitation file.
bool invitation_write(meshlink_handle_t *mesh, const char *name, const config_t *config) {
	char path[PATH_MAX];
	make_invitation_path(mesh, name, path, sizeof(path));

	FILE *f = fopen(path, "w");

	if(!f) {
		logger(mesh, MESHLINK_ERROR, "Failed to open `%s': %s", path, strerror(errno));
		return false;
	}

	if(!config_write_file(mesh, f, config)) {
		logger(mesh, MESHLINK_ERROR, "Failed to write `%s': %s", path, strerror(errno));
		fclose(f);
		return false;
	}

	fclose(f);
	return true;
}

/// Purge old invitation files
size_t invitation_purge_old(meshlink_handle_t *mesh, time_t deadline) {
	char path[PATH_MAX];
	make_invitation_path(mesh, "", path, sizeof(path));

	DIR *dir = opendir(path);

	if(!dir) {
		logger(mesh, MESHLINK_DEBUG, "Could not read directory %s: %s\n", path, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return 0;
	}

	errno = 0;
	size_t count = 0;
	struct dirent *ent;

	while((ent = readdir(dir))) {
		if(strlen(ent->d_name) != 24) {
			continue;
		}

		char invname[PATH_MAX];
		struct stat st;

		if(snprintf(invname, sizeof(invname), "%s" SLASH "%s", path, ent->d_name) >= PATH_MAX) {
			logger(mesh, MESHLINK_DEBUG, "Filename too long: %s" SLASH "%s", path, ent->d_name);
			continue;
		}

		if(!stat(invname, &st)) {
			if(mesh->invitation_key && deadline < st.st_mtime) {
				count++;
			} else {
				unlink(invname);
			}
		} else {
			logger(mesh, MESHLINK_DEBUG, "Could not stat %s: %s\n", invname, strerror(errno));
			errno = 0;
		}
	}

	if(errno) {
		logger(mesh, MESHLINK_DEBUG, "Error while reading directory %s: %s\n", path, strerror(errno));
		closedir(dir);
		meshlink_errno = MESHLINK_ESTORAGE;
		return 0;
	}

	closedir(dir);

	return count;
}
