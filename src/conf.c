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
#include <sys/types.h>
#include <utime.h>

#include "conf.h"
#include "crypto.h"
#include "logger.h"
#include "meshlink_internal.h"
#include "xalloc.h"
#include "packmsg.h"

/// Generate a path to the main configuration file.
static void make_main_path(meshlink_handle_t *mesh, const char *conf_subdir, char *path, size_t len) {
	snprintf(path, len, "%s" SLASH "%s" SLASH "meshlink.conf", mesh->confbase, conf_subdir);
}

/// Generate a path to a host configuration file.
static void make_host_path(meshlink_handle_t *mesh, const char *conf_subdir, const char *name, char *path, size_t len) {
	snprintf(path, len, "%s" SLASH "%s" SLASH "hosts" SLASH "%s", mesh->confbase, conf_subdir, name);
}

/// Generate a path to an unused invitation file.
static void make_invitation_path(meshlink_handle_t *mesh, const char *conf_subdir, const char *name, char *path, size_t len) {
	snprintf(path, len, "%s" SLASH "%s" SLASH "invitations" SLASH "%s", mesh->confbase, conf_subdir, name);
}

/// Generate a path to a used invitation file.
static void make_used_invitation_path(meshlink_handle_t *mesh, const char *conf_subdir, const char *name, char *path, size_t len) {
	snprintf(path, len, "%s" SLASH "%s" SLASH "invitations" SLASH "%s.used", mesh->confbase, conf_subdir, name);
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

static bool sync_path(const char *pathname) {
	int fd = open(pathname, O_RDONLY);

	if(fd < 0) {
		logger(NULL, MESHLINK_ERROR, "Failed to open %s: %s\n", pathname, strerror(errno));
		return false;
	}

	if(fsync(fd)) {
		logger(NULL, MESHLINK_ERROR, "Failed to sync %s: %s\n", pathname, strerror(errno));
		close(fd);
		return false;
	}

	if(close(fd)) {
		logger(NULL, MESHLINK_ERROR, "Failed to close %s: %s\n", pathname, strerror(errno));
		close(fd);
		return false;
	}

	return true;
}

/// Try decrypting the main configuration file from the given sub-directory.
static bool main_config_decrypt(meshlink_handle_t *mesh, const char *conf_subdir) {
	if(!mesh->config_key && !mesh->confbase && !conf_subdir) {
		return false;
	}

	config_t config;

	if(!main_config_read(mesh, conf_subdir, &config, mesh->config_key)) {
		logger(mesh, MESHLINK_ERROR, "Could not read main configuration file");
		return false;
	}

	packmsg_input_t in = {config.buf, config.len};

	uint32_t version = packmsg_get_uint32(&in);
	config_free(&config);

	return version == MESHLINK_CONFIG_VERSION;
}

/// Create a fresh configuration directory
bool config_init(meshlink_handle_t *mesh, const char *conf_subdir) {
	if(!mesh->confbase) {
		return true;
	}

	if(!conf_subdir) {
		return false;
	}

	if(mkdir(mesh->confbase, 0700) && errno != EEXIST) {
		logger(mesh, MESHLINK_DEBUG, "Could not create directory %s: %s\n", mesh->confbase, strerror(errno));
		return false;
	}

	char path[PATH_MAX];

	// Create "current" sub-directory in the confbase
	snprintf(path, sizeof(path), "%s" SLASH "%s", mesh->confbase, conf_subdir);
	deltree(path);

	if(mkdir(path, 0700)) {
		logger(mesh, MESHLINK_DEBUG, "Could not create directory %s: %s\n", path, strerror(errno));
		return false;
	}

	make_host_path(mesh, conf_subdir, "", path, sizeof(path));

	if(mkdir(path, 0700)) {
		logger(mesh, MESHLINK_DEBUG, "Could not create directory %s: %s\n", path, strerror(errno));
		return false;
	}

	make_invitation_path(mesh, conf_subdir, "", path, sizeof(path));

	if(mkdir(path, 0700)) {
		logger(mesh, MESHLINK_DEBUG, "Could not create directory %s: %s\n", path, strerror(errno));
		return false;
	}

	return true;
}

/// Wipe an existing configuration directory
bool config_destroy(const char *confbase, const char *conf_subdir) {
	if(!confbase && !conf_subdir) {
		return false;
	}

	struct stat st;

	char path[PATH_MAX];

	// Check the presence of configuration base sub directory.
	snprintf(path, sizeof(path), "%s" SLASH "%s", confbase, conf_subdir);

	if(stat(path, &st)) {
		if(errno == ENOENT) {
			return true;
		} else {
			logger(NULL, MESHLINK_ERROR, "Cannot stat %s: %s\n", path, strerror(errno));
			meshlink_errno = MESHLINK_ESTORAGE;
			return false;
		}
	}

	// Remove meshlink.conf
	snprintf(path, sizeof(path), "%s" SLASH "%s" SLASH "meshlink.conf", confbase, conf_subdir);

	if(unlink(path)) {
		if(errno != ENOENT) {
			logger(NULL, MESHLINK_ERROR, "Cannot delete %s: %s\n", path, strerror(errno));
			meshlink_errno = MESHLINK_ESTORAGE;
			return false;
		}
	}

	snprintf(path, sizeof(path), "%s" SLASH "%s", confbase, conf_subdir);
	deltree(path);
	return true;
}

static bool copytree(const char *src_dir_name, const void *src_key, const char *dst_dir_name, const void *dst_key) {
	if(!src_dir_name || !dst_dir_name) {
		return false;
	}

	char src_filename[PATH_MAX];
	char dst_filename[PATH_MAX];
	struct dirent *ent;

	DIR *src_dir = opendir(src_dir_name);

	if(!src_dir) {
		logger(NULL, MESHLINK_ERROR, "Could not open directory file %s\n", src_dir_name);
		return false;
	}

	// Delete if already exists and create a new destination directory
	deltree(dst_dir_name);

	if(mkdir(dst_dir_name, 0700)) {
		logger(NULL, MESHLINK_ERROR, "Could not create directory %s\n", dst_filename);
		return false;
	}

	while((ent = readdir(src_dir))) {
		if(ent->d_name[0] == '.') {
			continue;
		}

		snprintf(dst_filename, sizeof(dst_filename), "%s" SLASH "%s", dst_dir_name, ent->d_name);
		snprintf(src_filename, sizeof(src_filename), "%s" SLASH "%s", src_dir_name, ent->d_name);

		if(ent->d_type == DT_DIR) {
			if(!copytree(src_filename, src_key, dst_filename, dst_key)) {
				logger(NULL, MESHLINK_ERROR, "Copying %s to %s failed\n", src_filename, dst_filename);
				return false;
			}

			if(!sync_path(dst_filename)) {
				return false;
			}
		} else if(ent->d_type == DT_REG) {
			struct stat st;
			config_t config;

			if(stat(src_filename, &st)) {
				logger(NULL, MESHLINK_ERROR, "Could not stat file `%s': %s\n", src_filename, strerror(errno));
				return false;
			}

			FILE *f = fopen(src_filename, "r");

			if(!f) {
				logger(NULL, MESHLINK_ERROR, "Failed to open `%s': %s\n", src_filename, strerror(errno));
				return false;
			}

			if(!config_read_file(NULL, f, &config, src_key)) {
				logger(NULL, MESHLINK_ERROR, "Failed to read `%s': %s\n", src_filename, strerror(errno));
				fclose(f);
				return false;
			}

			if(fclose(f)) {
				logger(NULL, MESHLINK_ERROR, "Failed to close `%s': %s\n", src_filename, strerror(errno));
				config_free(&config);
				return false;
			}

			f = fopen(dst_filename, "w");

			if(!f) {
				logger(NULL, MESHLINK_ERROR, "Failed to open `%s': %s", dst_filename, strerror(errno));
				config_free(&config);
				return false;
			}

			if(!config_write_file(NULL, f, &config, dst_key)) {
				logger(NULL, MESHLINK_ERROR, "Failed to write `%s': %s", dst_filename, strerror(errno));
				config_free(&config);
				fclose(f);
				return false;
			}

			if(fclose(f)) {
				logger(NULL, MESHLINK_ERROR, "Failed to close `%s': %s", dst_filename, strerror(errno));
				config_free(&config);
				return false;
			}

			config_free(&config);

			struct utimbuf times;
			times.modtime = st.st_mtime;
			times.actime = st.st_atime;

			if(utime(dst_filename, &times)) {
				logger(NULL, MESHLINK_ERROR, "Failed to utime `%s': %s", dst_filename, strerror(errno));
				return false;
			}
		}
	}

	closedir(src_dir);
	return true;
}

bool config_copy(meshlink_handle_t *mesh, const char *src_dir_name, const void *src_key, const char *dst_dir_name, const void *dst_key) {
	char src_filename[PATH_MAX];
	char dst_filename[PATH_MAX];

	snprintf(dst_filename, sizeof(dst_filename), "%s" SLASH "%s", mesh->confbase, dst_dir_name);
	snprintf(src_filename, sizeof(src_filename), "%s" SLASH "%s", mesh->confbase, src_dir_name);

	return copytree(src_filename, src_key, dst_filename, dst_key);
}

/// Check the presence of the main configuration file.
bool main_config_exists(meshlink_handle_t *mesh, const char *conf_subdir) {
	if(!mesh->confbase && !conf_subdir) {
		return false;
	}

	char path[PATH_MAX];
	make_main_path(mesh, conf_subdir, path, sizeof(path));
	return access(path, F_OK) == 0;
}

bool config_rename(meshlink_handle_t *mesh, const char *old_conf_subdir, const char *new_conf_subdir) {
	if(!mesh->confbase && !old_conf_subdir && !new_conf_subdir) {
		return false;
	}

	char old_path[PATH_MAX];
	char new_path[PATH_MAX];

	snprintf(old_path, sizeof(old_path), "%s" SLASH "%s", mesh->confbase, old_conf_subdir);
	snprintf(new_path, sizeof(new_path), "%s" SLASH "%s", mesh->confbase, new_conf_subdir);

	return rename(old_path, new_path) == 0;
}

bool meshlink_confbase_exists(meshlink_handle_t *mesh) {
	if(!mesh->confbase) {
		return false;
	}

	bool confbase_exists = false;
	bool confbase_decryptable = false;

	if(main_config_exists(mesh, "current")) {
		confbase_exists = true;

		if(mesh->config_key && main_config_decrypt(mesh, "current")) {
			confbase_decryptable = true;
		}
	}

	if(mesh->config_key && !confbase_decryptable && main_config_exists(mesh, "new")) {
		confbase_exists = true;

		if(main_config_decrypt(mesh, "new")) {
			if(!config_destroy(mesh->confbase, "current")) {
				return false;
			}

			if(!config_rename(mesh, "new", "current")) {
				return false;
			}

			confbase_decryptable = true;
		}
	}

	if(mesh->config_key && !confbase_decryptable && main_config_exists(mesh, "old")) {
		confbase_exists = true;

		if(main_config_decrypt(mesh, "old")) {
			if(!config_destroy(mesh->confbase, "current")) {
				return false;
			}

			if(!config_rename(mesh, "old", "current")) {
				return false;
			}

			confbase_decryptable = true;
		}
	}

	// Cleanup if current is existing with old and new
	if(confbase_exists && confbase_decryptable) {
		config_destroy(mesh->confbase, "old");
		config_destroy(mesh->confbase, "new");
	}

	return confbase_exists;
}

/// Lock the main configuration file.
bool main_config_lock(meshlink_handle_t *mesh) {
	if(!mesh->confbase) {
		return true;
	}

	char path[PATH_MAX];
	make_main_path(mesh, "current", path, sizeof(path));

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
bool config_read_file(meshlink_handle_t *mesh, FILE *f, config_t *config, const void *key) {
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

	if(key) {
		uint8_t *decrypted = xmalloc(len);
		size_t decrypted_len = len;
		chacha_poly1305_ctx_t *ctx = chacha_poly1305_init();
		chacha_poly1305_set_key(ctx, key);

		if(len > 12 && chacha_poly1305_decrypt_iv96(ctx, buf, buf + 12, len - 12, decrypted, &decrypted_len)) {
			chacha_poly1305_exit(ctx);
			free(buf);
			config->buf = decrypted;
			config->len = decrypted_len;
			return true;
		} else {
			logger(mesh, MESHLINK_ERROR, "Cannot decrypt config file\n");
			meshlink_errno = MESHLINK_ESTORAGE;
			chacha_poly1305_exit(ctx);
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
bool config_write_file(meshlink_handle_t *mesh, FILE *f, const config_t *config, const void *key) {
	if(key) {
		uint8_t buf[config->len + 16];
		size_t len = sizeof(buf);
		uint8_t seqbuf[12];
		randomize(&seqbuf, sizeof(seqbuf));
		chacha_poly1305_ctx_t *ctx = chacha_poly1305_init();
		chacha_poly1305_set_key(ctx, key);
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

	if(fsync(fileno(f))) {
		logger(mesh, MESHLINK_ERROR, "Failed to sync file: %s\n", strerror(errno));
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
bool config_exists(meshlink_handle_t *mesh, const char *conf_subdir, const char *name) {
	if(!mesh->confbase && !conf_subdir) {
		return false;
	}

	char path[PATH_MAX];
	make_host_path(mesh, conf_subdir, name, path, sizeof(path));

	return access(path, F_OK) == 0;
}

/// Read a host configuration file.
bool config_read(meshlink_handle_t *mesh, const char *conf_subdir, const char *name, config_t *config, void *key) {
	if(!mesh->confbase && !conf_subdir) {
		return false;
	}

	char path[PATH_MAX];
	make_host_path(mesh, conf_subdir, name, path, sizeof(path));

	FILE *f = fopen(path, "r");

	if(!f) {
		logger(mesh, MESHLINK_ERROR, "Failed to open `%s': %s", path, strerror(errno));
		return false;
	}

	if(!config_read_file(mesh, f, config, key)) {
		logger(mesh, MESHLINK_ERROR, "Failed to read `%s': %s", path, strerror(errno));
		fclose(f);
		return false;
	}

	fclose(f);

	return true;
}

bool config_scan_all(meshlink_handle_t *mesh, const char *conf_subdir, const char *conf_type, config_scan_action_t action, void *arg) {
	if(!mesh->confbase && !conf_subdir && !conf_type) {
		return false;
	}

	DIR *dir;
	struct dirent *ent;
	char dname[PATH_MAX];
	snprintf(dname, sizeof(dname), "%s" SLASH "%s" SLASH "%s", mesh->confbase, conf_subdir, conf_type);

	dir = opendir(dname);

	if(!dir) {
		logger(mesh, MESHLINK_ERROR, "Could not open %s: %s", dname, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	while((ent = readdir(dir))) {
		if(ent->d_name[0] == '.') {
			continue;
		}

		if(!action(mesh, ent->d_name, arg)) {
			closedir(dir);
			return false;
		}
	}

	closedir(dir);
	return true;
}

/// Write a host configuration file.
bool config_write(meshlink_handle_t *mesh, const char *conf_subdir, const char *name, const config_t *config, void *key) {
	if(!mesh->confbase && !conf_subdir && !name) {
		return true;
	}

	char path[PATH_MAX];
	make_host_path(mesh, conf_subdir, name, path, sizeof(path));

	FILE *f = fopen(path, "w");

	if(!f) {
		logger(mesh, MESHLINK_ERROR, "Failed to open `%s': %s", path, strerror(errno));
		return false;
	}

	if(!config_write_file(mesh, f, config, key)) {
		logger(mesh, MESHLINK_ERROR, "Failed to write `%s': %s", path, strerror(errno));
		fclose(f);
		return false;
	}

	if(fclose(f)) {
		logger(mesh, MESHLINK_ERROR, "Failed to close `%s': %s", path, strerror(errno));
		return false;
	}

	return true;
}

/// Read the main configuration file.
bool main_config_read(meshlink_handle_t *mesh, const char *conf_subdir, config_t *config, void *key) {
	if(!mesh->confbase && !conf_subdir) {
		return false;
	}

	char path[PATH_MAX];
	make_main_path(mesh, conf_subdir, path, sizeof(path));

	FILE *f = fopen(path, "r");

	if(!f) {
		logger(mesh, MESHLINK_ERROR, "Failed to open `%s': %s", path, strerror(errno));
		return false;
	}

	if(!config_read_file(mesh, f, config, key)) {
		logger(mesh, MESHLINK_ERROR, "Failed to read `%s': %s", path, strerror(errno));
		fclose(f);
		return false;
	}

	fclose(f);

	return true;
}

/// Write the main configuration file.
bool main_config_write(meshlink_handle_t *mesh, const char *conf_subdir, const config_t *config, void *key) {
	if(!mesh->confbase && !conf_subdir) {
		return true;
	}

	char path[PATH_MAX];
	make_main_path(mesh, conf_subdir, path, sizeof(path));

	FILE *f = fopen(path, "w");

	if(!f) {
		logger(mesh, MESHLINK_ERROR, "Failed to open `%s': %s", path, strerror(errno));
		return false;
	}

	if(!config_write_file(mesh, f, config, key)) {
		logger(mesh, MESHLINK_ERROR, "Failed to write `%s': %s", path, strerror(errno));
		fclose(f);
		return false;
	}

	if(fclose(f)) {
		logger(mesh, MESHLINK_ERROR, "Failed to close `%s': %s", path, strerror(errno));
		return false;
	}

	return true;
}

/// Read an invitation file from the confbase sub-directory, and immediately delete it.
bool invitation_read(meshlink_handle_t *mesh, const char *conf_subdir, const char *name, config_t *config, void *key) {
	if(!mesh->confbase && !conf_subdir) {
		return false;
	}

	char path[PATH_MAX];
	char used_path[PATH_MAX];
	make_invitation_path(mesh, conf_subdir, name, path, sizeof(path));
	make_used_invitation_path(mesh, conf_subdir, name, used_path, sizeof(used_path));

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

	if(!config_read_file(mesh, f, config, key)) {
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
bool invitation_write(meshlink_handle_t *mesh, const char *conf_subdir, const char *name, const config_t *config, void *key) {
	if(!mesh->confbase && !conf_subdir) {
		return false;
	}

	char path[PATH_MAX];
	make_invitation_path(mesh, conf_subdir, name, path, sizeof(path));

	FILE *f = fopen(path, "w");

	if(!f) {
		logger(mesh, MESHLINK_ERROR, "Failed to open `%s': %s", path, strerror(errno));
		return false;
	}

	if(!config_write_file(mesh, f, config, key)) {
		logger(mesh, MESHLINK_ERROR, "Failed to write `%s': %s", path, strerror(errno));
		fclose(f);
		return false;
	}

	if(fclose(f)) {
		logger(mesh, MESHLINK_ERROR, "Failed to close `%s': %s", path, strerror(errno));
		return false;
	}

	return true;
}

/// Purge old invitation files
size_t invitation_purge_old(meshlink_handle_t *mesh, time_t deadline) {
	if(!mesh->confbase) {
		return true;
	}

	char path[PATH_MAX];
	make_invitation_path(mesh, "current", "", path, sizeof(path));

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
