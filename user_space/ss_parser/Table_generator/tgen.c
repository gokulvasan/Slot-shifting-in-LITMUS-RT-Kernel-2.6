#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include <string.h>
 
// #define DEBUG

int main(int argc, char *argv[]) {
	/*needs to do just one check before making anything further 
	 i.e. char "" should be checked if < 255 then fill the rest with 0xFF
	*/
	int count;
	static char buffer[2048];
	char *t;
	char *arg;
	int j = -1;
	int i = 0;
	t = buffer;
	memset(t, 0x00, sizeof(buffer));
	if(argc <= 1) 
		return 0;
#ifdef DEBUG
	printf("argc: %d\n", argc);
#endif
	for(count = 1; count < argc; count++) {

		arg = argv[count];

		j += 1;


		if('*' == arg[0] && '#' == arg[1]) {
			arg += 2;
#ifdef DEBUG
			printf("i :%d *******string*******\n", i);
#endif
			for (i = 0; arg[i] != '*'; i++,j++)
				buffer[j] = arg[i]; // do string formatting here...

			while(i < 255) {
				buffer[j] = 0xFF;
				i++;
				j++;
			}
			
			buffer[j] = ',';
#ifdef DEBUG
			//buffer[j+1] = '\0';
			printf("buffer parsing: %d: %d\n", j, i);
#endif
		}
		else if('%' == arg[0] && '[' == arg[1]) {

			char temp[5];			
			memcpy(&buffer[j], &arg[1], 1);
			arg += 2;
			j++;
#ifdef DEBUG
			printf("In job parsing\n");
#endif			
			while(']' != *arg) {
				 //intr id
				memcpy(temp, arg, 1);
				arg++;

				i = atoi(temp);	
				memcpy(&buffer[j], &i, 4);
				j += 4;

				buffer[j] = ':';
				j++;
				arg++;

				// est 
				memcpy(temp, arg, 1);
				arg++;
				i = atoi(temp);	
				memcpy(&buffer[j], &i, 4);
				j += 4;

				buffer[j] = ',';
				arg++;
				j++;
			}
			buffer[j] = ']';
			j++;
			buffer[j] = ',';
		//	j++;
		}	
		else {
			i = atoi(arg);
#ifdef DEBUG
			printf("i: %d, j: %d\n", i, j);
#endif
			memcpy(&buffer[j], &i, 4);
			j += 4;
			buffer[j] = ',';
		}			
	} 
			buffer[j] = '\r';
			buffer[j+1] = '\n';
			for(i=0; !((buffer[i+1] == '\n') && (buffer[i] == '\r')) ; i++){
				printf("%c", buffer[i]);

#ifdef DEBUG
				if(buffer[i] == ',') {
					int j;
					memcpy(&j, &buffer[i+1], 4);
					printf("\n%d\n", j);
				}
#endif
			}
			printf("\r\n");
#ifdef DEBUG
			printf("******Final count: %d*********\n", i);
#endif
	return 0;
}
