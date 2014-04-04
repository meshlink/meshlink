#include <libmeshlink.h>

int main(int argc , char **argv){

char *confbase = "/tmp/meshlink/";
char *name = "test";

tinc_setup(confbase, name);

return 0;
}

