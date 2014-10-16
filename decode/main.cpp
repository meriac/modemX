#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include "config.h"
#include "crypto.h"

#define LOOP_SAMPLES (LOOP_SIZE*4*AES_BLOCK_SIZE/2)
uint64_t g_aggregate[LOOP_SAMPLES];

int main(int argc, char * argv[])
{
	int r1, r2, sample, pos, t;
	memset(&g_aggregate, 0, sizeof(g_aggregate));

	printf("LOOP_SAMPLES=%i\n\r", LOOP_SAMPLES);
	pos = 0;
	while(true)
	{
		if((r1 = getchar())==EOF)
			break;
		if((r2 = getchar())==EOF)
			break;

		sample = abs((r1 | (r2<<8))-0x8000);

		g_aggregate[pos++] += sample;
		if(pos>=LOOP_SAMPLES)
			pos = 0;
	}

	for(t=0; t<LOOP_SAMPLES; t++)
		printf("%li\n", g_aggregate[t]);
	printf("===\n\r");
}
