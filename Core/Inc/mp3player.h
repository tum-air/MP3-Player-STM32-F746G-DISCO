#ifndef __mainmp3_H__
#define __mainmp3_H__


#include "main.h"
#include "stm32f7xx_hal.h"
#include "fatfs.h"

#include "lcd_log.h"
#include "arm_math.h"



// ------------------------------------------------------
// the most crucial part: the Audio buffer
// ------------------------------------------------------

typedef enum {
	BUFFER_OFFSET_NONE = 0,
	BUFFER_OFFSET_HALF,
	BUFFER_OFFSET_FULL,
}BUFFER_StateTypeDef;

#define AUDIO_BUFFER_SIZE       2304 * 2 * 2 // == (2304 MP-3 Stereo-Samples * 4 Bytes per Stereo Sample) = 9216 as "double" ring buffer

typedef struct {
	uint8_t buff[AUDIO_BUFFER_SIZE];
	BUFFER_StateTypeDef state;
}AUDIO_OUT_BufferTypeDef;

static  AUDIO_OUT_BufferTypeDef BufferCtl; // needs to be static, otherwise optimizer will interfere!!

// ------------------------------------------------------
// end of the most crucial part: the Audio buffer
// ------------------------------------------------------



#define FILEMGR_LIST_DEPDTH                        512 // number of song titles per disk
#define FILEMGR_FILE_NAME_SIZE                     255
#define FILEMGR_FULL_PATH_SIZE                     256
#define FILEMGR_MAX_LEVEL                          4
#define FILETYPE_DIR                               0
#define FILETYPE_FILE                              1

typedef struct _FILELIST_LineTypeDef {
	uint8_t type;
	uint8_t name[FILEMGR_FILE_NAME_SIZE];
	int size;
}FILELIST_LineTypeDef;

typedef struct _FILELIST_FileTypeDef {
	FILELIST_LineTypeDef  file[FILEMGR_LIST_DEPDTH] ;
	uint16_t              ptr;
}FILELIST_FileTypeDef;

typedef enum {
	FILE_PLAYING = 0,
	FILE_ENDED,
}FILE_Playing_StateTypeDef;

extern FILE_Playing_StateTypeDef File_Playing_State;
extern FILELIST_FileTypeDef FileList;
extern uint16_t NumObs;
extern uint16_t file_idx;
extern FRESULT res;


// For FFT:
extern int16_t FFTBuffer [AUDIO_BUFFER_SIZE/2]; // 4608 bytes:  1152 16 bit values L and 1152 16 bit values R
extern float32_t FFT_inputstruct_Left[2048]; // for 1024 FFT
extern float32_t FFTOutputMagLeft[1024];
extern float32_t FFT_inputstruct_Right[2048]; // for 1024 FFT
extern float32_t FFTOutputMagRight[1024];


const char *get_filename_ext(const char *filename);
void mp3_init_play (char* filename, int filesizeinfo);
uint8_t Audio_Process (void);

uint32_t Mp3ReadId3V2Text(FIL* pInFile, uint32_t unDataLen, char* pszBuffer, uint32_t unBufferSize);
uint32_t Mp3ReadId3V2Tag(FIL* pInFile, char* pszArtist, uint32_t unArtistSize, char* pszTitle, uint32_t unTitleSize);

FRESULT AUDIO_StorageParse(char* pathname);
uint8_t AUDIO_ShowMP3Files(char* pathname);

void Draw_Dynamic_Text (void);

#endif
