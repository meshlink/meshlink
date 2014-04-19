#include <libmeshlink.h>


void handle_recv_data(void *data);
void handle_recv_data(void *data) {
printf("Data received is %s\n",data);

}

int main(int argc , char **argv){

char *confbase = argc > 1 ? argv[1] : "/tmp/meshlink/";
char *name = argc > 2 ? argv[2] : "foo";
debug_level = 5;

node_t* remotenode = new_node();
char *remotename = argc > 3 ? argv[3] : "bar";

//TODO: change this, calling a function that returns node_t
remotenode->name = malloc(16);
remotenode->name = remotename;

tinc_setup(confbase, name);
tinc_start(confbase);

//Register callback function for incoming data
recvdata_register_cb(handle_recv_data);

sleep(2); //there is a race condition here, tinc_start detaches to a thread the needs time to setup stuff
while(1) {

//sample data to send out
char mydata[200];
memset(mydata,0,200);
strcpy(mydata,"Hello World!");

//send out data
tinc_send_packet(remotenode,mydata,sizeof(mydata));
sleep(10); //give time to this thread to finish before we exit
}
free(remotenode);
return 0;
}

