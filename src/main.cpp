#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <termios.h>
#include <unistd.h>
#include "config.h"

#define FREQ_MAX_FREQ ((FREQ_SAMPLING_RATE)/2)

volatile bool g_run;
volatile double g_step;

static void* modulator(void *ptr)
{
	uint16_t data;
	double pos,sample;

	pos = 0;
	while(g_run)
	{
		sample = sin(pos);
		pos += g_step;

		/* scale +/- 1 to 16 bit */
		data = (sample*0x8000)+0x8000;
		putchar((data >> 0) & 0xFF);
		putchar((data >> 8) & 0xFF);
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
