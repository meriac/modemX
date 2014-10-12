#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <termios.h>
#include <unistd.h>
#include "config.h"

#define FREQ_MAX_FREQ ((FREQ_SAMPLING_RATE)/2)

#define CARRIER_FREQ (FREQ_SAMPLING_RATE/8)
#define CYCLES_PER_SYMBOL 4
#define SYMBOL_PAUSE 16

volatile bool g_run;
volatile double g_step;

const uint8_t g_hello_world[]="Hello World!\n\r";

static void qam256_enc(uint8_t data, double &phase, double &amplitude)
{
	int x,y;

	x = ((data >> 4)      )-8;
	y = ((data >> 0) & 0xF)-8;

	phase = atan2(y, x);
	amplitude = sqrt(x*x + y*y)/sqrt(8*8+8*8);
}

static void* modulator(void *ptr)
{
	int length, sample;
	const uint8_t* data;
	double phase, amplitude, end, pos;

	pos = 0;
	while(g_run)
	{
		data = g_hello_world;
		length = sizeof(g_hello_world);
		while(length--)
		{
			/* QAM encode data */
			qam256_enc(*data++, phase, amplitude);

			/* calculate termination sample */
			end = pos + ((CYCLES_PER_SYMBOL)*2*M_PI);
			while(pos <= end)
			{
				/* calculate wave */
				sample = ((int)(sin(pos + phase)*amplitude*0x7FFF+0.5))+0x8000;

				/* scale +/- 1 to 16 bit */
				putchar((sample >> 0) & 0xFF);
				putchar((sample >> 8) & 0xFF);

				/* get next sample */
				pos += g_step;
			}
		}

		/* emit modulation pause */
		end = pos + (((SYMBOL_PAUSE)*(CYCLES_PER_SYMBOL))*2*M_PI);
		while(pos <= end)
		{
			/* emit pause */
			putchar(0x00);
			putchar(0x80);

			/* get next sample */
			pos += g_step;
		}
	}
	return NULL;
}

static void bailout(const char* msg)
{
	fprintf(stderr, "ERROR: %s\n", msg);
	exit(EXIT_FAILURE);
}

int main(int argc, char * argv[])
{
	int res;
	pthread_t thread;
	double freq;
	struct termios otio, ntio;

	/* init variables */
	freq = FREQ_START;
	g_step = freq/FREQ_SAMPLING_RATE*2*M_PI;
	g_run = true;

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
		g_step = freq/FREQ_SAMPLING_RATE*2*M_PI;
	}

	/* restore the former settings */
	tcsetattr(STDIN_FILENO,TCSANOW,&otio);

	/* waiting for thread to exit */
	g_run = false;
	pthread_join( thread, NULL);

	fprintf(stderr, "\n\n[DONE]\n");
	return EXIT_SUCCESS;
}
