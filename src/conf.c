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
	assert(conf_subdir);
	assert(path);
	assert(len);

	snprintf(path, len, "%s" SLASH "%s" SLASH "meshlink.conf", mesh->confbase, conf_subdir);
}

/// Generate a path to a host configuration file.
static void make_host_path(meshlink_handle_t *mesh, const char *conf_subdir, const char *name, char *path, size_t len) {
	assert(conf_subdir);
	assert(name);
	assert(path);
	assert(len);

	snprintf(path, len, "%s" SLASH "%s" SLASH "hosts" SLASH "%s", mesh->confbase, conf_subdir, name);
}

/// Generate a path to an unused invitation file.
static void make_invitation_path(meshlink_handle_t *mesh, const char *conf_subdir, const char *name, char *path, size_t len) {
	assert(conf_subdir);
	assert(name);
	assert(path);
	assert(len);

	snprintf(path, len, "%s" SLASH "%s" SLASH "invitations" SLASH "%s", mesh->confbase, conf_subdir, name);
}

/// Generate a path to a used invitation file.
static void make_used_invitation_path(meshlink_handle_t *mesh, const char *conf_subdir, const char *name, char *path, size_t len) {
	assert(conf_subdir);
	assert(name);
	assert(path);
	assert(len);

	snprintf(path, len, "%s" SLASH "%s" SLASH "invitations" SLASH "%s.used", mesh->confbase, conf_subdir, name);
}

/// Remove a directory recursively
static bool deltree(const char *dirname) {
	assert(dirname);

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
				if(!deltree(filename)) {
					return false;
				}
			}
		}

		closedir(d);
	} else {
		return errno == ENOENT;
	}

	return rmdir(dirname) == 0;
}

bool sync_path(const char *pathname) {
	assert(pathname);

	int fd = open(pathname, O_RDONLY);

	if(fd < 0) {
		logger(NULL, MESHLINK_ERROR, "Failed to open %s: %s\n", pathname, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	if(fsync(fd)) {
		logger(NULL, MESHLINK_ERROR, "Failed to sync %s: %s\n", pathname, strerror(errno));
		close(fd);
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	if(close(fd)) {
		logger(NULL, MESHLINK_ERROR, "Failed to close %s: %s\n", pathname, strerror(errno));
		close(fd);
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	return true;
}

/// Try decrypting the main configuration file from the given sub-directory.
static bool main_config_decrypt(meshlink_handle_t *mesh, const char *conf_subdir) {
	assert(mesh->config_key);
	assert(mesh->confbase);
	assert(conf_subdir);

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
	assert(conf_subdir);

	if(!mesh->confbase) {
		return true;
	}

	char path[PATH_MAX];

	// Create "current" sub-directory in the confbase
	snprintf(path, sizeof(path), "%s" SLASH "%s", mesh->confbase, conf_subdir);

	if(!deltree(path)) {
		logger(mesh, MESHLINK_DEBUG, "Could not delete directory %s: %s\n", path, strerror(errno));
		return false;
	}

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
	assert(conf_subdir);

	if(!confbase) {
		return true;
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

	if(!deltree(path)) {
		logger(NULL, MESHLINK_ERROR, "Cannot delete %s: %s\n", path, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	return sync_path(confbase);
}

static bool copytree(const char *src_dir_name, const void *src_key, const char *dst_dir_name, const void *dst_key) {
	assert(src_dir_name);
	assert(dst_dir_name);

	char src_filename[PATH_MAX];
	char dst_filename[PATH_MAX];
	struct dirent *ent;

	DIR *src_dir = opendir(src_dir_name);

	if(!src_dir) {
		logger(NULL, MESHLINK_ERROR, "Could not open directory file %s\n", src_dir_name);
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	// Delete if already exists and create a new destination directory
	if(!deltree(dst_dir_name)) {
		logger(NULL, MESHLINK_ERROR, "Cannot delete %s: %s\n", dst_dir_name, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	if(mkdir(dst_dir_name, 0700)) {
		logger(NULL, MESHLINK_ERROR, "Could not create directory %s\n", dst_filename);
		meshlink_errno = MESHLINK_ESTORAGE;
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
				meshlink_errno = MESHLINK_ESTORAGE;
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
				meshlink_errno = MESHLINK_ESTORAGE;
				return false;
			}

			FILE *f = fopen(src_filename, "r");

			if(!f) {
				logger(NULL, MESHLINK_ERROR, "Failed to open `%s': %s\n", src_filename, strerror(errno));
				meshlink_errno = MESHLINK_ESTORAGE;
				return false;
			}

			if(!config_read_file(NULL, f, &config, src_key)) {
				logger(NULL, MESHLINK_ERROR, "Failed to read `%s': %s\n", src_filename, strerror(errno));
				fclose(f);
				meshlink_errno = MESHLINK_ESTORAGE;
				return false;
			}

			if(fclose(f)) {
				logger(NULL, MESHLINK_ERROR, "Failed to close `%s': %s\n", src_filename, strerror(errno));
				config_free(&config);
				meshlink_errno = MESHLINK_ESTORAGE;
				return false;
			}

			f = fopen(dst_filename, "w");

			if(!f) {
				logger(NULL, MESHLINK_ERROR, "Failed to open `%s': %s", dst_filename, strerror(errno));
				config_free(&config);
				meshlink_errno = MESHLINK_ESTORAGE;
				return false;
			}

			if(!config_write_file(NULL, f, &config, dst_key)) {
				logger(NULL, MESHLINK_ERROR, "Failed to write `%s': %s", dst_filename, strerror(errno));
				config_free(&config);
				fclose(f);
				meshlink_errno = MESHLINK_ESTORAGE;
				return false;
			}

			if(fclose(f)) {
				logger(NULL, MESHLINK_ERROR, "Failed to close `%s': %s", dst_filename, strerror(errno));
				config_free(&config);
				meshlink_errno = MESHLINK_ESTORAGE;
				return false;
			}

			config_free(&config);

			struct utimbuf times;
			times.modtime = st.st_mtime;
			times.actime = st.st_atime;

			if(utime(dst_filename, &times)) {
				logger(NULL, MESHLINK_ERROR, "Failed to utime `%s': %s", dst_filename, strerror(errno));
				meshlink_errno = MESHLINK_ESTORAGE;
				return false;
			}
		}
	}

	closedir(src_dir);
	return true;
}

bool config_copy(meshlink_handle_t *mesh, const char *src_dir_name, const void *src_key, const char *dst_dir_name, const void *dst_key) {
	assert(src_dir_name);
	assert(dst_dir_name);

	char src_filename[PATH_MAX];
	char dst_filename[PATH_MAX];

	snprintf(dst_filename, sizeof(dst_filename), "%s" SLASH "%s", mesh->confbase, dst_dir_name);
	snprintf(src_filename, sizeof(src_filename), "%s" SLASH "%s", mesh->confbase, src_dir_name);

	return copytree(src_filename, src_key, dst_filename, dst_key);
}

/// Check the presence of the main configuration file.
bool main_config_exists(meshlink_handle_t *mesh, const char *conf_subdir) {
	assert(conf_subdir);

	if(!mesh->confbase) {
		return false;
	}

	char path[PATH_MAX];
	make_main_path(mesh, conf_subdir, path, sizeof(path));
	return access(path, F_OK) == 0;
}

bool config_rename(meshlink_handle_t *mesh, const char *old_conf_subdir, const char *new_conf_subdir) {
	assert(old_conf_subdir);
	assert(new_conf_subdir);

	if(!mesh->confbase) {
		return false;
	}

	char old_path[PATH_MAX];
	char new_path[PATH_MAX];

	snprintf(old_path, sizeof(old_path), "%s" SLASH "%s", mesh->confbase, old_conf_subdir);
	snprintf(new_path, sizeof(new_path), "%s" SLASH "%s", mesh->confbase, new_conf_subdir);

	return rename(old_path, new_path) == 0 && sync_path(mesh->confbase);
}

bool config_sync(meshlink_handle_t *mesh, const char *conf_subdir) {
	assert(conf_subdir);

	if(!mesh->confbase) {
		return true;
	}

	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s" SLASH "%s" SLASH "hosts", mesh->confbase, conf_subdir);

	if(!sync_path(path)) {
		return false;
	}

	snprintf(path, sizeof(path), "%s" SLASH "%s", mesh->confbase, conf_subdir);

	if(!sync_path(path)) {
		return false;
	}

	return true;
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
		if(!config_destroy(mesh->confbase, "old") || !config_destroy(mesh->confbase, "new")) {
			return false;
		}
	}

	return confbase_exists;
}

/// Lock the main configuration file. Creates confbase if necessary.
bool main_config_lock(meshlink_handle_t *mesh) {
	if(!mesh->confbase) {
		return true;
	}

	if(mkdir(mesh->confbase, 0700) && errno != EEXIST) {
		logger(NULL, MESHLINK_ERROR, "Cannot create configuration directory %s: %s", mesh->confbase, strerror(errno));
		meshlink_close(mesh);
		meshlink_errno = MESHLINK_ESTORAGE;
		return NULL;
	}

	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s" SLASH "meshlink.lock", mesh->confbase);

	mesh->lockfile = fopen(path, "w+");

	if(!mesh->lockfile) {
		logger(NULL, MESHLINK_ERROR, "Cannot not open %s: %s\n", path, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

#ifdef FD_CLOEXEC
	fcntl(fileno(mesh->lockfile), F_SETFD, FD_CLOEXEC);
#endif

#ifdef HAVE_MINGW
	// TODO: use _locking()?
#else

	if(flock(fileno(mesh->lockfile), LOCK_EX | LOCK_NB) != 0) {
		logger(NULL, MESHLINK_ERROR, "Cannot lock %s: %s\n", path, strerror(errno));
		fclose(mesh->lockfile);
		mesh->lockfile = NULL;
		meshlink_errno = MESHLINK_EBUSY;
		return false;
	}

#endif

	return true;
}

/// Unlock the main configuration file.
void main_config_unlock(meshlink_handle_t *mesh) {
	if(mesh->lockfile) {
		fclose(mesh->lockfile);
		mesh->lockfile = NULL;
	}
}

/// Read a configuration file from a FILE handle.
bool config_read_file(meshlink_handle_t *mesh, FILE *f, config_t *config, const void *key) {
	assert(f);

	long len;

	if(fseek(f, 0, SEEK_END) || !(len = ftell(f)) || fseek(f, 0, SEEK_SET)) {
		logger(mesh, MESHLINK_ERROR, "Cannot get config file size: %s\n", strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	uint8_t *buf = xmalloc(len);

	if(fread(buf, len, 1, f) != 1) {
		logger(mesh, MESHLINK_ERROR, "Cannot read config file: %s\n", strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
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
	assert(f);

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

			if(!success) {
				logger(mesh, MESHLINK_ERROR, "Cannot write config file: %s", strerror(errno));
			}

			meshlink_errno = MESHLINK_ESTORAGE;
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

	if(fflush(f)) {
		logger(mesh, MESHLINK_ERROR, "Failed to flush file: %s", strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	if(fsync(fileno(f))) {
		logger(mesh, MESHLINK_ERROR, "Failed to sync file: %s\n", strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	return true;
}

/// Free resources of a loaded configuration file.
void config_free(config_t *config) {
	assert(!config->len || config->buf);

	free((uint8_t *)config->buf);
	config->buf = NULL;
	config->len = 0;
}

/// Check the presence of a host configuration file.
bool config_exists(meshlink_handle_t *mesh, const char *conf_subdir, const char *name) {
	assert(conf_subdir);

	if(!mesh->confbase) {
		return false;
	}

	char path[PATH_MAX];
	make_host_path(mesh, conf_subdir, name, path, sizeof(path));

	return access(path, F_OK) == 0;
}

/// Read a host configuration file.
bool config_read(meshlink_handle_t *mesh, const char *conf_subdir, const char *name, config_t *config, void *key) {
	assert(conf_subdir);

	if(!mesh->confbase) {
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
	assert(conf_subdir);
	assert(conf_type);

	if(!mesh->confbase) {
		return true;
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
	assert(conf_subdir);
	assert(name);
	assert(config);

	if(!mesh->confbase) {
		return true;
	}

	char path[PATH_MAX];
	char tmp_path[PATH_MAX + 4];
	make_host_path(mesh, conf_subdir, name, path, sizeof(path));
	snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

	FILE *f = fopen(tmp_path, "w");

	if(!f) {
		logger(mesh, MESHLINK_ERROR, "Failed to open `%s': %s", tmp_path, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	if(!config_write_file(mesh, f, config, key)) {
		logger(mesh, MESHLINK_ERROR, "Failed to write `%s': %s", tmp_path, strerror(errno));
		fclose(f);
		return false;
	}

	if(fclose(f)) {
		logger(mesh, MESHLINK_ERROR, "Failed to close `%s': %s", tmp_path, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	if(rename(tmp_path, path)) {
		logger(mesh, MESHLINK_ERROR, "Failed to rename `%s' to `%s': %s", tmp_path, path, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	return true;
}

/// Delete a host configuration file.
bool config_delete(meshlink_handle_t *mesh, const char *conf_subdir, const char *name) {
	assert(conf_subdir);
	assert(name);

	if(!mesh->confbase) {
		return true;
	}

	char path[PATH_MAX];
	make_host_path(mesh, conf_subdir, name, path, sizeof(path));

	if(unlink(path) && errno != ENOENT) {
		logger(mesh, MESHLINK_ERROR, "Failed to unlink `%s': %s", path, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	return true;
}

/// Read the main configuration file.
bool main_config_read(meshlink_handle_t *mesh, const char *conf_subdir, config_t *config, void *key) {
	assert(conf_subdir);
	assert(config);

	if(!mesh->confbase) {
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
	assert(conf_subdir);
	assert(config);

	if(!mesh->confbase) {
		return true;
	}

	char path[PATH_MAX];
	char tmp_path[PATH_MAX + 4];
	make_main_path(mesh, conf_subdir, path, sizeof(path));
	snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

	FILE *f = fopen(tmp_path, "w");

	if(!f) {
		logger(mesh, MESHLINK_ERROR, "Failed to open `%s': %s", tmp_path, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	if(!config_write_file(mesh, f, config, key)) {
		logger(mesh, MESHLINK_ERROR, "Failed to write `%s': %s", tmp_path, strerror(errno));
		fclose(f);
		return false;
	}

	if(rename(tmp_path, path)) {
		logger(mesh, MESHLINK_ERROR, "Failed to rename `%s' to `%s': %s", tmp_path, path, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		fclose(f);
		return false;
	}

	if(fclose(f)) {
		logger(mesh, MESHLINK_ERROR, "Failed to close `%s': %s", tmp_path, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	return true;
}

/// Read an invitation file from the confbase sub-directory, and immediately delete it.
bool invitation_read(meshlink_handle_t *mesh, const char *conf_subdir, const char *name, config_t *config, void *key) {
	assert(conf_subdir);
	assert(name);
	assert(config);

	if(!mesh->confbase) {
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

	if(time(NULL) >= st.st_mtime + mesh->invitation_timeout) {
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

	if(unlink(used_path)) {
		logger(mesh, MESHLINK_ERROR, "Failed to unlink `%s': %s", path, strerror(errno));
		return false;
	}

	snprintf(path, sizeof(path), "%s" SLASH "%s" SLASH "invitations", mesh->confbase, conf_subdir);

	if(!sync_path(path)) {
		logger(mesh, MESHLINK_ERROR, "Failed to sync `%s': %s", path, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	return true;
}

/// Write an invitation file.
bool invitation_write(meshlink_handle_t *mesh, const char *conf_subdir, const char *name, const config_t *config, void *key) {
	assert(conf_subdir);
	assert(name);
	assert(config);

	if(!mesh->confbase) {
		return false;
	}

	char path[PATH_MAX];
	make_invitation_path(mesh, conf_subdir, name, path, sizeof(path));

	FILE *f = fopen(path, "w");

	if(!f) {
		logger(mesh, MESHLINK_ERROR, "Failed to open `%s': %s", path, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	if(!config_write_file(mesh, f, config, key)) {
		logger(mesh, MESHLINK_ERROR, "Failed to write `%s': %s", path, strerror(errno));
		fclose(f);
		return false;
	}

	if(fclose(f)) {
		logger(mesh, MESHLINK_ERROR, "Failed to close `%s': %s", path, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
		return false;
	}

	snprintf(path, sizeof(path), "%s" SLASH "%s" SLASH "invitations", mesh->confbase, conf_subdir);

	if(!sync_path(path)) {
		logger(mesh, MESHLINK_ERROR, "Failed to sync `%s': %s", path, strerror(errno));
		meshlink_errno = MESHLINK_ESTORAGE;
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

/// Purge invitations for the given node
size_t invitation_purge_node(meshlink_handle_t *mesh, const char *node_name) {
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

		if(snprintf(invname, sizeof(invname), "%s" SLASH "%s", path, ent->d_name) >= PATH_MAX) {
			logger(mesh, MESHLINK_DEBUG, "Filename too long: %s" SLASH "%s", path, ent->d_name);
			continue;
		}

		FILE *f = fopen(invname, "r");

		if(!f) {
			errno = 0;
			continue;
		}

		config_t config;

		if(!config_read_file(mesh, f, &config, mesh->config_key)) {
			logger(mesh, MESHLINK_ERROR, "Failed to read `%s': %s", invname, strerror(errno));
			config_free(&config);
			fclose(f);
			errno = 0;
			continue;
		}

		packmsg_input_t in = {config.buf, config.len};
		packmsg_get_uint32(&in); // skip version
		char *name = packmsg_get_str_dup(&in);

		if(name && !strcmp(name, node_name)) {
			logger(mesh, MESHLINK_DEBUG, "Removing invitation for %s", node_name);
			unlink(invname);
		}

		free(name);
		config_free(&config);
		fclose(f);
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
