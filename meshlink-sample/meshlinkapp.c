#include <libmeshlink.h>

int main(int argc , char **argv){

char *confbase = "/tmp/meshlink/";
char *name = "test";
debug_level = 5;

node_t* remotenode = new_node();
char *remotename = "ml";

//TODO: change this, calling a function that returns node_t
remotenode->name = malloc(16);
remotenode->name = remotename;

tinc_setup(confbase, name);
tinc_start(confbase);
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

