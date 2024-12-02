/* Copyright (C) Technical University of Munich - All Rights Reserved
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * Written by Alois Knoll <knoll@in.tum.de>, 2021
 */

#include "wallclock.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "math.h"

#include "stm32746g_discovery_lcd.h"
#include "stm32746g_discovery.h"

#include "mp3player.h"

const double pi = 3.14159265358979323846264338327950288;

#define LINE_OVERLAP_NONE 0 	// No line overlap
#define LINE_OVERLAP_MAJOR 0x01 // Overlap - first go major then minor direction
#define LINE_OVERLAP_MINOR 0x02 // Overlap - first go minor then major direction
#define LINE_OVERLAP_BOTH 0x03  // Overlap - both

#define LINE_THICKNESS_MIDDLE 0
#define LINE_THICKNESS_DRAW_CLOCKWISE 1
#define LINE_THICKNESS_DRAW_COUNTERCLOCKWISE 2

#define DISPLAY_WIDTH 480
#define DISPLAY_HEIGHT 272

#define x_offset 10 // for FFT graphics block shift to the right
#define ytop  30
#define ybottom  270
#define height (ybottom-ytop)

// Center
const short xs = 400;
const short ys = 187;

// Minute Marks
const short r_in_min = 62;
const short r_out_min = 66;
const short thick_one = 2;

// Five Minute Marks
const short r_in_five = 56;
const short r_out_five = 66;
const short thick_five = 3;

// Seconds hand
const short r_sec = 64;
const short thick_sec = 3;

// Minutes hand
const short r_min = 64;
const short thick_min = 4;

// Hours hand
const short r_hour = 48;
const short thick_hour = 4;

int Hour1, Hour2, Min1, Min2, Sec1, Sec2;


struct ThickLine {
	int16_t StartX;
	int16_t StartY;
	int16_t EndX;
	int16_t EndY;
	int16_t Thickness;
	uint8_t ThicknessMode;
	uint16_t Color;
	int16_t BackgroundColor;
};

extern sFONT Font24;
extern sFONT Font20;
extern sFONT Font16;
extern sFONT Font12;
extern sFONT Font8;

float r;
float si, co, si2, co2, mem_min_si , mem_min_co, mem_hour_si , mem_hour_co,
	   mem_min_si2 , mem_min_co2, mem_hour_si2 , mem_hour_co2;
int r_in, r_out;
int i;
int Hour1, Hour2, Min1, Min2, Sec1, Sec2;
int left, left1, left2, right, right1, right2;
float siVU, coVU, siVU2, coVU2;
volatile int x, yy1, yy2, yy, vv, vv1;

// ******  memory variable for VU Meter
float memoryVU_L, memoryVU_R;



extern volatile int
 		tmp11, tmp22, XTAL_Min, XTAL_Min1, XTAL_Min2,
 		XTAL_Hour, XTAL_Hour1, XTAL_Hour2,
 		XTAL_Sec, XTAL_Sec1, XTAL_Sec2,
 		XTAL_Day, XTAL_Day1, XTAL_Day2,
 		XTAL_Month, XTAL_Year, XTAL_Weekday, XTAL_MESZ,
		First_XTAL_Min, First_XTAL_Hour,
		Leap_year, XTAL_MSEC, stopped_at_sec, XTAL_Tmp;

// Taken from Hennning Karlsen's UTFT library: http://www.rinkydinkelectronics.com/library.php?id=51

void LCD_DrawThickLine(int x1, int y1, int x2, int y2, uint16_t color)
{
		unsigned int	dx = (x2 > x1 ? x2 - x1 : x1 - x2);
		short			xstep =  x2 > x1 ? 1 : -1;
		unsigned int	dy = (y2 > y1 ? y2 - y1 : y1 - y2);
		short			ystep =  y2 > y1 ? 1 : -1;
		int				col = x1, row = y1;

		if (dx < dy)
		{
			int t = - (dy >> 1);
			while (1)
			{
				BSP_LCD_DrawPixel (col,   row,   color);
				BSP_LCD_DrawPixel (col-1, row,   color);
				BSP_LCD_DrawPixel (col+1, row,   color);
				BSP_LCD_DrawPixel (col,   row-1, color);
				BSP_LCD_DrawPixel (col-1, row-1, color);
				BSP_LCD_DrawPixel (col+1, row-1, color);
				BSP_LCD_DrawPixel (col,   row+1, color);
				BSP_LCD_DrawPixel (col-1, row+1, color);
				BSP_LCD_DrawPixel (col+1, row+1, color);
				if (row == y2)
					return;
				row += ystep;
				t += dx;
				if (t >= 0)
				{
					col += xstep;
					t   -= dy;
				}
			}
		}
		else
		{
			int t = - (dx >> 1);
			while (1)
			{
				BSP_LCD_DrawPixel (col,   row,   color);
				BSP_LCD_DrawPixel (col-1, row,   color);
				BSP_LCD_DrawPixel (col+1, row,   color);
				BSP_LCD_DrawPixel (col,   row-1, color);
				BSP_LCD_DrawPixel (col-1, row-1, color);
				BSP_LCD_DrawPixel (col+1, row-1, color);
				BSP_LCD_DrawPixel (col,   row+1, color);
				BSP_LCD_DrawPixel (col-1, row+1, color);
				BSP_LCD_DrawPixel (col+1, row+1, color);
				if (col == x2)
					return;
				col += xstep;
				t += dy;
				if (t >= 0)
				{
					row += ystep;
					t   -= dx;
				}
			}
		}

	}

// Taken from https://github.com/ArminJo/STMF3-Discovery-Demos/blob/master/lib/graphics/src/thickLine.cpp

void drawLineOverlap(int16_t aXStart, int16_t aYStart, int16_t aXEnd, int16_t aYEnd, uint8_t aOverlap, uint32_t aColor) {
	int16_t tDeltaX, tDeltaY, tDeltaXTimes2, tDeltaYTimes2, tError, tStepX, tStepY;

	if (aXStart >= DISPLAY_WIDTH) {
		aXStart = DISPLAY_WIDTH - 1;
	}
	if (aXStart < 0) {
		aXStart = 0;
	}
	if (aXEnd >= DISPLAY_WIDTH) {
		aXEnd = DISPLAY_WIDTH - 1;
	}
	if (aXEnd < 0) {
		aXEnd = 0;
	}
	if (aYStart >= DISPLAY_HEIGHT) {
		aYStart = DISPLAY_HEIGHT - 1;
	}
	if (aYStart < 0) {
		aYStart = 0;
	}
	if (aYEnd >= DISPLAY_HEIGHT) {
		aYEnd = DISPLAY_HEIGHT - 1;
	}
	if (aYEnd < 0) {
		aYEnd = 0;
	}

	{
		//calculate direction
		tDeltaX = aXEnd - aXStart;
		tDeltaY = aYEnd - aYStart;
		if (tDeltaX < 0) {
			tDeltaX = -tDeltaX;
			tStepX = -1;
		} else {
			tStepX = +1;
		}
		if (tDeltaY < 0) {
			tDeltaY = -tDeltaY;
			tStepY = -1;
		} else {
			tStepY = +1;
		}
		tDeltaXTimes2 = tDeltaX << 1;
		tDeltaYTimes2 = tDeltaY << 1;
		//draw start pixel
		BSP_LCD_DrawPixel(aXStart, aYStart, aColor);
		if (tDeltaX > tDeltaY) {
			// start value represents a half step in Y direction
			tError = tDeltaYTimes2 - tDeltaX;
			while (aXStart != aXEnd) {
				// step in main direction
				aXStart += tStepX;
				if (tError >= 0) {
					if (aOverlap & LINE_OVERLAP_MAJOR) {
						// draw pixel in main direction before changing
						BSP_LCD_DrawPixel(aXStart, aYStart, aColor);
					}
					// change Y
					aYStart += tStepY;
					if (aOverlap & LINE_OVERLAP_MINOR) {
						// draw pixel in minor direction before changing
						BSP_LCD_DrawPixel(aXStart - tStepX, aYStart, aColor);
					}
					tError -= tDeltaXTimes2;
				}
				tError += tDeltaYTimes2;
				BSP_LCD_DrawPixel(aXStart, aYStart, aColor);
			}
		} else {
			tError = tDeltaXTimes2 - tDeltaY;
			while (aYStart != aYEnd) {
				aYStart += tStepY;
				if (tError >= 0) {
					if (aOverlap & LINE_OVERLAP_MAJOR) {
						// draw pixel in main direction before changing
						BSP_LCD_DrawPixel(aXStart, aYStart, aColor);
					}
					aXStart += tStepX;
					if (aOverlap & LINE_OVERLAP_MINOR) {
						// draw pixel in minor direction before changing
						BSP_LCD_DrawPixel(aXStart, aYStart - tStepY, aColor);
					}
					tError -= tDeltaYTimes2;
				}
				tError += tDeltaXTimes2;
				BSP_LCD_DrawPixel(aXStart, aYStart, aColor);
			}
		}
	}
}

/**
 * Bresenham with thickness
 * Taken from https://github.com/ArminJo/STMF3-Discovery-Demos/blob/master/lib/graphics/src/thickLine.cpp
 * no pixel missed and every pixel only drawn once!
 */

// Thickness mode ist: nur rechts (1) oder mittig (0) werden Linien angef�gt
void drawThickLine(int16_t aXStart, int16_t aYStart, int16_t aXEnd, int16_t aYEnd, int16_t aThickness, uint8_t aThicknessMode,
		uint32_t aColor) {
	int16_t i, tDeltaX, tDeltaY, tDeltaXTimes2, tDeltaYTimes2, tError, tStepX, tStepY;



	if(aThickness <= 1) {
//		drawLineOverlap(aXStart, aYStart, aXEnd, aYEnd, LINE_OVERLAP_NONE, aColor);
	}
	/*
	 * Clip to display size
	 */
	if (aXStart >= DISPLAY_WIDTH) {
		aXStart = DISPLAY_WIDTH - 1;
	}
	if (aXStart < 0) {
		aXStart = 0;
	}
	if (aXEnd >= DISPLAY_WIDTH) {
		aXEnd = DISPLAY_WIDTH - 1;
	}
	if (aXEnd < 0) {
		aXEnd = 0;
	}
	if (aYStart >= DISPLAY_HEIGHT) {
		aYStart = DISPLAY_HEIGHT - 1;
	}
	if (aYStart < 0) {
		aYStart = 0;
	}
	if (aYEnd >= DISPLAY_HEIGHT) {
		aYEnd = DISPLAY_HEIGHT - 1;
	}
	if (aYEnd < 0) {
		aYEnd = 0;
	}

	/**
	 * For coordinate system with 0.0 topleft
	 * Swap X and Y delta and calculate clockwise (new delta X inverted)
	 * or counterclockwise (new delta Y inverted) rectangular direction.
	 * The right rectangular direction for LINE_OVERLAP_MAJOR toggles with each octant
	 */
	tDeltaY = aXEnd - aXStart;
	tDeltaX = aYEnd - aYStart;
	// mirror 4 quadrants to one and adjust deltas and stepping direction
	uint8_t tSwap = 1; // count effective mirroring
	if (tDeltaX < 0) {
		tDeltaX = -tDeltaX;
		tStepX = -1;
		tSwap = !tSwap;
	} else {
		tStepX = +1;
	}
	if (tDeltaY < 0) {
		tDeltaY = -tDeltaY;
		tStepY = -1;
		tSwap = !tSwap;
	} else {
		tStepY = +1;
	}
	tDeltaXTimes2 = tDeltaX << 1;
	tDeltaYTimes2 = tDeltaY << 1;
	uint8_t tOverlap;
	// adjust for right direction of thickness from line origin
	int tDrawStartAdjustCount = aThickness / 2;
	if (aThicknessMode == LINE_THICKNESS_DRAW_COUNTERCLOCKWISE) {
		tDrawStartAdjustCount = aThickness - 1;
	} else if (aThicknessMode == LINE_THICKNESS_DRAW_CLOCKWISE) {
		tDrawStartAdjustCount = 0;
	}

	// which octant are we now
	if (tDeltaX >= tDeltaY) {
		if (tSwap) {
			tDrawStartAdjustCount = (aThickness - 1) - tDrawStartAdjustCount;
			tStepY = -tStepY;
		} else {
			tStepX = -tStepX;
		}
		/*
		 * Vector for draw direction of lines is rectangular and counterclockwise to original line
		 * Therefore no pixel will be missed if LINE_OVERLAP_MAJOR is used
		 * on changing in minor rectangular direction
		 */
		// adjust draw start point
		tError = tDeltaYTimes2 - tDeltaX;
		for (i = tDrawStartAdjustCount; i > 0; i--) {
			// change X (main direction here)
			aXStart -= tStepX;
			aXEnd -= tStepX;
			if (tError >= 0) {
				// change Y
				aYStart -= tStepY;
				aYEnd -= tStepY;
				tError -= tDeltaXTimes2;
			}
			tError += tDeltaYTimes2;
		}
		//draw start line
		BSP_LCD_SetTextColor(aColor);
		BSP_LCD_DrawLine(aXStart, aYStart, aXEnd, aYEnd);
		// draw aThickness lines
		tError = tDeltaYTimes2 - tDeltaX;
		for (i = aThickness; i > 1; i--) {
			// change X (main direction here)
			aXStart += tStepX;
			aXEnd += tStepX;
			tOverlap = LINE_OVERLAP_NONE;
			if (tError >= 0) {
				// change Y
				aYStart += tStepY;
				aYEnd += tStepY;
				tError -= tDeltaXTimes2;
				/*
				 * change in minor direction reverse to line (main) direction
				 * because of chosing the right (counter)clockwise draw vector
				 * use LINE_OVERLAP_MAJOR to fill all pixel
				 *
				 * EXAMPLE:
				 * 1,2 = Pixel of first lines
				 * 3 = Pixel of third line in normal line mode
				 * - = Pixel which will be drawn in LINE_OVERLAP_MAJOR mode
				 *           33
				 *       3333-22
				 *   3333-222211
				 * 33-22221111
				 *  221111                     /\
					 *  11                          Main direction of draw vector
				 *  -> Line main direction
				 *  <- Minor direction of counterclockwise draw vector
				 */
				tOverlap = LINE_OVERLAP_MAJOR;
			}
			tError += tDeltaYTimes2;
			drawLineOverlap(aXStart, aYStart, aXEnd, aYEnd, tOverlap, aColor);
		}
	} else {
		// the other octant
		if (tSwap) {
			tStepX = -tStepX;
		} else {
			tDrawStartAdjustCount = (aThickness - 1) - tDrawStartAdjustCount;
			tStepY = -tStepY;
		}
		// adjust draw start point
		tError = tDeltaXTimes2 - tDeltaY;
		for (i = tDrawStartAdjustCount; i > 0; i--) {
			aYStart -= tStepY;
			aYEnd -= tStepY;
			if (tError >= 0) {
				aXStart -= tStepX;
				aXEnd -= tStepX;
				tError -= tDeltaYTimes2;
			}
			tError += tDeltaXTimes2;
		}
		//draw start line
		BSP_LCD_SetTextColor(aColor);
		BSP_LCD_DrawLine(aXStart, aYStart, aXEnd, aYEnd);
		tError = tDeltaXTimes2 - tDeltaY;
		for (i = aThickness; i > 1; i--) {
			aYStart += tStepY;
			aYEnd += tStepY;
			tOverlap = LINE_OVERLAP_NONE;
			if (tError >= 0) {
				aXStart += tStepX;
				aXEnd += tStepX;
				tError -= tDeltaYTimes2;
				tOverlap = LINE_OVERLAP_MAJOR;
			}
			tError += tDeltaXTimes2;
			drawLineOverlap(aXStart, aYStart, aXEnd, aYEnd, tOverlap, aColor);
		}
	}
}

void panel100 (int Hour, int Min, int Sec)
{
	Sec1 = Sec2; Sec2 = Sec;
	Min1 = Min2; Min2 = Min;
	Hour1 = Hour2; Hour2 = Hour;

	// Minute can change if there is a Sync WITHOUT change of second!

		{
			// Five minute marks
			for (i = 0; i < 360; i += 30) {
				si = sin((i / 180.0) * pi);
				co = cos((i / 180.0) * pi);
				if (i == 90) {
					si = 1;
					co = 0;
				}
				drawThickLine(xs - r_in_five * si, ys + r_in_five * co,
						xs - r_out_five * si, ys + r_out_five * co, thick_five, 0,
						LCD_COLOR_BLACK);
			};

			// Hour hand
			si = r_hour * sin(((30 * Hour1 / 180.0) * pi + Min1 / 360.0 * pi));
			co = r_hour * cos(((30 * Hour1 / 180.0) * pi + Min1 / 360.0 * pi));
			si2 = r_hour * sin(((30 * Hour2 / 180.0) * pi + Min2 / 360.0 * pi));
			co2 = r_hour * cos(((30 * Hour2 / 180.0) * pi + Min2 / 360.0 * pi));

			if ((si < 2.0) & (si > -2.0))
				si = 0.0;
			if ((si2 < 2.0) & (si2 > -2.0))
				si2 = 0.0;
			if ((co < 2.0) & (co > -2.0))
				co = 0.0;
			if ((co2 < 2.0) & (co2 > -2.0))
				co2 = 0.0;

			mem_hour_si = si; // store position for drawing further down
			mem_hour_co = co;
			mem_hour_si2 = si2; // store position for drawing further down
			mem_hour_co2 = co2;

			// Minute Hand
			si = r_min * sin((6 * Min1 / 180.0) * pi);
			co = r_min * cos((6 * Min1 / 180.0) * pi);
			si2 = r_min * sin((6 * Min2 / 180.0) * pi);
			co2 = r_min * cos((6 * Min2 / 180.0) * pi);

			// Correction of rounding errors
			if ((si < 2.0) & (si > -2.0))
				si = 0.0;
			if ((si2 < 2.0) & (si2 > -2.0))
				si2 = 0.0;
			if ((co < 2.0) & (co > -2.0))
				co = 0.0;
			if ((co2 < 2.0) & (co2 > -2.0))
				co2 = 0.0;

			mem_min_si2 = si2; // store position for drawing further down
			mem_min_co2 = co2;
			mem_min_si = si; // store position for drawing further down
			mem_min_co = co;

			// Seconds hand
			si = r_sec * sin((6 * Sec1 / 180.0) * pi);
			co = r_sec * cos((6 * Sec1 / 180.0) * pi);
			si2 = r_sec * sin((6 * Sec2 / 180.0) * pi);
			co2 = r_sec * cos((6 * Sec2 / 180.0) * pi);

			if ((si < 2.0) & (si > -2.0))
				si = 0.0;
			if ((si2 < 2.0) & (si2 > -2.0))
				si2 = 0.0;
			if ((co < 2.0) & (co > -2.0))
				co = 0.0;
			if ((co2 < 2.0) & (co2 > -2.0))
				co2 = 0.0;

			// Delete old Position seconds hand
			drawThickLine(xs, ys, xs + si, ys - co, 3, 0, LCD_COLOR_WHITE);
			BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
			BSP_LCD_DrawCircle(xs, ys, thick_sec);

			// Draw seconds hand
			drawThickLine(xs, ys, xs + si2, ys - co2, thick_sec, 0,
			LCD_COLOR_RED);

			// Five Minute Marks
			for (i = 0; i < 360; i += 30) {
				si = sin((i / 180.0) * pi);
				co = cos((i / 180.0) * pi);
				if (i == 90) {
					si = 1;
					co = 0;
				}
				drawThickLine(xs - r_in_five * si, ys + r_in_five * co,
						xs - r_out_five * si, ys + r_out_five * co, thick_five, 0,
						LCD_COLOR_BLACK);
			};

			// Delete minute hand
			if ((Sec2 % 10) == 0)
				drawThickLine(xs, ys, xs + mem_min_si, ys - mem_min_co, thick_min,
						0, LCD_COLOR_WHITE);

			// Draw Minute hand over deleted Position
			drawThickLine(xs, ys, xs + mem_min_si2, ys - mem_min_co2, thick_min, 0,
			LCD_COLOR_BLACK);

			// Delete hour hand
			if ((Sec2 % 10) == 0)
				drawThickLine(xs, ys, xs + mem_hour_si, ys - mem_hour_co,
						thick_hour, 0, LCD_COLOR_WHITE);

			// Draw Hour hand over deleted Position
			drawThickLine(xs, ys, xs + mem_hour_si2, ys - mem_hour_co2, thick_hour,
					0, LCD_COLOR_BLACK);

			BSP_LCD_SetTextColor(LCD_COLOR_RED); // BLACK
			BSP_LCD_FillCircle(xs, ys, 4);

			// Draw seconds hand
			drawThickLine(xs, ys, xs + si2, ys - co2, thick_sec, 0,
			LCD_COLOR_RED);

			// Five minute marks
			for (i = 0; i < 360; i += 30) {
				si = sin((i / 180.0) * pi);
				co = cos((i / 180.0) * pi);
				if (i == 90) {
					si = 1;
					co = 0;
				}
				drawThickLine(xs - r_in_five * si, ys + r_in_five * co,
						xs - r_out_five * si, ys + r_out_five * co, thick_five, 0,
						LCD_COLOR_BLACK);
			};

			// One minute marks
			for (i = 0; i < 360; i += 6) {
				si = sin((i / 180.0) * pi);
				co = cos((i / 180.0) * pi);
				drawThickLine(xs - r_in_min * si, ys + r_in_min * co,
						xs - r_out_min * si, ys + r_out_min * co, thick_one, 0,
						LCD_COLOR_BLACK);
			};

			// Draw seconds hand
			drawThickLine(xs, ys, xs + si2, ys - co2, thick_sec, 0,
			LCD_COLOR_RED);
		};
}


void prep_panel100 (void)
{
	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	BSP_LCD_DrawCircle(xs, ys, r_out_five + 1);

	BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
	BSP_LCD_FillCircle(xs, ys, r_out_five - 1);

	BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
	BSP_LCD_DrawCircle(xs, ys, r_out_five);

	// One minute marks
	for (i = 0; i < 360; i += 6) {
		si = sin((i / 180.0) * pi);
		co = cos((i / 180.0) * pi);
		drawThickLine(xs - r_in_min * si, ys + r_in_min * co,
				xs - r_out_min * si, ys + r_out_min * co, thick_one, 0,
				LCD_COLOR_BLACK);
	};
}

/* *************************************************************************************
 * *************************************************************************************
 * Draw Spectrum, i.e. FFT graph as computed in mp3player.c
 * Note correct scaling only for 48 kHz sampling frequency!
 */
void Draw_Spectrum (void)
{     int xx, zz;

	  BSP_LCD_SetTextColor (LCD_COLOR_GRAY);
	  BSP_LCD_DrawVLine(x_offset  , ytop, height); //left line
	  BSP_LCD_DrawVLine(x_offset+1, ytop, height); //left line
	  BSP_LCD_DrawVLine(x_offset+2, ytop, height); //left line
	  BSP_LCD_DrawVLine(x_offset+3, ytop, height); //left line

	  for (xx = 1; xx <= 450/2; xx++) // 450/512 * maxfreq/2, i.e., 48kHz * 0.5 * 450/512 = 21 kHz
	  {
		  yy = (int) (  (FFTOutputMagLeft  [2*xx])  / 120000.0 * height / 20.0);
		  zz = (int) (  (FFTOutputMagRight [2*xx])  / 120000.0 * height / 20.0);

		  if (yy < 0) yy = 0;
		  if (yy > height) // if overflow: limit and display red line
		  {
			  yy = height;
			  BSP_LCD_SetTextColor (LCD_COLOR_WHITE);
			  BSP_LCD_DrawLine(x_offset + 2*xx, ybottom, x_offset + 2*xx, ybottom-yy);
		  }
		  else
		  {   BSP_LCD_SetTextColor (LCD_COLOR_BLUE);
			  BSP_LCD_DrawLine(x_offset + 2*xx, ybottom, x_offset + 2*xx, ybottom-yy);
		  }


		  if (zz < 0) zz = 0;
		  if (zz > height) // if overflow: limit and display red line
		  {
			  zz = height;
			  BSP_LCD_SetTextColor (LCD_COLOR_WHITE);
			  BSP_LCD_DrawLine(x_offset + 2*xx + 1, ybottom, x_offset + 2*xx + 1, ybottom-zz);
		  }
		  else
		  {   BSP_LCD_SetTextColor (LCD_COLOR_RED);
			  BSP_LCD_DrawLine(x_offset + 2*xx + 1, ybottom, x_offset + 2*xx + 1, ybottom-zz);
		  }



		  // (pre-)draw pattern for bands (thereby deleting old spectral lines from last run, clearig the line for next line of this run, therefore: xoffset+x+1)
		  BSP_LCD_SetTextColor(LCD_COLOR_BLACK);

		  if ((xx / 23) % 2 == 0) // frequency bands: 248 lines are at 10kHz (see explanation in mp3player.c)
		  {
			  BSP_LCD_SetTextColor (LCD_COLOR_LIGHTGRAY);
		  }
		  else
		  {
			  BSP_LCD_SetTextColor (LCD_COLOR_GRAY);
		  }

		  // delete previous line elements (from last complete run) to clear two vertical lines for ensuing element of this run
		  // if these are not drawn without interrupt there may be interference lines visible on the display
		  __disable_irq();
		  BSP_LCD_DrawVLine(x_offset+2*xx + 2, ytop, height);
		  BSP_LCD_DrawVLine(x_offset+2*xx + 3, ytop, height);
		  __enable_irq();
	  }
}



/* *************************************************************************************
 * *************************************************************************************
 * Draw Amplitude as output by MP3 decoder m, i.e. valued as computed in mp3player.c
 *
 */

void Draw_Amplitude_OverlayLR (void)
{     int 	xx,
			ll2, ll1, ll,  // left channel, blue
			rr2, rr1, rr;  // right channel, red

      BSP_LCD_SetTextColor (LCD_COLOR_WHITE);
      BSP_LCD_DrawVLine(x_offset, ytop, height);

      ll2 = height/2; ll1 = height/2;
      rr2 = height/2; rr1 = height/2;

	  for (xx = 1; xx <= 479 - 8 ; xx++) // limit to maximum x coordinate x = 479
	  {


		  ll = height/2 + (int) ((FFTBuffer[4*xx+0]/100 + FFTBuffer[4*xx+2]/100 )/3);
		  rr = height/2 + (int) ((FFTBuffer[4*xx+1]/100 + FFTBuffer[4*xx+3]/100 )/3);

		  if (ll < 0)      ll = 0;
		  if (ll > height) ll = height;
		  if (rr < 0)      rr = 0;
		  if (rr > height) rr = height;

		  ll2 = ll1 ; ll1 = ll;
		  rr2 = rr1 ; rr1 = rr;

		  // draw line from previous to current element
		  BSP_LCD_SetTextColor (LCD_COLOR_BLUE);
		  BSP_LCD_DrawLine(x_offset + xx - 1, ybottom-ll2, x_offset + xx, ybottom-ll);
		  BSP_LCD_SetTextColor (LCD_COLOR_RED);
		  BSP_LCD_DrawLine(x_offset + xx - 1, ybottom-rr2, x_offset + xx, ybottom-rr);

		  // delete previous line element (from last complete run) to clear vertical line for ensuing element of this run
		  BSP_LCD_SetTextColor (LCD_COLOR_WHITE);
		  __disable_irq();
		  BSP_LCD_DrawVLine(x_offset+xx+1, ytop, height);
		  __enable_irq();
	  }
}


void Draw_Amplitude_SeparateLR (void)
{     int 	xx,
			ll2, ll1, ll,  // left channel, blue
			rr2, rr1, rr;  // right channel, red

	  BSP_LCD_SetTextColor (LCD_COLOR_WHITE);
      BSP_LCD_DrawVLine(x_offset, ytop, height);

      ll2 = 3*height/4; ll1 = 3*height/4;
      rr2 = height/4;   rr1 = height/4;

	  for (xx = 1; xx <= 479 ; xx++) // limit to maximum x coordinate x = 479
	  {
		  ll = 3*height/4 + (int) ((FFTBuffer[4*xx+0]/100 + FFTBuffer[4*xx+2]/100 )/6);
		  rr =   height/4 + (int) ((FFTBuffer[4*xx+1]/100 + FFTBuffer[4*xx+3]/100 )/6);

		  if (ll < 0)      ll = 0;
		  if (ll > height) ll = height;
		  if (rr < 0)      rr = 0;
		  if (rr > height) rr = height;

		  ll2 = ll1 ; ll1 = ll;
		  rr2 = rr1 ; rr1 = rr;

		  // draw line from previous to current element
		  BSP_LCD_SetTextColor (LCD_COLOR_BLUE);
		  BSP_LCD_DrawLine(x_offset + xx - 1, ybottom-ll2, x_offset + xx, ybottom-ll);
		  BSP_LCD_SetTextColor (LCD_COLOR_RED);
		  BSP_LCD_DrawLine(x_offset + xx - 1, ybottom-rr2, x_offset + xx, ybottom-rr);

		  // delete previous line element (from last complete run) to clear vertical line for ensuing element of this run
		  BSP_LCD_SetTextColor (LCD_COLOR_WHITE);
		  __disable_irq();
		  BSP_LCD_DrawVLine(x_offset+xx+1, ytop, height);
		  __enable_irq();
	  }
}


void prep_panelVU (short xs)
{

	 const short r_out = 150;
	 const short r_in  = 130;

	 const short ys    = 270;

	 const short M20 = -50;
	 const short M10 = -35;
	 const short M7  = -23;
	 const short M5  = -15;
	 const short M4  = -10;
	 const short M3  = -5;
	 const short M2  =  0;
	 const short M1  =  5;
	 const short M0  =  10;
	 const short P1 =   23;
	 const short P2 =   35;
	 const short P3 =   50;

	 si = sin((M20/180.0)*pi);
 	 co = cos((M20/180.0)*pi);
 	 drawThickLine(xs + r_in*si, ys - r_in*co, xs + r_out*si, ys - r_out * co, 2, 0, LCD_COLOR_BLACK);

	 si = sin((M10/180.0)*pi);
 	 co = cos((M10/180.0)*pi);
	 drawThickLine(xs + r_in*si, ys - r_in*co, xs + r_out*si, ys - r_out * co, 2, 0, LCD_COLOR_BLACK);

	 si = sin((M7/180.0)*pi);
 	 co = cos((M7/180.0)*pi);
	 drawThickLine(xs + r_in*si, ys - r_in*co, xs + r_out*si, ys - r_out * co, 2, 0, LCD_COLOR_BLACK);

	 si = sin((M5/180.0)*pi);
 	 co = cos((M5/180.0)*pi);
	 drawThickLine(xs + r_in*si, ys - r_in*co, xs + r_out*si, ys - r_out * co, 2, 0, LCD_COLOR_BLACK);

	 si = sin((M4/180.0)*pi);
 	 co = cos((M4/180.0)*pi);
	 drawThickLine(xs + r_in*si, ys - r_in*co, xs + r_out*si, ys - r_out * co, 2, 0, LCD_COLOR_BLACK);

	 si = sin((M3/180.0)*pi);
 	 co = cos((M3/180.0)*pi);
	 drawThickLine(xs + r_in*si, ys - r_in*co, xs + r_out*si, ys - r_out * co, 2, 0, LCD_COLOR_BLACK);

	 si = sin((M2/180.0)*pi);
 	 co = cos((M2/180.0)*pi);
	 drawThickLine(xs + r_in*si, ys - r_in*co, xs + r_out*si, ys - r_out * co, 2, 0, LCD_COLOR_BLACK);

	 si = sin((M1/180.0)*pi);
 	 co = cos((M1/180.0)*pi);
	 drawThickLine(xs + r_in*si, ys - r_in*co, xs + r_out*si, ys - r_out * co, 2, 0, LCD_COLOR_BLACK);

	 si = sin((M0/180.0)*pi);
 	 co = cos((M0/180.0)*pi);
	 drawThickLine(xs + r_in*si, ys - r_in*co, xs + r_out*si, ys - r_out * co, 2, 0, LCD_COLOR_BLACK);

	 si = sin((P1/180.0)*pi);
 	 co = cos((P1/180.0)*pi);
	 drawThickLine(xs + r_in*si, ys - r_in*co, xs + r_out*si, ys - r_out * co, 3, 0, LCD_COLOR_RED);

	 si = sin((P2/180.0)*pi);
 	 co = cos((P2/180.0)*pi);
	 drawThickLine(xs + r_in*si, ys - r_in*co, xs + r_out*si, ys - r_out * co, 3, 0, LCD_COLOR_RED);

	 si = sin((P3/180.0)*pi);
 	 co = cos((P3/180.0)*pi);
	 drawThickLine(xs + r_in*si, ys - r_in*co, xs + r_out*si, ys - r_out * co, 3, 0, LCD_COLOR_RED);

	 BSP_LCD_SetTextColor (LCD_COLOR_BLUE);
	 BSP_LCD_FillCircle(xs, ys, 7);

}


void VU (int left, int right) // left, right =
{
   const short r_hand = 150; // length of hand
   const short xsVU_L    = 120;
   const short xsVU_R    = 360;
   const short ysVU    = 270;

   // from prep VU above, for text position "VU"
//	 const short r_out = 150;
//	 const short ys    = 270;

   left2= left1; left1 = left;
   right2= right1; right1 = right;

   siVU = r_hand*sin((left1/180.0)*pi);
   coVU = r_hand*cos((left1/180.0)*pi);
   siVU2 = r_hand*sin((left2/180.0)*pi);
   coVU2 = r_hand*cos((left2/180.0)*pi);

   if (left1 != left2)
   {

	   // Zeiger alte Position löschen
	   BSP_LCD_SetTextColor (LCD_COLOR_WHITE);
	   BSP_LCD_DrawLine(xsVU_L, ysVU, xsVU_L+siVU2,ysVU-coVU2);

//		 BSP_LCD_SetTextColor (LCD_COLOR_BLACK);
//		 BSP_LCD_SetBackColor (LCD_COLOR_WHITE);
//		 BSP_LCD_SetFont (&Font24);
//		 BSP_LCD_DisplayStringAt (xsVU_L, (ys + r_out)/2, (char *)"VU", CENTER_MODE);
//		 BSP_LCD_SetFont (&Font12);

	   // Zeiger zeichnen
	   BSP_LCD_SetTextColor (LCD_COLOR_BLUE);
	   __disable_irq();
	   BSP_LCD_DrawLine(xsVU_L, ysVU, xsVU_L+siVU, ysVU-coVU);
	   __enable_irq();

	   prep_panelVU(xsVU_L);

   }

   siVU = r_hand*sin((right1/180.0)*pi);
   coVU = r_hand*cos((right1/180.0)*pi);
   siVU2 = r_hand*sin((right2/180.0)*pi);
   coVU2 = r_hand*cos((right2/180.0)*pi);

   if (right1 != right2)
   {
	   // Zeiger alte Position löschen
	   BSP_LCD_SetTextColor (LCD_COLOR_WHITE);
	   drawThickLine(xsVU_R, ysVU, xsVU_R+siVU2,ysVU-coVU2, 1, 0, LCD_COLOR_WHITE);

//		 BSP_LCD_SetTextColor (LCD_COLOR_BLACK);
//		 BSP_LCD_SetBackColor (LCD_COLOR_WHITE);
//		 BSP_LCD_SetFont (&Font24);
//		 BSP_LCD_DisplayStringAt (xsVU_R, (ys + r_out)/2, (char *)"VU", CENTER_MODE);
//		 BSP_LCD_SetFont (&Font12);

	   // Zeiger zeichnen
	   BSP_LCD_SetTextColor (LCD_COLOR_BLUE);
	   __disable_irq();
	   drawThickLine(xsVU_R, ysVU, xsVU_R+siVU, ysVU-coVU,  1, 0, LCD_COLOR_BLUE);
	   __enable_irq();

	   prep_panelVU(xsVU_R);
   }
}


void Draw_VU_Meter (void)
{     int 	xx;
	  float	ll;  // left channel, blue
	  float	rr;  // right channel, red
      float angle_L, angle_R;

      ll = 0.0;
      if (memoryVU_L > 0.0) memoryVU_L = memoryVU_L - 0.00005; // slowly decay

      rr = 0.0;
      if (memoryVU_R > 0.0) memoryVU_R = memoryVU_R - 0.00005; // slowly decay

      //sum up all amplitude values RMS LEFT channel
   	  for (xx = 1; xx <= 479 ; xx++) // limit to maximum x coordinate x = 799
	  {
		  ll = ll +  FFTBuffer[2*xx] * FFTBuffer[2*xx]; // left channel items only
	  }
   	  ll = sqrt (ll);
   	  ll = ll / (600000); // scaling from ll = [0...1]
   	  if (ll > 1.0) ll = 1.0;
   	  if (ll > memoryVU_L) memoryVU_L = ll; // lift up to peak value
   	  angle_L = -51.0 + memoryVU_L*100.0; // from -50.0 to 50.0 (== VU angle)

      //sum up all amplitude values RMS RIGHT channel
      for (xx = 1; xx <= 479 ; xx++) // limit to maximum x coordinate x = 799
 	  {
 		  rr = rr +  FFTBuffer[2*xx+1] * FFTBuffer[2*xx+1]; // right channel items only
 	  }
   	  rr = sqrt (rr);
   	  rr = rr / (600000); // scaling from ll = [0...1]
   	  if (rr > 1.0) rr = 1.0;
   	  if (rr > memoryVU_R) memoryVU_R = rr; // lift up to peak value
   	  angle_R = -51.0 + memoryVU_R*100.0; // from -50.0 to 50.0 (== VU angle)

   	  VU((int) angle_L, (int) angle_R);
}


void Clear_Spectrum_Area (uint32_t Color)
{
	BSP_LCD_SetTextColor (Color);	BSP_LCD_FillRect(0, 0, 480, 272);
}

