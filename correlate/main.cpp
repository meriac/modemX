#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include "crypto.h"

#define CODE_BITS 2048
#define AES_BLOCKS (CODE_BITS/(AES_BLOCK_SIZE*8))

#define CARRIER_FREQ (FREQ_SAMPLING_RATE/8)
#define CYCLES_PER_SYMBOL 4

#define SAMPLES_PER_CYCLE (FREQ_SAMPLING_RATE/CARRIER_FREQ)
#define SAMPLES_PER_SYMBOL (AES_BLOCKS*AES_BLOCK_SIZE*8*CYCLES_PER_SYMBOL*SAMPLES_PER_CYCLE)

#define SYMBOL_COUNT 2
#define TEST_REPEAT 16

static int16_t g_symbol[SYMBOL_COUNT][SAMPLES_PER_SYMBOL];
static int16_t g_test_signal[TEST_REPEAT*SAMPLES_PER_SYMBOL];

void symbol(double freq, uint8_t data, int16_t* dst)
{
	int i,j,k,bit;
	double phase, pos, step;
	TCryptoEngine prng;

	/* FIXME: initialize with proper key */
	memset(&prng, 0, sizeof(prng));
	/* seed code */
	prng.in[0] = data;

	pos = 0;
	step = (2*M_PI*freq)/FREQ_SAMPLING_RATE;

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
				*dst++ = (int)(sin(pos + phase)*0x7FFF+0.5);
				pos += step;
			}
		}
	}
}

int correlate(uint8_t data, int16_t* dst)
{
	int i;
	int16_t *src;
	uint64_t sum;

	sum = 0;
	src = g_symbol[data];
	for(i=0; i<SAMPLES_PER_SYMBOL; i++)
		sum += (*src++)*(int)(*dst++);
	sum/=(SAMPLES_PER_SYMBOL*0x100UL);

	return sum;
}

int main(int argc, char * argv[])
{
	int i,j,t,n;
	int16_t *p, r0, r1, r2;
	TCryptoEngine noise;

	/* set up noise generator */
	memset(&noise, 0, sizeof(noise));

	/* init correlators */
	for(j=0; j<SYMBOL_COUNT; j++)
		symbol(CARRIER_FREQ, j, g_symbol[j]);

	j = 0;
	p = g_test_signal;
	for(t=0; t<TEST_REPEAT; t++)
		for(i=0; i<SAMPLES_PER_SYMBOL; i++)
		{
			n = j % (AES_BLOCK_SIZE/2);
			j++;
			if(n==0)
			{
				aes(&noise);
				memcpy(noise.in, noise.out, AES_BLOCK_SIZE);
			}
			n = ((int16_t*)&noise.out)[n];

			*p++ = g_symbol[t&1][i]/64 + (n/2);
		}

	p = g_test_signal;
	for(t=0; t<(TEST_REPEAT-1); t++)
		for(i=0; i<SAMPLES_PER_SYMBOL; i++)
		{
			r1 = (int16_t)correlate(0, p);
			r2 = (int16_t)correlate(1, p);
			r0 = *p++;

			putchar((r0 >> 0) & 0xFF);
			putchar((r0 >> 8) & 0xFF);
			putchar((r1 >> 0) & 0xFF);
			putchar((r1 >> 8) & 0xFF);
			putchar((r2 >> 0) & 0xFF);
			putchar((r2 >> 8) & 0xFF);
		}
}
