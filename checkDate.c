#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h> // for struct tm
#include <unistd.h> //for getcwd
#include <stdbool.h> // for boolean

bool compareName(char* fileName, char* inDate){
	struct tm *tm;
	struct stat st = {0};
	char datestring[256];

	stat(fileName, &st);
	printf("%s\n", fileName);

	tm = gmtime(&st.st_mtime);
	strftime(datestring, sizeof(datestring), "%a, %d %b %Y %X %Z", tm);	

	printf("%s\n",datestring);

	if (strcmp(datestring,inDate) == 0){
		return true;
	}
	else
		return false;
}

int main(int argc, char* argv[]){
	if (compareName(argv[1], argv[2])){
		printf("true\n");
	}
	else
		printf("false\n");
}