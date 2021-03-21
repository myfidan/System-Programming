#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

int main(int argc, char *argv[]){

	int opt;

	//declare all bool flags false
	//fbtplw
	int flags_bool[6];
	for(int i=0; i<6; i++){
		flags_bool[i] = 0;
	} 
	
	char* target_directory; //w
	char* filename; //f
	int file_size; //b
	char file_type; //t
	char* permissions; //p
	int link_number; //l

	extern char *optarg;
	extern int optind, optopt, opterr;
 	while((opt = getopt(argc, argv, "f:b:t:p:l:w:")) != -1)  
    {  
        switch(opt)  
        {  
            case 'f': 
            	flags_bool[0] = 1;
            	filename = optarg;
            	printf("option: %c\n", opt);
                break; 
            case 'b':  
            	flags_bool[1] = 1;
            	file_size = atoi(optarg);
            	printf("option: %c\n", opt); 
                break;
            case 't':
            	flags_bool[2] = 1; 
            	file_type = optarg[0];
                printf("option: %c\n", opt);
                break;
            case 'p':
            	flags_bool[3] = 1; 
            	permissions = optarg; 
                printf("option: %c\n", opt);
                break;  
            case 'l':
            	flags_bool[4] = 1; 
            	link_number = atoi(optarg); 
                printf("option: %c\n", opt);
                break;
			case 'w':
				flags_bool[5] = 1; 
				target_directory = optarg;
                printf("option: %c\n", opt);
                break; 
            default:
            	printf("ERROR opt\n"); 
            	return(-1); 
        }  
    }

    //Check there is w argument or not
    if(flags_bool[5] == 0){
    	printf("ERROR, No w argument, its mandatory.\n");
    	return (-1);
    }
    
    //Check there is at least 1 fbtpl argument
    int flag_count = 0;
    for (int i = 0; i < 5; ++i){
    	if(flags_bool[i] == 1){
    		flag_count++;
    	}
    }
    if(flag_count == 0){
    	printf("Error, There must be at least 1 -f -b -t-p -l argument\n");
    	return -1;
    }

    for(int i=0; i<6; i++){
    	printf("flags_bool[%d] = %d\n",i,flags_bool[i]);
    }

    printf("%s\n", target_directory );
    printf("%s\n", filename);
    printf("%d\n", file_size);
    printf("%c\n", file_type);

	return 0;
}