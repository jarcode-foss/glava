#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "pcm.h"

void* pcm_main(void* data)
{
	struct pcm_struct* pcm = (struct pcm_struct*) data;

	FILE *fp;
	char path[1035];

	/* Open the command for reading. */
	fp = popen("while :;     do         /usr/bin/pacmd list-sink-inputs | grep -c 'state: RUNNING'; done", "r");
	if (fp == NULL) {
		printf("Failed to run command\n" );
		exit(1);
	}


	int stateold=0;
	int state=0;

	/* Read the output a line at a time - output it. */
	// while (fgets(path, sizeof(path) - 1, fp) != NULL) {
	while (fgets(path, sizeof(path) - 1, fp) != NULL) {
		// printf("\nnayeet%s", path);
		state = atoi(path);
		if (state==0 && stateold>0)
		{
			sleep(5);
			printf("Audio Paused\n\n");
		}
		stateold=state;
		pcm->stateplay=state;
	}

	/* close */
	pclose(fp);

}