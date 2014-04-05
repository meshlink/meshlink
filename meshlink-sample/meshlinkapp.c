#include <libmeshlink.h>

int main(int argc , char **argv){

char *confbase = "/tmp/meshlink/";
char *name = "test";

tinc_setup(confbase, name);
tinc_start(confbase);
sleep(10); //give time to this thread to finish before we exit
return 0;
}

