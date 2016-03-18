#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <crypt.h>
#include <stdlib.h>

main(int argc, char * argv[]){

	char filename[23];
	int i;
	strncpy(filename, crypt(argv[1], "$1$00$")+6, 23);
	for (i=0; i<22; ++i){
		if (filename[i] == '/'){
			filename[i] = '_';
		}
	}

	struct stat st = {0};

	if (stat("./proxyFiles", &st) == -1){
		mkdir("./proxyFiles", 0777);
	}


	char partOne[] = "./proxyFiles/";
	char * dirName;
	dirName = malloc((strlen(partOne)+ strlen(filename))* sizeof(char));
	dirName = strcat(partOne, filename);


	if (stat(dirName, &st) == -1){
		mkdir(dirName, 0777);
	}

	printf("%s\n", dirName);
}