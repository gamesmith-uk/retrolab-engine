BLINK_SHOW = (COLOR_CYAN << 4)								  ; color when blinking is on
BLINK_HIDE = COLOR_BLACK											  ; color when blinking is off
BLINK_LINE = VIDEO_TXT_COLOR + 2								; line color address for line 2

                ; first char = green on dark blue
                mov         [VIDEO_TXT_COLOR], ((COLOR_GREEN << 4) | COLOR_DARK_BLUE)

				; place cursor at the end of sentence
				mov			^[VIDEO_CURSOR_POS], msglen - 1	

				; setup blinking color text and timer
				mov			[BLINK_LINE], BLINK_SHOW				; set line color
				mov			[TIMER_FRAME_0], 30							; timer each 30 frames (500 ms)
				ivec		INT_TIMER, timer0								; interrupt when timer reaches zero

				; set interrupt for keyboard and joystick
				ivec		INT_KEYBOARD, keyboard
				ivec		INT_JOYSTICK, joystick
				int			INT_JOYSTICK, 0

				; loop forever
halt:   jmp			halt

				; ----------
				; INTERRUPTS
				; ----------

				; when timer reaches zero
timer0:
				ifne XT, XT_TIMER_0											; only if timer 0
				iret
				pushw		A
				ifeq		[BLINK_LINE], BLINK_SHOW				; if showing, hide
				mov			A, BLINK_HIDE
				ifeq		[BLINK_LINE], BLINK_HIDE				; if hiding, show
				mov			A, BLINK_SHOW
				mov			[BLINK_LINE], A
				popw		A
				mov			[TIMER_FRAME_0], 30							; reset timer
				iret

				; when a key is pressed
keyboard:
				and			XT, 0xff												; put key on the screen
				mov			[key], XT
				iret

				; when a joystick is pressed
joystick:
				pusha
				mov			I, 7														; for (i = 7; i >= 0; --i) {
				mov			J, joy													;   j = video memory position
.cont:	
				mov			A, [JOYSTICK_STATE]							;   A = (XT >> i) & 1
				shr			A, I
				and			A, 1

				mov			[J], '.'												;   print '.' or '0'
				ifeq		A, 1
				mov			[J], '1'

				ifeq		I, 0														;   break if i == 0
				jmp			.done

				sub			I, 1														;   --i; ++j;
				add			J, 1
				jmp			.cont														; }
.done:
				popa
				iret

				; ------------
				; VIDEO MEMORY
				; ------------

				org			VIDEO_TXT
msg:		db			"Hello world!"									; print "Hello world" (line 0)
msglen = $-msg

				org     VIDEO_TXT + (2 * 40) + 11				; print "Blinking message" (line 2, centered)
       	db			"Blinking message"

				org			VIDEO_TXT + (10 * 40)
				db			"Character pressed: "
key:		bss			1

				org			VIDEO_TXT + (13 * 40)
				db			"Joystick state: "
joy:		bss			8

; vim:st=8:sts=8:sw=8:noexpandtab:foldmethod=marker:syntax=fasm
