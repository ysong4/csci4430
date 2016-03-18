#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>

main(){
	struct stat st = {0};
	
	if (stat("./proxyFiles", &st) == -1){
		mkdir("./proxyFiles", 0777);
	}
}