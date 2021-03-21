#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

int main(int argc, char *argv[]){

	int opt;

	while((opt = getopt(argc, argv, "fbtplw")) != -1)  
    {  
        switch(opt)  
        {  
            case 'f': 
            	printf("option: %c\n", opt);
                break; 
            case 'b':  
            	printf("option: %c\n", opt); 
                break;
            case 't':  
                printf("option: %c\n", opt);
                break;
            case 'p':  
                printf("option: %c\n", opt);
                break;  
            case 'l':  
                printf("option: %c\n", opt);
                break;
			case 'w':  
                printf("option: %c\n", opt);
                break; 
            default:
            	printf("ERROR opt\n");  
        }  
    }

	return 0;
}