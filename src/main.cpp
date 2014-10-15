#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <termios.h>
#include <unistd.h>
#include "crypto.h"
#include "config.h"

#define FREQ_MAX_FREQ ((FREQ_SAMPLING_RATE)/2)
#define LOOP_SIZE 64

volatile bool g_run;
const uint8_t g_src_data[LOOP_SIZE]="Hello World!\n\r";
uint8_t g_dst_data[LOOP_SIZE];
uint32_t g_data_pos;

static double modulate(void)
{

	int byte, bits;

	bits = (g_data_pos&3)*2;
	byte = g_data_pos >> 2;
	if(byte>=LOOP_SIZE)
	{
		g_data_pos=0;
		bits = 0;
		byte = 0;
	}
	else
		g_data_pos++;

	return ((g_dst_data[byte]>>bits) & 0x3)/3.0;
}

static void* modulator(void *ptr)
{
	int i, sample;
	uint16_t noise;
	TCryptoEngine prng;
	double modulation;

	memset(&prng, 0, sizeof(prng));

	while(g_run)
	{
		modulation = modulate();

		/* AES encrypt block, result in 'out' */
		aes(&prng);
		/* make AES output the IV of the next encryption */
		memcpy(prng.in, prng.out, AES_BLOCK_SIZE);

		for(i=0; i<(AES_BLOCK_SIZE/2); i++)
		{
			/* get white PRNG noise */
			noise = ((uint16_t*)&prng.out)[i];

			/* modulate data on noise */
			sample = 0x8000 +
				(modulation * (((int)(noise & 0x7F)) - 0x40));

			/* scale +/- 1 to 16 bit */
			putchar((sample >> 0) & 0xFF);
			putchar((sample >> 8) & 0xFF);
		}
	}
	return NULL;
}

static void bailout(const char* msg)
{
	fprintf(stderr, "ERROR: %s\n", msg);
	exit(EXIT_FAILURE);
}

static void aes_encrypt(void)
{
	uint8_t t, i, *dst, length;
	const uint8_t *src;
	TCryptoEngine whitening_key;

	src = g_src_data;
	dst = g_dst_data;
	length = LOOP_SIZE;

	/* reset whitening key to zero: FIXME config file needed */
	memset(&whitening_key, 0, sizeof(whitening_key));
	/* FIXME randomize IV and put it into circular buffer */

	/* encrypt data */
	while(length)
	{
		/* process data block by block */
		t = (length>=AES_BLOCK_SIZE) ? AES_BLOCK_SIZE : length;
		length -= t;

		/* AES encrypt block, result in 'out' */
		aes(&whitening_key);
		/* make AES output the IV of the next encryption */
		if(length)
			memcpy(whitening_key.in, whitening_key.out, AES_BLOCK_SIZE);

		/* XOR previous AES output data in */
		for(i=0; i<t; i++)
			*dst++ = whitening_key.out[i] ^ *src++;
	}
}

int main(int argc, char * argv[])
{
	int res;
	pthread_t thread;
	double freq;
	struct termios otio, ntio;

	/* init variables */
	freq = FREQ_START;
	g_run = true;
	g_data_pos = 0;

	/* encrypt data */
	aes_encrypt();

	/* start sound processing thread */
	if((res = pthread_create(&thread, NULL, &modulator, NULL)))
		bailout("failed to start thread\n");
	fprintf(stderr, "\n(press 'q' to exit)\n\n");

	/* disable input buffering */
	tcgetattr(STDIN_FILENO,&otio);
	ntio = otio;
	ntio.c_lflag &=(~ICANON & ~ECHO);
	tcsetattr(STDIN_FILENO,TCSANOW,&ntio);

	/* start looping */
	while(true)
	{
		/* print current frequency */
		fprintf(stderr,"\rfreq = %08.2f Hz", freq);

		/* terminate on button press 'q' or 'x' */
		if((res = getchar())==EOF)
			break;

		if(res=='q')
			break;
		else
			switch(res)
			{
				case '+' :
					freq+=FREQ_STEP_COARSE;
					if(freq>FREQ_MAX_FREQ)
						freq = FREQ_MAX_FREQ;
					break;
				case '-' :
					freq-=FREQ_STEP_COARSE;
					if(freq<10)
						freq = 10;
					break;
			}
	}

	/* restore the former settings */
	tcsetattr(STDIN_FILENO,TCSANOW,&otio);

	/* waiting for thread to exit */
	g_run = false;
	pthread_join( thread, NULL);

	fprintf(stderr, "\n\n[DONE]\n");
	return EXIT_SUCCESS;
}
