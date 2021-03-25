#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include <sys/types.h>
#include <dirent.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>


struct FileAttributes{
	char* target_directory; //w
	char* filename; //f
	int file_size; //b
	char file_type; //t
	char* permissions; //p
	int link_number; //l
};

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
	//printf("%s\n",filename );
	//printf("%s\n", reg_file);
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

//tokenize file path and fetch file name
// for example if filepath = aaa/bbb/ccc.txt
// return ccc.txt
char* takeFileName(char* file_path){
	char* tempstr = calloc(strlen(file_path)+1, sizeof(char));
	strcpy(tempstr, file_path);
	char* token;
	char* backup;
	token = strtok(tempstr,"/");

	while(token != NULL){
		backup = token;
		token = strtok(NULL,"/");
		if(token == NULL) break;
	}
	free(tempstr);
	return backup;
}

// check a file is matching with corresponding search criteria
//if so return 1, otherwise return 0
//used stat system call for access file attributes.
int checkFileMatching(char* file_path, struct FileAttributes file_attributes, int* arg_flags){
	int check_valid_match = 1;
	struct stat fileStat;
	lstat(file_path,&fileStat);

	if(arg_flags[0] == 1){ //filename
		char* filename = takeFileName(file_path);
		if(checkRegularExp(filename,file_attributes.filename) != 1){
			check_valid_match = 0;
		}
	}
	if(arg_flags[1] == 1){ //filesize
		if(fileStat.st_size != file_attributes.file_size){
			check_valid_match = 0;
		}
	}
	if(arg_flags[2] == 1){ //filetype
		char stat_type; 
		if(S_ISDIR(fileStat.st_mode)){ // check file dir 
			stat_type = 'd';
		}
		else if(S_ISREG(fileStat.st_mode)){ //check file regular
			stat_type = 'f';
		}
		else if(S_ISBLK(fileStat.st_mode)){ //check file block device
			stat_type = 'b';
		}
		else if(S_ISCHR(fileStat.st_mode)){ //check file character device
			stat_type = 'c';
		}
		else if(S_ISLNK(fileStat.st_mode)){ //chcek file Symbolic link
			stat_type = 'l';
		}
		else if(S_ISFIFO(fileStat.st_mode)){ //check file fifo or pipe
			stat_type = 'p';
		}
		else{ //check file socket file or not
			stat_type = 's';
		}

		if(stat_type != file_attributes.file_type){
			check_valid_match = 0;
		}
	}
	if(arg_flags[3] == 1){//permissions
		char stat_permissions[9];
		stat_permissions[0] = (fileStat.st_mode & S_IRUSR) ? 'r' : '-';
		stat_permissions[1] = (fileStat.st_mode & S_IWUSR) ? 'w' : '-';
		stat_permissions[2] = (fileStat.st_mode & S_IXUSR) ? 'x' : '-';

		stat_permissions[3] = (fileStat.st_mode & S_IRGRP) ? 'r' : '-';
		stat_permissions[4] = (fileStat.st_mode & S_IWGRP) ? 'w' : '-';
		stat_permissions[5] = (fileStat.st_mode & S_IXGRP) ? 'x' : '-';

		stat_permissions[6] = (fileStat.st_mode & S_IROTH) ? 'r' : '-';
		stat_permissions[7] = (fileStat.st_mode & S_IWOTH) ? 'w' : '-';
		stat_permissions[8] = (fileStat.st_mode & S_IXOTH) ? 'x' : '-';

		if(strcmp(stat_permissions,file_attributes.permissions) != 0){
			check_valid_match = 0;
		}
	}
	if(arg_flags[4] == 1){//linknumber
		if(fileStat.st_nlink != file_attributes.link_number){
			check_valid_match = 0;
		}
	}
	

	return check_valid_match;
}

//Traverse Directory and try to find a matching file
void traverseDictionary(char* openDirectory,struct FileAttributes file_attributes,int* arg_flags){

	
	DIR* dir;
	struct dirent *dp;
	dir = opendir(openDirectory);
	
	struct stat fileStat;
	while((dp = readdir(dir)) != NULL ){
		
		char* subdir = calloc(1, 4094 + 1);
		if ( !strcmp(dp->d_name, ".") || !strcmp(dp->d_name, "..") )
        {
            // do nothing (straight logic)
        }
        else{
        	
        	strcat(subdir,openDirectory);
        	strcat(subdir + strlen(openDirectory),"/");
        	strcat(subdir + strlen(openDirectory) + 1,dp->d_name);
			//printf("%s\n",subdir );
			int x = checkFileMatching(subdir,file_attributes,arg_flags);
			if(x==1) printf("%s\n",subdir );
			stat(subdir,&fileStat);
			if(S_ISDIR(fileStat.st_mode)){
				
				traverseDictionary(subdir,file_attributes,arg_flags);
			}
        }
        free(subdir);
	}
	closedir(dir);
}

int main(int argc, char *argv[]){

	int opt;
	struct FileAttributes fileAttributes;
	//declare all bool flags false
	//fbtplw
	int flags_bool[6];
	for(int i=0; i<6; i++){
		flags_bool[i] = 0;
	} 

/*
	char* target_directory; //w
	char* filename; //f
	int file_size; //b
	char file_type; //t
	char* permissions; //p
	int link_number; //l
*/

	extern char *optarg;
	extern int optind, optopt, opterr;
 	while((opt = getopt(argc, argv, "f:b:t:p:l:w:")) != -1)  
    {  
        switch(opt)  
        {  
            case 'f': 
            	flags_bool[0] = 1;

            	if(optarg[0] == '+'){
            		printf("Error in -f argument, wrong regular expression usage..\n");
            		return -1;
            	}
            	fileAttributes.filename = optarg;
            	printf("option: %c\n", opt);
                break; 
            case 'b':  
            	flags_bool[1] = 1;

            	int isNumberB = checkNumber(optarg);
            	if(isNumberB == 0){
            		printf("Error, b argument should be integer\n");
            		return -1;
            	}
            	fileAttributes.file_size = atoi(optarg);
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
            	fileAttributes.file_type = optarg[0];
            	printf("option: %c\n", opt);
                break;
            case 'p':
            	flags_bool[3] = 1;
            	//check -p valid or not
            	if(strlen(optarg) != 9){
            		printf("Error, -p argument must be 9 characters\n");
            		return -1;
            	}
            	fileAttributes.permissions = optarg;
                printf("option: %c\n", opt);
                break;  
            case 'l':
            	flags_bool[4] = 1; 

            	int isNumberL = checkNumber(optarg);
            	if(isNumberL == 0){
            		printf("Error, b argument should be integer\n");
            		return -1;
            	}
            	fileAttributes.link_number = atoi(optarg);
                printf("option: %c\n", opt);
                break;
			case 'w':
				flags_bool[5] = 1; 
				fileAttributes.target_directory = optarg;
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

    printf("%s\n", fileAttributes.target_directory );
    printf("%s\n", fileAttributes.filename);
    printf("%d\n", fileAttributes.file_size);
    printf("%c\n", fileAttributes.file_type);

    int temp = checkRegularExp("losttttfile", "lost+fil+e+");
    printf("%d\n", temp );
    printf("-------------------\n");
    traverseDictionary("testfile",fileAttributes,flags_bool);

    char str[] ="testfile/z2/textttty.txt";
    //printf("%s\n",takeFileName(str) );
    //printf("%s\n",takeFileName(str) );
    //printf("%s\n",takeFileName(str) );
    //printf("%d\n",checkRegularExp(takeFileName(str),"text+y.txt") );
	
	return 0;
}