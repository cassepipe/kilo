#include <stdlib.h>
#include <string.h> 
#include <stdio.h>

#define NULL_CHECK(x) do{\
	if (!x) {\
	  	fprintf(stderr, "Failed to allocate memory for pointer %s inside  %s", #x, __func__); \
		exit(EXIT_FAILURE); \
		} \
} while (0)


struct gbuf {
	char* buff;
	int len;
};

void gbufAppend(struct gbuf* gbuf_ptr, const char* s, int len)
{

	gbuf_ptr->buff = realloc(gbuf_ptr->buff, gbuf_ptr->len + len) ; // Give the buff member of gbuf1 instance larger memoryblock

 	NULL_CHECK(gbuf_ptr->buff); //Check for realloc failure

	memcpy(gbuf_ptr->buff + gbuf_ptr->len, s, len); // Copy s in the buffer 

	gbuf_ptr->len += len; //Update the lenght of the new buffer

	//printf(" Buff = \"%s\" \n", gbuf_ptr->buff);
	//printf(" len = %d \n", gbuf_ptr->len);
}

int main()
{ 
	struct gbuf gbuf1 = {NULL, 0};

	gbufAppend(&gbuf1, "Hello ", 6); 		 

	printf("\n %s \n", gbuf2.buff);

	gbufAppend(&gbuf1, "World\0", 6);

	printf("\n %s \n", gbuf1.buff);


	// It works !
	
	return 0;
}

