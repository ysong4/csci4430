#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h> // for struct tm
#include <unistd.h> //for getcwd
#include <stdbool.h> // for boolean

bool compareName(char* fileName, char* inDate){
	struct tm *tm;
	struct tm tm1;
	struct tm tm2;
	time_t time1;
	time_t time2;
	double seconds;
	struct stat st = {0};
	char datestring[256];

	stat(fileName, &st);

	tm = gmtime(&st.st_mtime);
	strftime(datestring, sizeof(datestring), "%a, %d %b %Y %X %Z", tm);	

	printf("%s\n",datestring);
	tm1 = *tm;
	strptime(inDate, "%a, %d %b %Y %X %Z", &tm2);
	printf("%s\n", inDate);

	time1 = mktime(&tm1);
	time2 = mktime(&tm2);

	seconds = difftime(time1, time2);

	printf("%f\n", seconds);

	if (seconds >= 0){
		printf("200\n");
	}
	else
		printf("304\n");
}

int main(int argc, char* argv[]){
	compareName(argv[1], argv[2]);
}