frame_prepare:
	; set background color
        mov 	a, 0
.next:
        mov 	[a + VIDEO_TXT_COLOR], COLOR_RED | (COLOR_WHITE << 4)
        inc	    a
        ifne 	a, 1200
        jmp 	.next
	    ret

start:
  jmp start
