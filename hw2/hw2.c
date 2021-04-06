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

//macro for be ensure reading not interrupted by a signal
#define NO_EINTR(stmt) while ((stmt) == -1 && errno == EINTR);
#define BUF_SIZE 1024

void take_first_6_point(char* buffer,double* x_first_6,double* y_first_6);
double calculate_lagrande(double* x_first_6,double* y_first_6, int numberCount, int nextPoint);
void create_new_row(char* buffer,double res_6);

int main(int argc,char* argv[]){

	if(argc != 2){
		printf("Error argument\n");
		exit(1);
	}

	pid_t child_process[8];
	char* filepath = argv[1];
	
	int fd = open(filepath,O_RDWR | O_SYNC | O_APPEND);
	if(fd == -1){
		perror("open file error");
		exit(1);
	}

	
	//Create 8 process
	int i = 0;
	for(i=0; i<8; i++){

		switch(child_process[i] = fork()){
			case -1:
				perror("error in fork");
				exit(1);
				break;
			case 0: ;
				struct flock lock;
				memset(&lock,0,sizeof(lock));
				char buffer[BUF_SIZE];
				buffer[0] = '\0';

				int cnt;
				lock.l_type = F_WRLCK;
				//lock file 
				if(fcntl(fd,F_SETLKW,&lock) == -1){
					perror("fcntl error");
					exit(1);
				}
				lseek(fd,0,SEEK_SET);
				//NO_EINTR(cnt = read(fd,buffer,BUF_SIZE))
				

				char c[2];
				int count=1;
				char chr;
				//read(fd,buffer,BUF_SIZE);
				//Each prosess read corresponding row
				int k = 0;
				char beforeRow[BUF_SIZE];
				beforeRow[0] = '\0';
				char afterRow[BUF_SIZE];
				afterRow[0] = '\0';
				while((count = read(fd,&chr,sizeof(chr)))){
					if(k>i){
						strncat(afterRow,&chr,1);
						int count2 = 1;
						while((count2 = read(fd,&chr,sizeof(chr)))){
							strncat(afterRow,&chr,1);
						}
						break;
					}
					if (k == i){
						strncat(buffer,&chr,1);
					}
					else{
						strncat(beforeRow,&chr,1);
					}	
					if(chr == '\n') k++;
					//printf("%c",chr);
				}
				//printf("%s",buffer );
				char* useStrForToken = calloc(strlen(buffer)+1, sizeof(char));
				strcpy(useStrForToken, buffer);


				char* strNewRow = calloc(strlen(buffer)+100, sizeof(char));
				strcpy(strNewRow, buffer);


				
			   	
			   	//Arrays for first 6 coordinate
			   	double x_first_6[6];
			   	double y_first_6[6];
			   	//Take 6 point
			   	take_first_6_point(useStrForToken,x_first_6,y_first_6);
			   	double res_6 = calculate_lagrande(x_first_6,y_first_6,6,7);
			    create_new_row(strNewRow,res_6);
			    

			    char * newWrite = calloc(1024, sizeof(char));
			    strcat(newWrite,beforeRow);
			    strcat(newWrite,strNewRow);
			    strcat(newWrite,afterRow);
			    //printf("%s\n",newWrite );
			    //printf("%s",beforeRow);
			    //printf("%s",strNewRow );
			    //printf("%s\n",afterRow);
			    //fflush(stdout);
			    /*
			    printf("Calculated points:\n");
			    for(int j=0; j<6; j++){
			    	printf("(%.1lf,%.1lf) ",x_first_6[j],y_first_6[j] );
			    }
			    printf("\n");
			    printf("Next Point %.1lf\n", res_6);
				*/

				lseek(fd,0,0);
				
				
				ftruncate(fd, 0);
				write(fd,newWrite,strlen(newWrite));
				
				lock.l_type = F_UNLCK;
				
				if(fcntl(fd,F_SETLK,&lock) == -1){
					perror("fcntl error");
					exit(1);
				}
				

				free(newWrite);
				free(strNewRow);
				free(useStrForToken);

				close(fd);
				_exit(EXIT_SUCCESS);

			default:

				break;
		}
	}

	close(fd);


	return 0;
}


//push first 6 x and first 6 y coordinat to xfirst_6 and yfirst_6 arrays
//Then I will use this 6 points arrays for calculating legrange polynomial
void take_first_6_point(char* buffer,double* x_first_6,double* y_first_6){
	//tokineze
	const char s[2] = ",";
    char *token;
    /* get the first token */
    token = strtok(buffer, s);
   
    /* walk through other tokens */
    
    int x_index = 0;
    int y_index = 0;
    int j = 0;
    while( token != NULL ) {
    	double d;
    	sscanf(token, "%lf", &d);
    	if(j % 2 == 0){ //x point
    		if(x_index < 6){

	    		x_first_6[x_index] = d;
	    		x_index++;	
    		} 
    	}
    	else{ //y point
    		if(y_index < 6){

    			y_first_6[y_index] = d;
    			y_index++;	
    		}
    	}
    	j++;
    	token = strtok(NULL, s);
    }
}

//Calculate lagrande by using wikipedia formula for lagrande
double calculate_lagrande(double* x_first_6,double* y_first_6, int numberCount, int nextPoint){
	double res;
	double finalResult = 0;
	for(int i=0; i<numberCount; i++){
		res = 1;
		for(int j=0; j<numberCount; j++){
			if(i != j){
				res = res * (double)(nextPoint - x_first_6[j]) / (double)(x_first_6[i] - x_first_6[j]);
			}
		}
		finalResult += res * y_first_6[i];
	}

	return finalResult;
}

//Combine row with new finded lagrande value
void create_new_row(char* buffer,double res_6){
	//remove last \n
	buffer[strlen(buffer) - 1] = '\0';
	//Add comma
	char comma = ',';
	strncat(buffer,&comma,1);
	//add new value
	char d[15];
	sprintf(d,"%.1lf", res_6);
	strcat(buffer,d);
	//add \n again
	char newLine = '\n';
	strncat(buffer,&newLine,1);

}

