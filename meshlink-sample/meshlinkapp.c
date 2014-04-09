#include <libmeshlink.h>

int main(int argc , char **argv){

char *confbase = "/tmp/meshlink/";
char *name = "test";

tinc_setup(confbase, name);
tinc_start(confbase);
while(1) {
sleep(10); //give time to this thread to finish before we exit
tinc_send_packet(NULL,"datafgsdfsd",10);
}
return 0;
}

