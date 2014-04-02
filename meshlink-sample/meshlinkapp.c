#include <libmeshlink.h>

int main(int argc , char **argv){

char *tinc_conf = "/tmp/";
char *name = "test";

tinc_setup(tinc_conf, name);

return 0;
}

