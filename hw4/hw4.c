#define _GNU_SOURCE
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

//Global money and flag for ctrl c signal
int money;
sig_atomic_t flag = 0;
int endHire = 0;
//queue array
int queueSize = 0;
int frontIndex = 0;
int queueCap = 2;
char* queue;

//Student for hire infos
int total_student_for_hire;
typedef struct{
	char name[50];
	int quality;
	int speed;
	double price;
	int available; // 1 available 0 not
	int sem_num;
}students;
students* students_for_hire;

//Semaphores for syncronization between threads
sem_t* semaphores;
//Signal handler
void handler(int signal_number){
	++flag;
}

void check_student_for_hire_count(char* studentsFilePath);

//thread g
void* thread_g_fun(void* arg){
	char* filename = (char*) arg;
	int fd = open(filename, O_RDONLY); 
	if(fd == -1){
		printf("Open %s error\n",filename);
		exit(1);
	}
	//Read file to the que
	char chr;
	while(read(fd,&chr,1)){
		if(chr != '\n'){
			if(queueSize == queueCap){
				queue = (char*) realloc(queue,queueCap*2);
				queueCap *= 2;

			}
			queue[queueSize] = chr;
			queueSize++;
			printf("G has a new homework %c; remaining money is %dTL\n",chr,money );
			
		}
	}

	if(close(fd) == -1){
		perror("Close error..");
		exit(1);
	}
	//printf("Hello from thread g %s\n",queue);
	return NULL;
}

//Student for hire thread function
void* student_hire_func(void* arg){
	students* thread_struct_data = (students*) arg;
	//printf("%u\n",(unsigned int)pthread_self());
	
	while(1){

		if(sem_wait(&semaphores[thread_struct_data->sem_num]) == -1){
			printf("sem_wait error\n");
			exit(1);
		}
		if(flag > 0 || endHire > 0){ // if ctrl C signal arrive exit
			return NULL;
		}
		printf("%s %d %d %lf\n",thread_struct_data->name, thread_struct_data->quality, thread_struct_data->speed, thread_struct_data->price );
		
	}

	return NULL;
}

int main(int argc, char* argv[]){
	
	//Vars
	char* homeworkFilePath;
	char* studentsFilePath;

	//thread g
	pthread_t thread_g;
	pthread_attr_t attr;
	int s;
	//Handler for ctrl + c
	struct sigaction sa;
    memset(&sa,0,sizeof(sa));
    sa.sa_handler = &handler;
    sigaction(SIGINT,&sa,NULL);

    //Read arguments
   	if(argc != 4){
   		printf("Argument error..\n");
   		exit(1);	
   	} 

   	homeworkFilePath = argv[1];
   	studentsFilePath = argv[2];
   	money = atoi(argv[3]);
   	//take student for hire infos
   	check_student_for_hire_count(studentsFilePath);
   	//Create semaphores
   	semaphores = (sem_t*)malloc(sizeof(sem_t) * total_student_for_hire);
   	for (int i = 0; i < total_student_for_hire; ++i)
   	{
   		if(sem_init(&semaphores[i],0,0) == -1){
   			printf("Error while init semaphores\n");
   			exit(1);
   		}
   	}
   	//init queue
   	queue = (char*)malloc(queueCap*sizeof(char));
   	//Make thread g detached
   	s = pthread_attr_init(&attr);
   	if(s != 0){
   		printf("pthread_attr_init error\n");
   		exit(1);
   	}
   	s = pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED); // Detached thread attribute
   	if(s != 0){
   		printf("pthread_attr_setdetachstate error\n");
   		exit(1);
   	}
   	//create detached thread g
   	s = pthread_create(&thread_g,&attr,thread_g_fun,(void *) homeworkFilePath);
   	if(s != 0){
   		printf("pthread_create\n");
   		exit(1);
   	}
   	//Destroy attr dont need anymore
   	s = pthread_attr_destroy(&attr);
   	if(s != 0){
   		printf("pthread_attr_destory\n");
   		exit(1);
   	}
   	//main thread creates students for hire threads
   	pthread_t student_hire_threads[total_student_for_hire];
   	int check_err;
   	for (int i = 0; i < total_student_for_hire; ++i)
   	{	
   		
   		check_err = pthread_create(&student_hire_threads[i],NULL,student_hire_func,(void *)&students_for_hire[i]);
   		if(check_err != 0){
   			printf("Error in thread creation\n");
   			exit(1);
   		}
   		//printf("%u\n",student_hire_threads[i] );
   		char buff[100];
   		buff[0] = '\0';
   		snprintf(buff,100,"%u",(unsigned int)student_hire_threads[i]);
		printf("%s\n",buff );   		
   	}
   	
   	
   	sem_post(&semaphores[1]);
   	
   	sleep(2); //şimdilik
   

   	for(int i=0; i<queueSize; i++){
   		printf("%c",queue[i] );
   	}
   	// END students for hire
   	endHire = 1;
   	for (int i = 0; i < total_student_for_hire; ++i)
   	{
   		sem_post(&semaphores[i]);
   	}
   	printf("\n");

   	//Wait student hire threads end
   	for(int i=0; i < total_student_for_hire; i++){
   		pthread_join(student_hire_threads[i],NULL);
   	}
   	printf("After end of total_student_for_hire\n");
   	//Destroy all semaphores before exit and free resources
   	free(queue);
   	free(students_for_hire);
   	for (int i = 0; i < total_student_for_hire; ++i)
   	{	
   		if(sem_destroy(&semaphores[i]) == -1){
   			printf("Error in semaphore destroy\n");
   			exit(1);
   		}
   	}
   	free(semaphores);
   	
   	if(flag > 0){
   		printf("Terminating signal receiver, closing.\n");
   		exit(0);
   	}
   	printf("Main ends\n");
	return 0;
}

void check_student_for_hire_count(char* studentsFilePath){
	FILE* fd;
	char* line= NULL;
	size_t len = 0;
	ssize_t read;
	fd = fopen(studentsFilePath,"r");
	if(fd == NULL){
		printf("Open %s error\n",studentsFilePath);
		exit(1);
	}
	int hire_count = 0;
	while((read = getline(&line,&len,fd)) != -1){
		if(line[0] != '\n'){
			hire_count++;
		}
	}

	if(fclose(fd) != 0){
		printf("Error close file\n");
		exit(1);
	}
	free(line);
	//create dynamic struct
	total_student_for_hire = hire_count;
	students_for_hire = (students*) malloc(sizeof(students) * hire_count);
	
	line = NULL;
	len = 0;
	int structIndex = 0;
	fd = fopen(studentsFilePath,"r");
	if(fd == NULL){
		printf("Open %s error\n",studentsFilePath);
		exit(1);
	}
	while((read = getline(&line,&len,fd)) != -1){
		
		if(line[0] != '\n'){
			const char s[2] = " ";
   			char *token;
   			//name
   			token = strtok(line, s);
   			strcpy(students_for_hire[structIndex].name,token);
   			//quality
   			token = strtok(NULL, s);
   			students_for_hire[structIndex].quality = atoi(token);
   			//speed
   			token = strtok(NULL, s);
   			students_for_hire[structIndex].speed = atoi(token);
			//price
			token = strtok(NULL, s);
			double d;
			sscanf(token,"%lf",&d);
   			students_for_hire[structIndex].price = d;
   			students_for_hire[structIndex].available = 1;
   			students_for_hire[structIndex].sem_num = structIndex;
			structIndex++;
		}
	}

	if(fclose(fd) != 0){
		printf("Error close file\n");
		exit(1);
	}
	free(line);
		/*
	for (int i = 0; i < hire_count; ++i)
	{
		printf("%s %d %d %lf\n",students_for_hire[i].name, students_for_hire[i].quality, students_for_hire[i].speed, students_for_hire[i].price );
	}
	*/
}