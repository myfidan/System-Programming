#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

//check a string is consist of a number or not
//used for optarg in -b and -l paramaters
int checkNumber(char* str){
	while(*str != '\0'){
		if(*str < '0' || *str > '9'){
			return 0;
		}
		str++;
	}
	return 1;
}

//check a filename match with given regular expression filename or not
//like lost+file match lostttttfile
int checkRegularExp(char* filename, char* reg_file){
	int reg_index=0,file_index=0;

	while(reg_file[reg_index] != '\0'){

		//normal character
		if(reg_file[reg_index] != '+'){
			if(reg_file[reg_index] != filename[file_index]){
				return -1;//this 2 file not match
			}
		}//+ reg
		else{
			char prev_char = reg_file[reg_index-1];

			while(filename[file_index] == prev_char){

				file_index++;
			}
			file_index--;
		}
		reg_index++;
		file_index++;
	}

	return 1;
}

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

            	int isNumberB = checkNumber(optarg);
            	if(isNumberB == 0){
            		printf("Error, b argument should be integer\n");
            		return -1;
            	}
            	file_size = atoi(optarg);
            	
        		
            	printf("option: %c\n", opt); 
                break;
            case 't':
            	flags_bool[2] = 1; 
            	//check t argument valid or not
            	if(strlen(optarg) > 1 || (*optarg != 'd' && *optarg != 's' && 
            		*optarg != 'b' && *optarg != 'c' && *optarg != 'f' &&
            		*optarg != 'p' && *optarg != 'l')){

            		printf("Error for argument -t\n");
            		return -1;
            	}

            	file_type = optarg[0];
                printf("option: %c\n", opt);
                break;
            case 'p':
            	flags_bool[3] = 1;
            	//check -p valid or not
            	if(strlen(optarg) != 9){
            		printf("Error, -p argument must be 9 characters\n");
            		return -1;
            	}
            	permissions = optarg; 
                printf("option: %c\n", opt);
                break;  
            case 'l':
            	flags_bool[4] = 1; 

            	int isNumberL = checkNumber(optarg);
            	if(isNumberL == 0){
            		printf("Error, b argument should be integer\n");
            		return -1;
            	}

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

    //Check there is w argument or not because w argument mandatory
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

    int temp = checkRegularExp("losttttfile", "lost+fil+e+");
    printf("%d\n", temp );
	return 0;
}