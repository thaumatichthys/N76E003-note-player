N_CHANNELS 			equ 8
DUTY_CYCLE			equ 16



dseg ; the following should be in data ram



org 0x30
; org ACCUMULATOR_ADDR ; set the start address 
phase_accum: 		ds (2*N_CHANNELS) ; 16 bit phase accumulators
tunings_words: 		ds (2*N_CHANNELS) ; 16 bit tuning words


cseg

NotesUpdate: ; no parameters
	
	mov r5, #(N_CHANNELS)
	mov r0, #(phase_accum)
	mov r1, #(tunings_words)
	
	mov r6, #1 ; output flag 
	
notesUpdateLoop:
	
	mov a, @r0
	add a, @r1 
	mov @r0, a
	
	inc r0
	inc r1 
	
	mov a, @r0 
	addc a, @r1
	mov @r0, a 
	
	; if a is less than set value,
	subb a, #(DUTY_CYCLE) ; carry flag will be set if a < (255 - 16)
	jnc outputOff
	
	mov r6, #0

outputOff:
	
	inc r0
	inc r1 
	
	djnz r5, notesUpdateLoop
	
	; if r6 is set, set output. 
	mov a, r6 
	rrc a
	mov SOUND_OUT, c
	
	ret


NoteStop: ; channel to stop in a
	ret

NoteStart: ; channel to start in a, tuning word high in r2, low in r3.
	mov b, #2
	mul ab ; cuz 2 byte tuning words 
	mov r4, a ; save A 
	
	; mov r0, #(phase_accum)
	mov r1, #(tunings_words)
	mov a, r1 
	add a, r4 ; now A has the memory address of tuning word 
	mov r1, a 
	mov a, r3 
	mov @r1, a 
	inc r1 
	mov a, r2 
	mov @r1, a 
	ret


NotesInit:
	mov r5, #(N_CHANNELS)
	mov r0, #(phase_accum)
	mov r1, #(tunings_words)
notesUpdateLoop1:
	mov @r0, #0xff
	mov @r1, #0 ; low byte 
	inc r0
	inc r1 
	mov @r0, #0xff
	mov @r1, #0 ; high byte 
	inc r0
	inc r1 
	
	djnz r5, notesUpdateLoop1
	ret

