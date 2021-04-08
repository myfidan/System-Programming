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

sig_atomic_t flag = 0;

void catcher(int signum) {
	switch (signum) {
		case SIGUSR1: puts("catcher caught SIGUSR1");
		flag++;
			break;
		case SIGUSR2: puts("catcher caught SIGUSR2");
			break;
		default: printf("catcher caught unexpected signal %d\n",
			signum); // bad idea to call printf in handler
	}
}

int main(){

	sigset_t sigset;
	sigfillset(&sigset);
	sigdelset(&sigset, SIGUSR1);
	//handler-signal
	struct sigaction sact;
	sigemptyset(&sact.sa_mask);
	sact.sa_flags = 0;
	sact.sa_handler = catcher;
	sigaction(SIGUSR1, &sact, NULL);

	sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);


	sigprocmask(SIG_BLOCK, &mask, NULL);

	pid_t t = fork();

	if(t == 0){
		printf("Child1\n");
		
		kill(getppid(),SIGUSR1);

		sleep(5);
		exit(0);
	}

	pid_t y = fork();

	if(y == 0){
		printf("Child2\n");
		
		kill(getppid(),SIGUSR1);

		sleep(5);
		exit(0);
	}
		
	printf("Parent\n");
	int i= 0;
	for(i = 0; i<2; i++){
		sigsuspend(&sigset);
		printf("arrive %d\n",i);
	}
	
	printf("After Sigsuspend\n");
	printf("%d\n",flag );
	return 0;
}