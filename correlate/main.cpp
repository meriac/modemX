#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include "crypto.h"

#define CODE_BITS 512
#define AES_BLOCKS (CODE_BITS/(AES_BLOCK_SIZE*8))

#define CARRIER_FREQ_CENTER 880
#define CARRIER_FREQ_MOD 50
#define CYCLES_PER_SYMBOL 2

#ifndef NOISE
#define NOISE 0.5
#endif/*NOISE*/

#define SAMPLES_PER_CYCLE (FREQ_SAMPLING_RATE/CARRIER_FREQ_CENTER)
#define SAMPLES_PER_SYMBOL (AES_BLOCKS*AES_BLOCK_SIZE*8*CYCLES_PER_SYMBOL*SAMPLES_PER_CYCLE)

#define SYMBOL_COUNT 2
#define TEST_REPEAT 16

double g_pos;
static int16_t g_symbol[SYMBOL_COUNT][SAMPLES_PER_SYMBOL];
static int16_t g_test_signal[TEST_REPEAT*SAMPLES_PER_SYMBOL];

void symbol(uint8_t data, int16_t* dst)
{
	int i,j,k,bit;
	double step;
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
			step = (2*M_PI*(CARRIER_FREQ_CENTER +
				(bit ? CARRIER_FREQ_MOD : -CARRIER_FREQ_MOD)
			))/FREQ_SAMPLING_RATE;

			for(k=0; k<(CYCLES_PER_SYMBOL*SAMPLES_PER_CYCLE); k++)
			{
				/* calculate wave */
				*dst++ = (int)(sin(g_pos)*0x7FFF+0.5);
				g_pos += step;
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
	sum/=(SAMPLES_PER_SYMBOL*0x200UL);

	return sum;
}

int main(int argc, char * argv[])
{
	int i,j,t,n;
	int16_t *p, r0, r1, r2;
	int16_t modulation[SAMPLES_PER_SYMBOL];
	TCryptoEngine noise;

	fprintf(stderr, "SAMPLES_PER_SYMBOL=%i\n\r", SAMPLES_PER_SYMBOL);

	/* set up noise generator */
	memset(&noise, 0, sizeof(noise));

	/* init correlators */
	for(j=0; j<SYMBOL_COUNT; j++)
	{
		/* start on same phase for every symbol */
		g_pos = 0;
		symbol(j, g_symbol[j]);
	}

	j = 0;
	g_pos = 0;
	p = g_test_signal;
	for(t=0; t<TEST_REPEAT; t++)
	{
		/* calculate next symbol - alternate between 0/1 */
		symbol(t&1, modulation);

		for(i=0; i<SAMPLES_PER_SYMBOL; i++)
		{
			n = j % (AES_BLOCK_SIZE/2);
			j++;
			if(n==0)
			{
				aes(&noise);
				memcpy(noise.in, noise.out, AES_BLOCK_SIZE);
			}
			/* extract noise from AES stream in 16 bit steps */
			n = ((int16_t*)&noise.out)[n];

			/* add noise to attenuated symbol */
			*p++ = modulation[i]/64 + (n*NOISE);
		}
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
