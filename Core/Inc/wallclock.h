#include <stdint.h>
#include <stdarg.h>


#define VGA_BLACK		0x0000
#define VGA_WHITE		0xFFFF
#define VGA_RED			0xF800
#define VGA_GREEN		0x0400
#define VGA_BLUE		0x001F
#define VGA_SILVER		0xC618
#define VGA_GRAY		0x8410
#define VGA_MAROON		0x8000
#define VGA_YELLOW		0xFFE0
#define VGA_OLIVE		0x8400
#define VGA_LIME		0x07E0
#define VGA_AQUA		0x07FF
#define VGA_TEAL		0x0410
#define VGA_NAVY		0x0010
#define VGA_FUCHSIA		0xF81F
#define VGA_PURPLE		0x8010

void LCD_DrawThickLine(int x1, int y1, int x2, int y2, uint16_t color);
void panel100 (int Hour, int Min, int Sec);
void prep_panel100 (void);
void Clear_Spectrum_Area (uint32_t Color);
void Draw_VU_Meter (void);
void Draw_Amplitude_SeparateLR (void);
void Draw_Amplitude_OverlayLR (void);
void Draw_Spectrum (void);

