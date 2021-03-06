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
int money2;
sig_atomic_t flag = 0;
int endHire = 0;
int endQueue = 0;
int lowestMoney = 1000;
int lastStudents = 0;
int lastStudentsForHire = 0;
int checkLastStudents = 0;
//queue array
int queueSize = 0;
int frontIndex = 0;
int queueCap = 2;
char* queue;
char* homeworkType;
int* homeworkCountArray;
//Student for hire infos
int total_student_for_hire;
typedef struct{
	char name[50];
	int quality;
	int speed;
	int price;
	int available; // 1 available 0 not
	int sem_num;
}students;
students* students_for_hire;

//Semaphores for syncronization between threads
sem_t* semaphores;
sem_t main_thread_queue;
sem_t money_semaphore;
sem_t structure_semaphore;
sem_t busy_student_semaphore;

sem_t mainBarrier;
//Signal handler
void handler(int signal_number){
	++flag;
}

void check_student_for_hire_count(char* studentsFilePath);

//thread g
void* thread_h_fun(void* arg){
	char* filename = (char*) arg;
	int fd = open(filename, O_RDONLY); 
	if(fd == -1){
		printf("Open %s error\n",filename);
		exit(1);
	}
	//Read file to the que
	char chr;
	int checkFlag = 0;
	while(read(fd,&chr,1)){
		if(chr != '\n'){
			if(queueSize == queueCap){
				queue = (char*) realloc(queue,queueCap*2);
				queueCap *= 2;

			}
			queue[queueSize] = chr;
			queueSize++;
			if(sem_wait(&money_semaphore) == -1){
				if(errno == EINTR) break;
				printf("sem wait error\n");
				return NULL;
			}
			if(money < lowestMoney){
				printf("G has no more money for homeworks, terminating\n");	
				if(sem_post(&money_semaphore) == -1){
					printf("sem post error\n");
					exit(1);
				}
				if(sem_post(&main_thread_queue) == -1){
					printf("sem post error\n");
					exit(1);
				}
				checkFlag = 1;
				break;
			}
			printf("G has a new homework %c; remaining money is %dTL\n",chr,money );

			if(sem_post(&money_semaphore) == -1){
				printf("sem post error\n");
				exit(1);
			}
			if(sem_post(&main_thread_queue) == -1){
				printf("sem post error\n");
				exit(1);
			}
			
		}
	}

	if(checkFlag == 0) printf("G has no other homeworks, terminating.\n");
	endQueue = 1;
	if(sem_post(&main_thread_queue) == -1){
		printf("sem post error\n");
		exit(1);
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
	printf("%s is waiting for a homework\n",thread_struct_data->name );
	while(1){
		
		

		if(sem_wait(&semaphores[thread_struct_data->sem_num]) == -1){
			if(errno == EINTR) return NULL;
			printf("sem_wait error\n");
			return NULL;
		}
		if(flag > 0 || endHire > 0){ // if ctrl C signal arrive exit
			return NULL;
		}

		//BUNUDA CRITICAL SECTION YAP
		char homework_type = homeworkType[thread_struct_data->sem_num];
		//Critical section
		//change shared money
		if(sem_wait(&money_semaphore) == -1){
			if(errno == EINTR) return NULL;
			printf("sem_wait error\n");
			return NULL;
		}
		money -= students_for_hire[thread_struct_data->sem_num].price;
		printf("%s is solving homework %c for %d, G has %dTL left.\n", thread_struct_data->name,homework_type
			,thread_struct_data->price,money);
		//printf("%s %d %d %lf\n",thread_struct_data->name, thread_struct_data->quality, thread_struct_data->speed, thread_struct_data->price );
		if(sem_post(&money_semaphore) == -1){
			printf("sem post error\n");
			exit(1);
		}
		//Sleep here
		sleep(6-thread_struct_data->speed);
				//make not busy
		if(sem_wait(&structure_semaphore) == -1){
			if(errno == EINTR) return NULL;
			printf("sem wait error\n");
			return NULL;
		}
		homeworkCountArray[thread_struct_data->sem_num]++;
		thread_struct_data->available = 1;
		if(checkLastStudents != 1) printf("%s is waiting for a homework\n",thread_struct_data->name );
		if(checkLastStudents == 1){
			lastStudentsForHire += 1;
			if(lastStudentsForHire == lastStudents){
				if(sem_post(&mainBarrier) == -1){
					printf("sem post error\n");
					exit(1);
				}
			}
		}
		if(sem_post(&structure_semaphore) == -1){
			printf("sem post error\n");
			exit(1);
		}
		if(sem_post(&busy_student_semaphore) == -1){
			printf("sem post error\n");
			exit(1);
		}
	}

	return NULL;
}

int find_best_student(char type){
	//First check money 
	if(money2 < lowestMoney) return -1;

	if(sem_wait(&busy_student_semaphore) == -1){
		if(errno == EINTR) return flag;
		printf("sem wait errror\n");
		return flag;
	}

	

	int choice = -2;
	if(type == 'C'){ // Cost
		
			if(sem_wait(&structure_semaphore) == -1){
				if(errno == EINTR) return flag;
				printf("sem wait error\n");
				return flag;
			}
			int minC = 1000;
			for(int i=0; i<total_student_for_hire; i++){
				if(minC >= students_for_hire[i].price && students_for_hire[i].available == 1 && money2 >= students_for_hire[i].price){
					minC = students_for_hire[i].price;
					choice = i;
				}
			}
			if(sem_post(&structure_semaphore) == -1){
				printf("sem post error\n");
				exit(1);
			}
			if(choice == -2){
				return -1;
			}
		

		money2 -= students_for_hire[choice].price;
				
	}
	else if(type == 'Q'){// Quality
		
			if(sem_wait(&structure_semaphore) == -1){
				if(errno == EINTR) return flag;
				printf("sem wait error\n");
				return flag;
			}
		
			int maxQ = 0;
			
			for(int i=0; i<total_student_for_hire; i++){
				if(maxQ < students_for_hire[i].quality && students_for_hire[i].available == 1 && money2 >= students_for_hire[i].price){
					maxQ = students_for_hire[i].quality;
					choice = i;
				}
			}
			
			if(sem_post(&structure_semaphore) == -1){
				printf("sem post error\n");
				exit(1);
			}
			if(choice == -2){
				return -1;
			}
		
		money2 -= students_for_hire[choice].price;
		
	}
	else{ //S Speed
			if(sem_wait(&structure_semaphore) == -1){
				if(errno == EINTR) return flag;
				printf("sem wait error\n");
				return flag;
			}
			
			int maxS = 0;
			
			for(int i=0; i<total_student_for_hire; i++){
				if(maxS < students_for_hire[i].speed && students_for_hire[i].available == 1 && money2 >= students_for_hire[i].price){
					maxS = students_for_hire[i].speed;
					choice = i;
				}
			}
			if(sem_post(&structure_semaphore) == -1){
				printf("sem post error\n");
				exit(1);
			}
			if(choice == -2){
				return -1;
			}
		
		money2 -= students_for_hire[choice].price;	
	}
	
	
	return choice;
}

int main(int argc, char* argv[]){
	
	//Vars
	char* homeworkFilePath;
	char* studentsFilePath;

	//thread g
	pthread_t thread_h;
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
   	money2=money;
   	//take student for hire infos
   	check_student_for_hire_count(studentsFilePath);
   	//Print students attributes first
   	printf("%d students-for-hire threads have been created.\n",total_student_for_hire );
   	printf("Name Q S C\n");
   	for (int i = 0; i < total_student_for_hire; ++i)
   	{
   		printf("%s %d %d %d\n",students_for_hire[i].name,students_for_hire[i].quality,students_for_hire[i].speed,students_for_hire[i].price );
   	}
   	//create type homework array for students for hire
   	homeworkType = (char*)malloc(total_student_for_hire * sizeof(char));
   	//create array that hold whick student do how many homework
   	homeworkCountArray = (int*)malloc(total_student_for_hire * sizeof(int));
   	for(int i=0; i<total_student_for_hire; i++){
   		homeworkCountArray[i] = 0;
   	}
   	//Create and init semaphores
   	if(sem_init(&main_thread_queue,0,0) == -1){
   		printf("Error while init semaphores\n");
   		exit(1);
   	}
   	if(sem_init(&money_semaphore,0,1) == -1){
   		printf("Error while init semaphores\n");
   		exit(1);
   	}
   	if(sem_init(&structure_semaphore,0,1) == -1){
   		printf("Error while init semaphores\n");
   		exit(1);
   	}
    if(sem_init(&busy_student_semaphore,0,total_student_for_hire) == -1){
   		printf("Error while init semaphores\n");
   		exit(1);
    }
   	
   	if(sem_init(&mainBarrier,0,0) == -1){
   		printf("Error while init semaphores\n");
   		exit(1);
   	}
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
   	s = pthread_create(&thread_h,&attr,thread_h_fun,(void *) homeworkFilePath);
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
   		
   	}
   	
   	
   	//sem_post(&semaphores[1]);
   	
   	//Main thread assign queue jobs for other threads
   	//If queue or money end break loop
   	int k=0;
   	int endResult = 0;
   	int best_choice;
   	int ctrlC = 0;
   	while(1){
   		sem_wait(&main_thread_queue);
   		if(flag > 0){
   			ctrlC = 1;
   			break; //CTRL C SIGNAL ARRIVED EXIT
   		}
   		int val;
   		if(sem_getvalue(&main_thread_queue,&val) == -1){
   			printf("sem_getvalue error\n");
   			exit(1);
   		}
   		if(endQueue > 0 && val == 0) {
   			endResult = 1;
   			break;
   		}
   		// Burdan sonra uygun student?? bul
   		// busy bitini g??ncelle 
   		// array homework type ??n?? g??nncelle
   		// sonra onun semaphorunu postla
   		// ve devam et money bitene kadar veya que bitne kadar
   		
   		best_choice = find_best_student(queue[k]);
   		if(flag > 0){
   			ctrlC = 1;
   			break; //CTRL C SIGNAL ARRIVED EXIT
   		}
   		if(best_choice == -1) break;
   		homeworkType[best_choice] = queue[k];

   		students_for_hire[best_choice].available = 0;
   		if(sem_post(&semaphores[best_choice]) == -1){
   			printf("sem post error\n");
   			exit(1);
   		}

   		k++;
   	}

   	if(sem_wait(&structure_semaphore) == -1){
   		printf("sem wait error\n");
   		exit(1);
   	}
   	checkLastStudents = 1;
   	for(int i=0; i<total_student_for_hire; i++){
   		if(students_for_hire[i].available == 0) lastStudents++;
   	}
   	if(sem_post(&structure_semaphore) == -1){
   		printf("sem post error\n");
   		exit(1);
   	}

   	if(lastStudents != 0){
   		//semwait
   		if(sem_wait(&mainBarrier) == -1){
   			printf("sem wait error\n");
   			exit(1);
   		}
   	}
   	//sleep(10); //??imdilik
   	
   	if(endResult == 1){
   		printf("No more Homeworks left or coming in, closing.\n");
   	}
   	else if(ctrlC == 1){
   		printf("Terminating signal receiver, closing.\n");
   	}
   	else if(best_choice == -1){
   		printf("Money is over, closing.\n");
   	}
   	//Buraya geldi??imde t??m studentslar waitlerin bekliyor
   	//veya money d??????rme i??lemini yapm???? olmalar?? laz??m
   	//Yukar??daki sleep ??imdilik bunu sa??l??yor ama sleepsiz olmal??
   	// END students for hire
   	endHire = 1;
   	for (int i = 0; i < total_student_for_hire; ++i)
   	{
   		if(sem_post(&semaphores[i]) == -1){
   			printf("sem post error\n");
   			exit(1);
   		}
   	}
   	

   	//Wait student hire threads end
   	for(int i=0; i < total_student_for_hire; i++){
   		pthread_join(student_hire_threads[i],NULL);
   	}
   	printf("Homeworks solved and money made by the students:\n");
   	int total_homework = 0;
   	double total_money = 0;
   	for(int i=0; i<total_student_for_hire; i++){
   		printf("%s %d %d\n",students_for_hire[i].name,homeworkCountArray[i], homeworkCountArray[i]*students_for_hire[i].price);
   		total_homework += homeworkCountArray[i];
   		total_money += homeworkCountArray[i]*students_for_hire[i].price;
   	}
   	printf("Total cost for %d homeworks %.1lfTL\n",total_homework,total_money);
   	printf("Money left at G's account: %dTL\n",money);
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
   	free(homeworkType);
   	free(homeworkCountArray);
   	if(sem_destroy(&main_thread_queue) == -1){
		printf("Error in semaphore destroy\n");
		exit(1);
	}
   	if(sem_destroy(&money_semaphore) == -1){
   		printf("Error in semaphore destroy\n");
   		exit(1);
   	}
   	if(sem_destroy(&structure_semaphore) == -1){
   		printf("Error in semaphore destroy\n");
   		exit(1);
   	}
   	if(sem_destroy(&mainBarrier) == -1){
   		printf("Error in semaphore destroy\n");
   		exit(1);
   	}
   	
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
   			students_for_hire[structIndex].price = atoi(token);
   			students_for_hire[structIndex].available = 1;
   			students_for_hire[structIndex].sem_num = structIndex;
   			if(students_for_hire[structIndex].price < lowestMoney) lowestMoney = students_for_hire[structIndex].price;
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