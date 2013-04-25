#include <stdio.h>

#include <math.h>
#include "bass.h"


#define MAX_SCORES     10


void *data[17];
int length[17];

// ? c d e f g a b C D E F G A B Z
const int default_wavs[16] = {23, 24, 26, 28, 29, 31, 33, 35, 36, 38, 40, 41, 43, 45, 47, 48};

int loadWav(const char *name, int num)
{
FILE *fp;

    fp = fopen(name,"rb");
	if (fp == NULL)
	{
		return 0;
	}

	fseek(fp,0,SEEK_END);
	length[num] = ftell(fp);
	data[num] = malloc(length[num]);
	rewind(fp);      // could use "fseek(fp,0,SEEK_SET);"
	fread(data[num],length[num],1,fp);    

	fclose(fp);

    return 1;
}



void playMusic (int num, float volume)
{
	if (num < 0 || num > 16) return;

	HSTREAM stream = BASS_StreamCreateFile(TRUE,data[num],0,length[num],0);
	BASS_ChannelSetAttribute(stream, BASS_ATTRIB_VOL, volume);
	BASS_ChannelPlay(stream,TRUE);
}



void initMusic()
{
int ret, i;
char filename[256];

	BASS_Init (-1, 44100, 0, 0, NULL);

	for (i = 0; i < 16; ++i)
	{
	   sprintf(filename, "wavs/%d.wav", default_wavs[i]);
	   ret = loadWav(filename, i);

	   if (ret == 0)
	   {
		   int errCode = BASS_ErrorGetCode();
		   char errString[256];
		   sprintf(errString, "error code is %d\n", errCode);

		   OutputDebugStringA(errString);
	   }

	   playMusic(i, 0.2);
	   Sleep(100);
	}

	ret = loadWav("wavs/border.wav", 16);

	if (ret == 0)
	   {
		   int errCode = BASS_ErrorGetCode();
		   char errString[256];
		   sprintf(errString, "error code is %d\n", errCode);

		   OutputDebugStringA(errString);
	   }
}




void endMusic() 
{
	BASS_Stop();
	BASS_Free();
}



