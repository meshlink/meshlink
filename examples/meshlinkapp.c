#include "../src/logger.h"
#include "../src/system.h"
#include "../src/meshlink.h"

void handle_recv_data(meshlink_handle_t *mesh, meshlink_node_t *source, void *data, size_t len) {
	(void)mesh;

	printf("Received %zu bytes from %s: %s\n", len, source->name, (char *)data);
}

int main(int argc, char **argv) {
	char *confbase = argc > 1 ? argv[1] : "/tmp/meshlink/";
	char *name = argc > 2 ? argv[2] : "foo";

	char *remotename = argc > 3 ? argv[3] : "bar";

	meshlink_handle_t *myhandle;

	myhandle = meshlink_open(confbase, name, "meshlinkapp", DEV_CLASS_STATIONARY);

	if(!myhandle) {
		fprintf(stderr, "Could not open MeshLink: %s", meshlink_strerror(meshlink_errno));
		return 1;
	}

	//Register callback function for incoming data
	meshlink_set_receive_cb(myhandle, (meshlink_receive_cb_t)handle_recv_data);

	if(!meshlink_start(myhandle)) {
		fprintf(stderr, "Could not start MeshLink: %s", meshlink_strerror(meshlink_errno));
		return 1;
	}

	while(1) {
		sleep(10);

		meshlink_node_t *remotenode = meshlink_get_node(myhandle, remotename);

		if(!remotenode) {
			fprintf(stderr, "Node %s not known yet.\n", remotename);
			continue;
		}

		//sample data to send out
		char mydata[200];
		memset(mydata, 0, 200);
		strcpy(mydata, "Hello World!");

		//send out data
		if(!meshlink_send(myhandle, remotenode, mydata, sizeof(mydata))) {
			fprintf(stderr, "Error sending data: %s", meshlink_strerror(meshlink_errno));
			return 1;
		}
	}

	meshlink_stop(myhandle);
	meshlink_close(myhandle);

	return 0;
}

