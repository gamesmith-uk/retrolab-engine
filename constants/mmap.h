#ifndef MEMORY_MAP_H_
#define MEMORY_MAP_H_

// I/O and interrupt numbers

#define INT_CPU             0X0  // when a CPU error happens (compare XT with XT_CPU_...)
#define INT_KEYBOARD        0X1  // when a key is pressed (XT contains the ASCII code of the key pressed)
                                    //   special keys:
                                    //     [F0]: insert
                                    //     [F1]: home
                                    //     [F2]: end
                                    //     [F3]: page up
                                    //     [F4]: page down
                                    //     [F5]: right
                                    //     [F6]: left
                                    //     [F7]: up
                                    //     [F8]: down
#define INT_VIDEO           0X3  // no interrupts are implemented yet
#define INT_TIMER           0X4  // when a timer reaches zero (compare XT with XT_TIMER_...)
#define INT_SOUND           0X5  // not implemented yet
#define INT_FPU             0X6  // not implemented yet
#define INT_JOYSTICK        0X7  // when a joystick button is pressed (see JOYSTICK_STATE for XT register)

// Interrupt register (XT)

#define XT_TIMER_0          0X0  // Frame timer 0 has reached zero
#define XT_TIMER_1          0X1  // Frame timer 1 has reached zero
#define XT_TIMER_2          0X2  // Frame timer 2 has reached zero
#define XT_TIMER_3          0X3  // Frame timer 3 has reached zero
#define XT_CPU_IRET         0X0  // IRET outside of an interrupt
#define XT_CPU_DIVZERO      0X1  
#define XT_CPU_INVALID_OP   0X2  
#define DEV_MEM_MGR         0X2  // used to copy a block of memory:
                                    //     [register X]: initial origin position
                                    //     [register Y]: initial destination position
                                    //     [paramter]:   memory block size

// Colors

#define COLOR_BLACK         0X0  
#define COLOR_PURPLE        0X1  
#define COLOR_RED           0X2  
#define COLOR_ORANGE        0X3  
#define COLOR_YELLOW        0X4  
#define COLOR_LIME          0X5  
#define COLOR_GREEN         0X6  
#define COLOR_TURQUOISE     0X7  
#define COLOR_DARK_BLUE     0X8  
#define COLOR_BLUE          0X9  
#define COLOR_LIGHT_BLUE    0XA  
#define COLOR_CYAN          0XB  
#define COLOR_WHITE         0XC  
#define COLOR_LIGHT_GRAY    0XD  
#define COLOR_GRAY          0XE  
#define COLOR_DARK_GRAY     0XF  

// Initial stack

#define STACK_BOTTOM        0XE2A6  // Initial position for stack (SP)

// Joystick

#define JOYSTICK_STATE      0XE2A7  // contains the current state of the joystick
                                    //   [bit 0]: up
                                    //   [bit 1]: down
                                    //   [bit 2]: left
                                    //   [bit 3]: right
                                    //   [bit 4]: Z
                                    //   [bit 5]: X
                                    //   [bit 6]: A
                                    //   [bit 7]: S

// Timer

#define TIMER_FRAME_0       0XE2A8  // count down timer - decrements at the end of each frame
#define TIMER_FRAME_1       0XE2AA  
#define TIMER_FRAME_2       0XE2AC  
#define TIMER_FRAME_3       0XE2AE  

// CPU

#define CPU_VERSION_MAJOR   0XE2B0  // always 0x1 for this cpu type
#define CPU_VERSION_MINOR   0XE2B1  // increments with each new release
#define CPU_RANDOM          0XE2B2  // returns a random number at each read

// Video

#define VIDEO_MODE          0XE2B4  // video config
                                    //   [bit 0]: 0 = sprite/tiles, 1 = pixel data
                                    //   [bit 1]: interrupt on start of frame
                                    //   [bit 2]: interrupt of end of frame
                                    //   [bit 3]: interrupt of start of line
                                    //   [bit 4]: interrupt on end of file
#define VIDEO_BORDER        0XE2B5  // border color
#define VIDEO_TXT           0XE2B6  // text (40 * 30 characters)
#define VIDEO_TXT_COLOR     0XE766  // text line color - 30 lines
                                    //   each byte = foreground / background color
#define VIDEO_CURSOR_INFO   0XE784  // Cursor info
                                    //   [bits 0..3]: cursor color
                                    //   [4]: cursor visible
                                    //   [5]: cursor style (0 = whole, 1 = half) (not implemented)
                                    //   [6]: blinking (not implemented)
#define VIDEO_CURSOR_POS    0XE785  // cursor position (continuous)
#define VIDEO_VBLANK        0XE787  // raster: is beam in VBLANK?
#define VIDEO_BEAM_X        0XE788  // beam position X (line from 0 - 119, porch from 120-239)
#define VIDEO_BEAM_Y        0XE789  // beam position Y (frame from 0 - 135, ...)
#define VIDEO_TXT_DISLOC_X  0XE78A  // text dislocation (horizontal)
#define VIDEO_TXT_DISLOC_Y  0XE78B  // text dislocation (vertical)
#define VIDEO_TILE_MAP1     0XE78C  // 290 bytes (0x122)
                                    //   [000-11F]: (first 7 bits: tile index -- last bit: horizontal flip)
                                    //   [    120]: vertical dislocation
                                    //   [    121]: horizontal dislocation
                                    //   [122-130]: unused
#define VIDEO_TILE_MAP2     0XE8AE  
#define VIDEO_SPRITE_MAP    0XE9D0  // 32 sprites on screen * 8 bytes = 256 bytes (0x100)
                                    //   [   00]: sprite index
                                    //   [   01]: graphical representation: (visible, flip horizontal, vertical, double size)
                                    //   [02-03]: position on screen
                                    //   [04-07]: four first collisions (0xFF - no collision)
#define VIDEO_TILE_DATA     0XEAD0  // 128 tiles * 10 bytes per tile = 1280 bytes (0x500)
                                    //   [00-01]: tile palette (max 4 colors)
                                    //   [02-0A]: tile data (each byte - 1 line)
#define VIDEO_SPRITE_DATA   0XEFD0  // 64 sprites * 128 bytes per sprite = 8192 bytes (0x2000)
                                    //   (size 16*16, 2 pixels per byte)
#define VIDEO_PALETTE       0XFFD0  // palette (3 bytes per color)
#endif
