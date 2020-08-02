  mov [VIDEO_TXT+1], 'X'
  mov [VIDEO_TXT_COLOR+1], (COLOR_CYAN << 4) | COLOR_BLACK

start:
  jmp start
