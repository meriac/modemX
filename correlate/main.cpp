#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include "crypto.h"

#define CODE_BITS 1024
#define AES_BLOCKS (CODE_BITS/(AES_BLOCK_SIZE*8))

#define CARRIER_FREQ (FREQ_SAMPLING_RATE/8)
#define CYCLES_PER_SYMBOL 4

#define SAMPLES_PER_CYCLE (FREQ_SAMPLING_RATE/CARRIER_FREQ)
#define SAMPLES_PER_SYMBOL (AES_BLOCKS*AES_BLOCK_SIZE*8*CYCLES_PER_SYMBOL*SAMPLES_PER_CYCLE)

#define SYMBOL_COUNT 2

static double g_pos, g_step;
static int16_t g_symbol[SYMBOL_COUNT][SAMPLES_PER_SYMBOL];

void symbol(uint8_t data, int16_t* dst)
{
	int i,j,k,bit;
	double phase;
	TCryptoEngine prng;

	/* FIXME: initialize with proper key */
	memset(&prng, 0, sizeof(prng));
	/* seed code */
	prng.in[0] = data;

	for(i=0; i<AES_BLOCKS; i++)
	{
		/* run encryption */
		aes(&prng);

		/* make AES output the IV of the next encryption */
		memcpy(prng.in, prng.out, AES_BLOCK_SIZE);

		for(j=0; j<(AES_BLOCK_SIZE*8); j++)
		{
			bit = (prng.out[j>>3] >> (j&7)) & 1;
			phase = bit * M_PI;

			for(k=0; k<(CYCLES_PER_SYMBOL*SAMPLES_PER_CYCLE); k++)
			{
				/* calculate wave */
				*dst++ = (int)(sin(g_pos + phase)*0x3FFF+0.5);
				g_pos += g_step;
			}
		}
	}
}

int main(int argc, char * argv[])
{
	int i,j,t;
	int16_t sample;

	/* init correlators */
	for(t=0; t<2; t++)
		symbol(t, g_symbol[t]);

	g_pos = 0;
	g_step = (2*M_PI*CARRIER_FREQ)/FREQ_SAMPLING_RATE;

	for(t=0; t<128; t++)
	{
		for(j=0; j<2; j++)
		{
			for(i=0; i<SAMPLES_PER_SYMBOL; i++)
			{
				sample = g_symbol[j][i];
				putchar((sample >> 0) & 0xFF);
				putchar((sample >> 8) & 0xFF);
			}
		}
	}
}
