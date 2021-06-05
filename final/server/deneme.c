#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <dirent.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <sys/wait.h>
#include <math.h>
#include <getopt.h>

#include <semaphore.h>
#include <sys/mman.h>
#include <pthread.h>

struct Node {
    char** data;
    struct Node* next;
};

struct Node* head = NULL;

int main(){
 	
   	char** fr;
   	const char s[2] = ",";
    char *token;

	char * line = NULL;
    size_t len = 0;
    ssize_t read;
	FILE *fptr;
	fptr = fopen("data.csv","r");
	char* firstRow = NULL;
	getline(&firstRow, &len, fptr);
	int totalColm = 0;
	for(int i=0; firstRow[i] != '\0'; i++){
		if(firstRow[i] == ',') totalColm++;
	}
	totalColm++;
	fr = (char**)malloc(sizeof(char*)*totalColm);
	printf("%s\n",firstRow );
	token=strtok(firstRow,s);
	int index = 0;
	printf("*\n");
	while(token != NULL){
		if(token[strlen(token)-1] == '\n') token[strlen(token)-1] = '\0';
		fr[index] = (char*)malloc(sizeof(char)*255);
		strcpy(fr[index],token);
		token = strtok(NULL,s);
		index++;
	}

	int cap = 2;
	char*** a;
	a = (char***)malloc(sizeof(char**)*cap);
	int size = 0;
	while((read = getline(&line,&len,fptr))!=-1){
		if(size == cap){
			a = (char***)realloc(a,sizeof(char**)*cap*2);
			cap *= 2;
		}
		a[size] = (char**)malloc(sizeof(char*)*totalColm);
		int z=0;
		for(int k=0; k<totalColm; k++){
			a[size][k] = (char*)malloc(sizeof(char)*255);
			a[size][k][0] = '\0';
			int doubleq = 0;
			while(line[z] != '\0'){
				if(line[z] == ',' && doubleq == 0){
					z++;
					break;
				}
				if(line[z] == '"'){
					if(doubleq == 0){
						doubleq = 1;
					}
					else{
						doubleq = 0;
					}
				}
				strncat(a[size][k],&line[z],1);
				z++;
			}
			
			//strcpy(a[size][k],line);
			//a[size][k] = line;
		}
		size++;
	}
	/*
	for(int i = 0; i < size; ++i){
        for(int j = 0; j < totalColm; ++j){
            printf("%s\n", a[i][j]);
        }
    }
    */
    for(int i=0; i<totalColm; i++){
    	printf("%s\n",fr[i]);
    }
    for(int i=0; i<totalColm; i++){
    	free(fr[i]);
    }
    free(fr);
    for(int i=0;i<size; i++){
    	for(int j=0; j<totalColm; j++){
	    	free(a[i][j]);
	    }
	    free(a[i]);
    }
    free(a);
    
	/*
	char **arr = (char**) malloc(sizeof(char*)*cap);

	int size=0;

	while((read = getline(&line, &len, fptr)) != -1) {
		if(size == cap){
			arr = (char**)realloc(arr,cap*2*sizeof(char*));
			cap *= 2;
		}
		arr[size] = (char*) malloc(sizeof(char)*1024);
        strcat(arr[size],line);
        size++;
    }
	
	//back
	//here
	for(int i=0; i<size; i++){
		printf("%s\n",arr[i] );
	}
	for (int i = 0; i < size; i++ )
	{
	    free(arr[i]);
	}

	free(arr);
	*/
    fclose(fptr);
	 if (line)
	    free(line);
    free(firstRow);
	return 0;
	
}