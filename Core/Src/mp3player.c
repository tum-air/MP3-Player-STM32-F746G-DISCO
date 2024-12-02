/* Copyright (C) Technical University of Munich - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Alois Knoll <knoll@in.tum.de>, 2021
 */

#include "mp3player.h"

#include <mp3dec.h>
#include "main.h"
#include "mp3common.h"
#include <string.h>

#include "stm32f7xx_hal.h"
#include "stm32746g_discovery_audio.h"

#include "arm_const_structs.h"

char szArtist[120];
char szTitle[120];

// MP3 Variables
MP3FrameInfo			mp3FrameInfo;
HMP3Decoder 			hMP3Decoder;
__IO uint32_t uwVolume;
int unFramesDecoded;
DWORD totalsum;

// FATFS Variables
static FIL				file;
#define FILE_READ_BUFFER_SIZE 8192
static char				file_read_buffer[FILE_READ_BUFFER_SIZE];
static int			    bytes_left;
static char				*read_ptr;
static unsigned int br, btr;
static int offset, err;
FILE_Playing_StateTypeDef File_Playing_State;
FILELIST_FileTypeDef FileList;
uint16_t NumObs;

// Miscellaneous
uint32_t elapsed_time;
static uint32_t prev_elapsed_time = 0xFFFFFFFF;
char str [120];


// For FFT:
int16_t FFTBuffer [AUDIO_BUFFER_SIZE/2]; // 4608 bytes:  1152 16 bit values L and 1152 16 bit values R
float32_t FFT_inputstruct_Left[2048]; // for 1024 FFT
float32_t FFTOutputMagLeft[1024];
float32_t FFT_inputstruct_Right[2048]; // for 1024 FFT
float32_t FFTOutputMagRight[1024];
extern uint32_t fftSize;
extern uint32_t ifftFlag;
extern uint32_t doBitReverse;

// For Filter:
enum FilterType {
	NOF, // no filter
	ZRO, // zero the signal
	FIR, // finite impulse response
	IIR, // infinite impulse response
};
typedef enum FilterType filtertype_t;

int16_t y[2304 * 2 * 2] = { 0 };


//---------------------------------------------------------------------


// Write dynamic song information permanently on panel in lines 9 through 11
void Draw_Dynamic_Text (void)
{
	BSP_LCD_SetTextColor(LCD_COLOR_BLUE);
	sprintf(str, "Played out   %10u", (unsigned int) totalsum);
	BSP_LCD_DisplayStringAtLine(9, (uint8_t*) str);

	sprintf(str, "Frame        %10u", unFramesDecoded);
	BSP_LCD_DisplayStringAtLine(10, (uint8_t*) str);

	elapsed_time = (unFramesDecoded * 2304) / mp3FrameInfo.samprate;
	if(prev_elapsed_time != elapsed_time)
	{
		prev_elapsed_time = elapsed_time;
		sprintf((char *)str, "Elapsed Time [%02d:%02d:%02d]", (int)(elapsed_time /3600), (int)(elapsed_time /60) - (int)(elapsed_time /3600)*60, (int)(elapsed_time%60));
		BSP_LCD_DisplayStringAtLine(11, (uint8_t*) str);
	}
}

const char *get_filename_ext(const char *filename) {
	const char *dot = strrchr(filename, '.');
	if(!dot || dot == filename) return "";
	return dot + 1;
}

/**
 * @brief  Copies disk content in the explorer list.
 * @param  None
 * @retval Operation result
 */
FRESULT AUDIO_StorageParse(char * pathname)
{
	FRESULT res;
	FILINFO fno;
	DIR dir;
	char fn[FILEMGR_FILE_NAME_SIZE];

	printf ("***MP3PLAYER***: Parsing directory for MP3-files in path: %s\n\r", pathname);

	res = f_opendir(&dir, pathname);

	// do not set FileList.ptr = 0 if successive calls, e.g. first call with 1:/ and then second call with 0:/
	//  FileList.ptr = 0;

	if(res == FR_OK)
	{
		while (1)
		{
			res = f_readdir(&dir, &fno);

			if(res != FR_OK || fno.fname[0] == 0)
			{
				break;
			}
			if(fno.fname[0] == '.')
			{
				continue;
			}

			fn[0] = 0;
			strcpy (fn, pathname);
			//      printf ("path =%s= fn #%s#\n\r",pathname, fn);
			strcat (fn, fno.fname);
			//      printf ("FN =%s=\n\r",fn);

			if(FileList.ptr < FILEMGR_LIST_DEPDTH)
			{
				if((fno.fattrib & AM_DIR) == 0)
				{
					if((strstr(fn, "mp3")) || (strstr(fn, "MP3")))
					{//printf ("%03d-%010d-%s\n\r", FileList.ptr, fno.fsize, fno.fname);
						strncpy((char *)FileList.file[FileList.ptr].name, (char *)fn, FILEMGR_FILE_NAME_SIZE);
						FileList.file[FileList.ptr].size = fno.fsize;
						FileList.file[FileList.ptr].type = FILETYPE_FILE;
						FileList.ptr++;
					}
				}
			}
		}
	}
	NumObs = FileList.ptr;
	f_closedir(&dir);
	return res;
}

/**
 * @brief  Shows audio file on the root
 * @param  None
 * @retval None
 */
uint8_t AUDIO_ShowMP3Files(char* pathname)
{
	uint8_t i = 0;
	uint8_t line_idx = 0;
	//  if(AUDIO_StorageInit() == FR_OK)
	{
		if(AUDIO_StorageParse(pathname) ==  FR_OK)
		{
			if(FileList.ptr > 0)
			{
				printf("Audio file(s) [ROOT]:\n\r");

				for(i = 0; i < FileList.ptr; i++)
				{
					line_idx++;
					printf("   |__%03d__", i);
					printf((char *)FileList.file[i].name);
					printf("\n\r");
				}

				printf("End of files list.\n\r");
				return 0;
			}
			return 1;
		}
		return 2;
	}
	//  else
	//{
	//  return 3;
	//}
}

/**
 * @brief  Gets Wav Object Number.
 * @param  None
 * @retval None
 */
uint16_t AUDIO_GetMP3ObjectNumber(void)
{
	return NumObs;
}

uint8_t PlayerInit(uint32_t AudioFreq)
{
	uwVolume = 1;

	/* Initialize the Audio codec and all related peripherals (I2S, I2C, IOExpander, IOs...) */
	if(BSP_AUDIO_OUT_Init(OUTPUT_DEVICE_BOTH, uwVolume, AudioFreq) != 0)
	{
		return 1;
	}
	else
	{
		BSP_AUDIO_OUT_SetAudioFrameSlot(CODEC_AUDIOFRAME_SLOT_02);
		return 0;
	}
}

void mp3_init_play(char* filename, int filesizeinfo)
{
	int res2;

	bytes_left = FILE_READ_BUFFER_SIZE;
	read_ptr = file_read_buffer;

	printf ("\n\r-------------------------------------------------------------------------------------------\n\r***MP3PLAYER***: Init file to play.\n\r***MP3PLAYER***: Opening: %s \n\r", filename);
	res2 = f_open(&file, filename, FA_OPEN_EXISTING | FA_READ);

	printf ("***MP3PLAYER***: File System Opening Error: %02d \n\r", res2);

	if (res2 == FR_OK)
	{
		// Read ID3v2 Tag
		Mp3ReadId3V2Tag(&file, szArtist, sizeof(szArtist), szTitle, sizeof(szTitle));

		printf ("***MP3PLAYER***: tags read:     >artist< | >title< :  >%s< | >%s< \n\r", szArtist, szTitle);
		printf ("***MP3PLAYER***: filesize:  %d \n\r", filesizeinfo);

		BufferCtl.state = BUFFER_OFFSET_NONE;
		for(int i = 0 ; i < AUDIO_BUFFER_SIZE; i++) {BufferCtl.buff[i] = 0;}

		//		BufferCtl.fptr = 222;
		unFramesDecoded = 0;
		totalsum = 0;

		bytes_left = 0;
		btr = FILE_READ_BUFFER_SIZE - bytes_left;
		res = f_read(&file, file_read_buffer + bytes_left, btr, &br);
//		printf ("Read after initplay %d \r\n", res);

		totalsum = totalsum + br;
		bytes_left = FILE_READ_BUFFER_SIZE;
		memcpy(file_read_buffer, read_ptr, bytes_left);
		read_ptr = file_read_buffer;
		btr = 0;

		offset = MP3FindSyncWord((unsigned char*)read_ptr, bytes_left);
		bytes_left -= offset;
		read_ptr += offset;

		err = MP3Decode(hMP3Decoder, (unsigned char**)&read_ptr, (int*)&bytes_left,  (short*) &BufferCtl.buff[0], 0);
		MP3GetLastFrameInfo(hMP3Decoder, &mp3FrameInfo);

		printf ("***MP3PLAYER***: Decoder Error = %d\n\r", err);
		printf("***MP3PLAYER***: Bitrate=%d bps Samprate=%d Hz BitsPerSample=%d Bits Channels=%d\n\r\n\r", mp3FrameInfo.bitrate, mp3FrameInfo.samprate, mp3FrameInfo.bitsPerSample, mp3FrameInfo.nChans);

		PlayerInit(mp3FrameInfo.samprate);

		BSP_AUDIO_OUT_Play((uint16_t*)&BufferCtl.buff[0], AUDIO_BUFFER_SIZE); // start playing buffer content circular
		BSP_AUDIO_OUT_SetVolume(80);
	}
}

/*
 * Begin: Custom (helper) functions and types for the DSP MP3-Player project
 */

void prepare_fft_disp(void) {
	/* --------------------------------------------------/
	 *
	 * for displaying FFT: copy into FFT buffer
	 * and perform FFT for display  (buffer offset FULL)
	 *
	 * Note: we take an 1024-FFT of batches of 1152 samples.
	 * This is important for the frequency scaling:
	 * if fA = 44.1 kHz, then the 512th line ist at 22.05 kHz,
	 * but in our case it is at 22.05 * 1152/1024 = 24.80 kHz
	 *
	 * so in terms of pixels on the display: 248 lines are 10kHz,
	 * 20 kHz are at 496 lines
	 *
	 *
	 * --------------------------------------------------/
	 */
	memcpy(&FFTBuffer, &BufferCtl.buff, 2304 * 2);
	for (int i = 0; i < 1024; i++) {
		FFT_inputstruct_Left[2 * i] = FFTBuffer[2 * i]; // Left channel item only
		FFT_inputstruct_Left[2 * i + 1] = 0.0;             // complex component
		FFT_inputstruct_Right[2 * i] = FFTBuffer[2 * i + 1]; // Right channel item only
		FFT_inputstruct_Right[2 * i + 1] = 0.0;             // complex component
	}

	arm_cfft_f32(&arm_cfft_sR_f32_len1024, FFT_inputstruct_Left, ifftFlag,
			doBitReverse);
	arm_cmplx_mag_f32(FFT_inputstruct_Left, FFTOutputMagLeft, fftSize);

	arm_cfft_f32(&arm_cfft_sR_f32_len1024, FFT_inputstruct_Right, ifftFlag,
			doBitReverse);
	arm_cmplx_mag_f32(FFT_inputstruct_Right, FFTOutputMagRight, fftSize);
}

void mono_to_stereo(uint32_t offset) {
	if (mp3FrameInfo.nChans == 1) {
		for (int i = AUDIO_BUFFER_SIZE / 4; i >= 2; i = i - 2) {
			BufferCtl.buff[2 * i + 1 + offset] = BufferCtl.buff[i - 1 + offset]; // High Byte
			BufferCtl.buff[2 * i - 1 + offset] = BufferCtl.buff[i - 1 + offset]; // High Byte
			BufferCtl.buff[2 * i + 2 + offset] = BufferCtl.buff[i - 2 + offset]; // Low Byte
			BufferCtl.buff[2 * i - 0 + offset] = BufferCtl.buff[i - 2 + offset]; // Low Byte
		}
	}
}

/**
 * Zero the given array
 *
 * @param[in/out]   buf     pointer to the array
 * @param[in]       nelem   length of the array
 * @param[in]       elsize  size of elements in bytes
 */
void zero(void *buf, size_t nelem, size_t elsize) {
	memset(buf, 0, nelem * elsize);
}

/**
 * Negate a float array
 *
 * @param[in/out]   v       pointer to the array
 * @param[in]       vlen    length of the array
 */
void negate_floats(float *v, size_t vlen) {
	for (int i = 0; i < vlen; ++i)
		v[i] = -v[i];
}

/**
 * Add the stereo FIR sum of the signal in x with the filter function in b
 * to the output in y.
 *
 * @param[out]  y       pointer to the stereo output array
 * @param[in]   x       pointer to the stereo input array
 * @param[in]   len_x    length of the input/output array
 * @param[in]   b       pointer to the filter function array
 * @param[in]   len_b    length of the filter function array
 */
void add_stereo_fir(int16_t *y, int16_t *x, size_t len_x, float *b, size_t len_b) {
	// Todo: Implement an FIR filter here using the input x with filter function array b
	// and save the result in the output y.

}

/**
 * Apply the stereo FIR filter to the given signal in x and save the result
 * to y, the filter function is given in b.
 *
 * @param[out]  y       pointer to the stereo output array
 * @param[in]   x       pointer to the stereo input array
 * @param[in]   len_x    length of the input/output array
 * @param[in]   b       pointer to the filter function array
 * @param[in]   len_b    length of the filter function array
 */
void apply_stereo_fir_filter(int16_t *y, int16_t *x, size_t len_x, float *b,
		size_t len_b) {
	// zero the output array
	zero(y, len_x, sizeof(int16_t));

	// add the stereo FIR filter to the output array
	add_stereo_fir(y, x, len_x, b, len_b);
}

/**
 * Apply the stereo IIR filter to the given signal in x and save the result
 * to y, the filter functions are given in b and a. Note that this is not
 * 100% compliant with the definition of the IIR in tutorial 13, because
 * there the second sum's index starts at 1 rather than 0. This can the
 * emulated by appending another zero to a, and incrementing len_a by one.
 *
 * @param[out]  y       pointer to the stereo output array
 * @param[in]   x       pointer to the stereo input array
 * @param[in]   len_x    length of the input/output array
 * @param[in]   b       pointer to the first filter function array
 * @param[in]   len_b    length of the first filter function array
 * @param[in]   a       pointer to the second filter function array
 * @param[in]   len_b    length of the second filter function array
 */
void apply_stereo_iir_filter(int16_t *y, int16_t *x, size_t len_x, float *b,
		size_t len_b, float *a, size_t len_a) {
	// Todo: Implement an IIR filter here using the input x with filter function array b
	// and save the result in the output y.
	// Hint: You should first implement the FIR filter. You may also find some of the
	//       functions from above useful for implementing this filter here.

}

/*
 * End: Custom (helper) functions and types for the DSP MP3-Player project
 */

// Decode contents of file_read_buffer into AUDIO_BUFFER. Each decode step produces
// 1152 samples (in the case of stereo), i.e. 1152 * 2 chans * 2 bytes for each sample ( = 4608 bytes).
// AUDIOBUFFER_SIZE is twice the size, so that while the upper half is consumed by the Audio DMA,
// the lower half is refilled with decoder results and vice versa.
// If MP3 data stream is Mono, then there are only 576 samples output by the Helix.
// This is remedied by duplicating these samples and stretching over the whole Audio buffer,
// starting from the top (the top half is empty and needs to be filled up),
// i.e. buff[AUDIO_BUFFER_SIZE/4] copied to ----> buff[AUDIO_BUFFER_SIZE/2] and buff[AUDIO_BUFFER_SIZE/2 - 2 bytes] if lower half gets filled.
//  this gets repeated downwards. In the case of the upper half getting filled (and the lower half being played out), the process starts at
// 3/4*AUDIO_BUFFER_SIZE
uint8_t Audio_Process (void) {

	int index_into_audioutputbuffer;

	// --- Refill Filebuffer ... and copy into AudioBuffer
	//----------------------------------------------------
	File_Playing_State = FILE_PLAYING;

	if (bytes_left < (FILE_READ_BUFFER_SIZE / 2))
	{
		// Copy rest of data to beginning of read buffer
		memcpy(file_read_buffer, read_ptr, bytes_left);

		// Update read pointer for audio sampling
		read_ptr = file_read_buffer;

		// Read next part of file
		btr = FILE_READ_BUFFER_SIZE - bytes_left;
		//printf ("To Read %d \r\n", res);
		res = f_read(&file, file_read_buffer + bytes_left, btr, &br);
		//printf ("Gelesen %d \r\n", res);
		totalsum = totalsum + br;

		// Update the bytes left variable
		bytes_left = FILE_READ_BUFFER_SIZE;

		// Out of data or error: Stop playback!
		if (br < btr || res != FR_OK )
		{
			f_close(&file);
			BSP_AUDIO_OUT_SetVolume(0);
			BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW);
			File_Playing_State = FILE_ENDED;
		}
	}

	// --- Decode 4608 Bytes (1152 samples output by MP3 decoder) in one step once indicated through
	// --- DMA interrupts that they have been consumed (= played out)
	//-----------------------------------------------------------------------------------------------

	if(BufferCtl.state) // Half_Full or Full
	{
		if (BufferCtl.state == BUFFER_OFFSET_FULL) index_into_audioutputbuffer = AUDIO_BUFFER_SIZE/2;  // Write MP3 Decoder results into BufferCtl.buff interval [AUDIO_BUFFER_SIZE/2 ... AUDIO_BUFFER_SIZE]. Also used for Mono Correction
		if (BufferCtl.state == BUFFER_OFFSET_HALF) index_into_audioutputbuffer = 0; // Write MP3 Decoder results into BufferCtl.buff interval [0 ... AUDIO_BUFFER_SIZE/2]

		offset = MP3FindSyncWord((unsigned char*)read_ptr, bytes_left);
		bytes_left -= offset;
		read_ptr += offset;

		err = MP3Decode(hMP3Decoder, (unsigned char**)&read_ptr, (int*)&bytes_left,  (short*) &BufferCtl.buff[index_into_audioutputbuffer], 0);
		MP3GetLastFrameInfo(hMP3Decoder, &mp3FrameInfo);
		unFramesDecoded++;

		// Correction for Mono
	  	mono_to_stereo(index_into_audioutputbuffer);


	  	/*
		 * Begin: Code for DSP MP3-Player project
		 */

		// define variables and filter functions
		filtertype_t filtertype = FIR; 	// choose between:
										//   NOF: no filter,
										//   FIR: finite impulse response filter,
										//   IIR: infinite impulse response filter
										//   ZRO: zero the signal
		int16_t *x; 					// stereo input for the filters
		size_t len_x, len_b, len_a; 	// the lengths of the different arrays
		len_x = AUDIO_BUFFER_SIZE / 4; 	// (BufferCtl.state == BUFFER_OFFSET_HALF ? 2 : 4);
										// we cast the byte array to int16_t,
										// therefore the length halves

		// ToDo: Define two float arrays (b, a) with the filter functions
		// - b for FIR and first sum of IRR,
		// - a for the second sum of the IIR



		// compliance with the IIR definition in tutorial 13
		// we cast the signal buffer to int16_t *
		//
		// --- Data organization of BufferCtl.buff:
		// ---
		// --- [channel-l_high-byte, channel-l_low-byte, channel-r_high-byte, channel-r_low-byte,
		// ---							... , channel-r_high-byte, channel-r_low-byte]
		// ---
		// --- with channel l, r being the left and right channel, respectively.
		if (BufferCtl.state == BUFFER_OFFSET_HALF)
		{
			x = (int16_t *) &BufferCtl.buff[0];
		}
		else {
			x = (int16_t *) &BufferCtl.buff[AUDIO_BUFFER_SIZE / 2];
		}

		// clear memory
		if (filtertype != NOF)
			zero(y, len_x, sizeof(int16_t));

		// apply the filter
		if (filtertype == FIR)
			apply_stereo_fir_filter(y, x, len_x, b, len_b);
		else if (filtertype == IIR)
			apply_stereo_iir_filter(y, x, len_x, b, len_b, a, len_a);
		else if (filtertype == ZRO)
			memset(y, 0, len_x * 2 * sizeof(uint8_t));

		// write back the result
		if (filtertype != NOF)
			memcpy(x, y, len_x * sizeof(int16_t));

		/*
		 * End: Code for DSP MP3-Player project
		 */

		prepare_fft_disp();

		BufferCtl.state = BUFFER_OFFSET_NONE;
	}
	return err; //MP3 error
}



/*
 * Taken from
 * http://www.mikrocontroller.net/topic/252319
 */

uint32_t Mp3ReadId3V2Text(FIL* pInFile, uint32_t unDataLen, char* pszBuffer, uint32_t unBufferSize)
{
	UINT unRead = 0;
	BYTE byEncoding = 0;
	if((f_read(pInFile, &byEncoding, 1, &unRead) == FR_OK) && (unRead == 1))
	{
		unDataLen--;
		if(unDataLen <= (unBufferSize - 1))
		{
			if((f_read(pInFile, pszBuffer, unDataLen, &unRead) == FR_OK) ||
					(unRead == unDataLen))
			{
				if(byEncoding == 0)
				{
					// ISO-8859-1 multibyte
					// just add a terminating zero
					pszBuffer[unDataLen] = 0;
				}
				else if(byEncoding == 1)
				{
					// UTF16LE unicode
					uint32_t r = 0;
					uint32_t w = 0;
					if((unDataLen > 2) && (pszBuffer[0] == 0xFF) && (pszBuffer[1] == 0xFE))
					{
						// ignore BOM, assume LE
						r = 2;
					}
					for(; r < unDataLen; r += 2, w += 1)
					{
						// should be acceptable for 7 bit ascii
						pszBuffer[w] = pszBuffer[r];
					}
					pszBuffer[w] = 0;
				}
			}
			else
			{
				return 1;
			}
		}
		else
		{
			// we won't read a partial text
			if(f_lseek(pInFile, f_tell(pInFile) + unDataLen) != FR_OK)
			{
				return 1;
			}
		}
	}
	else
	{
		return 1;
	}
	return 0;
}

/*
 * Taken from
 * http://www.mikrocontroller.net/topic/252319
 */
uint32_t Mp3ReadId3V2Tag(FIL* pInFile, char* pszArtist, uint32_t unArtistSize, char* pszTitle, uint32_t unTitleSize)
{
	pszArtist[0] = 0;
	pszTitle[0] = 0;

	BYTE id3hd[10];
	UINT unRead = 0;
	if((f_read(pInFile, id3hd, 10, &unRead) != FR_OK) || (unRead != 10))
	{
		return 1;
	}
	else
	{
		uint32_t unSkip = 0;
		if((unRead == 10) &&
				(id3hd[0] == 'I') &&
				(id3hd[1] == 'D') &&
				(id3hd[2] == '3'))
		{
			unSkip += 10;
			unSkip = ((id3hd[6] & 0x7f) << 21) | ((id3hd[7] & 0x7f) << 14) | ((id3hd[8] & 0x7f) << 7) | (id3hd[9] & 0x7f);

			// try to get some information from the tag
			// skip the extended header, if present
			uint8_t unVersion = id3hd[3];
			if(id3hd[5] & 0x40)
			{
				BYTE exhd[4];
				f_read(pInFile, exhd, 4, &unRead);
				size_t unExHdrSkip = ((exhd[0] & 0x7f) << 21) | ((exhd[1] & 0x7f) << 14) | ((exhd[2] & 0x7f) << 7) | (exhd[3] & 0x7f);
				unExHdrSkip -= 4;
				if(f_lseek(pInFile, f_tell(pInFile) + unExHdrSkip) != FR_OK)
				{
					return 1;
				}
			}
			uint32_t nFramesToRead = 2;
			while(nFramesToRead > 0)
			{
				char frhd[10];
				if((f_read(pInFile, frhd, 10, &unRead) != FR_OK) || (unRead != 10))
				{
					return 1;
				}
				if((frhd[0] == 0) || (strncmp(frhd, "3DI", 3) == 0))
				{
					break;
				}
				char szFrameId[5] = {0, 0, 0, 0, 0};
				memcpy(szFrameId, frhd, 4);
				uint32_t unFrameSize = 0;
				uint32_t i = 0;
				for(; i < 4; i++)
				{
					if(unVersion == 3)
					{
						// ID3v2.3
						unFrameSize <<= 8;
						unFrameSize += frhd[i + 4];
					}
					if(unVersion == 4)
					{
						// ID3v2.4
						unFrameSize <<= 7;
						unFrameSize += frhd[i + 4] & 0x7F;
					}
				}

				if(strcmp(szFrameId, "TPE1") == 0)
				{
					// artist
					if(Mp3ReadId3V2Text(pInFile, unFrameSize, pszArtist, unArtistSize) != 0)
					{
						break;
					}
					nFramesToRead--;
				}
				else if(strcmp(szFrameId, "TIT2") == 0)
				{
					// title
					if(Mp3ReadId3V2Text(pInFile, unFrameSize, pszTitle, unTitleSize) != 0)
					{
						break;
					}
					nFramesToRead--;
				}
				else
				{
					if(f_lseek(pInFile, f_tell(pInFile) + unFrameSize) != FR_OK)
					{
						return 1;
					}
				}
			}
		}
		if(f_lseek(pInFile, unSkip) != FR_OK)
		{
			return 1;
		}
	}

	return 0;
}


/**
 * @brief  Manages the full Transfer complete event.
 * @param  None
 * @retval None
 */
void BSP_AUDIO_OUT_TransferComplete_CallBack(void)
{
	BufferCtl.state = BUFFER_OFFSET_FULL;
	Audio_Process();
}

/**
 * @brief  Manages the DMA Half Transfer complete event.
 * @param  None
 * @retval None
 */
void BSP_AUDIO_OUT_HalfTransfer_CallBack(void)
{
	BufferCtl.state = BUFFER_OFFSET_HALF;
	Audio_Process();
}


/**
 * @brief  Manages the DMA FIFO error event.
 * @param  None
 * @retval None
 */
void BSP_AUDIO_OUT_Error_CallBack(void)
{
	/* Display message on the LCD screen */
	BSP_LCD_SetBackColor(LCD_COLOR_RED);
	BSP_LCD_DisplayStringAtLine(14, (uint8_t *)"       DMA  ERROR     ");

	/* Stop the program with an infinite loop */
	while (BSP_PB_GetState(BUTTON_KEY) != RESET)
	{
		return;
	}

	/* could also generate a system reset to recover from the error */
	/* .... */
}
