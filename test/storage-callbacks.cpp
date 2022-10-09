#ifdef NDEBUG
#undef NDEBUG
#endif

#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "meshlink++.h"
#include "utils.h"

#include "devtools.h"

std::map<std::string, std::map<std::string, std::vector<char>>> storage;

static bool load_cb(meshlink_handle_t *mesh, const char *name, void *buf, size_t *len) {
	const auto& data = storage[mesh->name][name];
	if (data.empty())
		return false;

	char* begin = static_cast<char *>(buf);
	auto todo = std::min(data.size(), *len);
	*len = data.size();
	std::copy_n(data.begin(), todo, begin);
	return true;
}

static bool store_cb(meshlink_handle_t *mesh, const char *name, const void *buf, size_t len) {
	auto& data = storage[mesh->name][name];
	const char* begin = static_cast<const char *>(buf);
	data.assign(begin, begin + len);
	return true;
}

static bool ls_cb(meshlink_handle_t *mesh, meshlink_ls_entry_cb_t entry_cb) {
	for (auto& it: storage[mesh->name]) {
		auto& name = it.first;
		auto& data = it.second;
		if (data.empty()) {
			continue;
		}
		if (!entry_cb(mesh, name.c_str(), data.size())) {
			return false;
		}
	}

	return true;
}

int main() {
	meshlink::set_log_cb(MESHLINK_DEBUG, log_cb);

	meshlink::open_params params1("storage-callbacks_conf.1", "foo", "storage-callbacks", DEV_CLASS_BACKBONE);
	meshlink::open_params params2("storage-callbacks_conf.2", "bar", "storage-callbacks", DEV_CLASS_BACKBONE);
	meshlink::open_params params3("storage-callbacks_conf.3", "baz", "storage-callbacks", DEV_CLASS_BACKBONE);

	params1.set_storage_callbacks(load_cb, store_cb, ls_cb);
	params1.set_storage_key("hunter42", 8);
	params2.set_storage_callbacks(load_cb, store_cb, ls_cb);
	params3.set_storage_callbacks(load_cb, store_cb, ls_cb);

	// Start nodes and let foo invite bar
	{
		meshlink::mesh mesh1(params1);
		meshlink::mesh mesh2(params2);

		mesh1.enable_discovery(false);
		mesh2.enable_discovery(false);

		char *invitation = mesh1.invite({}, "bar", 0);
		assert(invitation);
		assert(mesh1.start());

		assert(mesh2.join(invitation));
		free(invitation);
	}


	// Start the nodes again and check that they know each other
	{
		meshlink::mesh mesh1(params1);
		meshlink::mesh mesh2(params2);

		mesh1.enable_discovery(false);
		mesh2.enable_discovery(false);

		assert(mesh1.start());
		assert(mesh2.start());

		assert(mesh1.get_node("bar"));
		assert(mesh2.get_node("foo"));
	}

	// Test key rotation
	{
		meshlink::mesh mesh1(params1);
		meshlink::mesh mesh3(params3);

		mesh1.enable_discovery(false);
		mesh3.enable_discovery(false);

		char *invitation = mesh1.invite({}, "baz", 0);
		assert(invitation);

		devtool_keyrotate_probe = [](int stage){ return stage != 1; };
		assert(!mesh1.encrypted_key_rotate("newkey", 6));
		mesh1.close();

		params1.set_storage_key("newkey", 6);
		assert(!mesh1.open(params1));
		params1.set_storage_key("hunter42", 8);
		assert(mesh1.open(params1));

		devtool_keyrotate_probe = [](int stage){ return stage != 2; };
		assert(mesh1.encrypted_key_rotate("newkey", 6));
		mesh1.close();
		params1.set_storage_key("newkey", 6);
		assert(mesh1.open(params1));

		assert(mesh1.start());
		assert(mesh3.join(invitation));
		assert(mesh1.get_node("baz"));
		assert(mesh3.get_node("foo"));
	}
}
