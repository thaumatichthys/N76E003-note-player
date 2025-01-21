N_CHANNELS 			equ 8  ; dont change this! if you increase it beyond 16, you need to change other stuff!
DUTY_CYCLE			equ 16 ; divide by 256 to get each channel's duty cycle. Total is approx (num of channels enabled) * DUTY_CYCLE / 256.
TICK_DIVIDER		equ 0b00111111 ; resolution of the note player's timebase in terms of samples (bitmask)


; PROTOCOL: DATA:
; 2 bytes for counter value, 2 bytes for delay after execution, 4 bits of metadata, 4 bits of channel 
; example: 0x1A, 0x02, 0x12, 0x34, 0x2
; 
; metadata: 0000_0000 --> note off; 
; 			0001_0000 --> note on;
; 			0011_0000 --> end of data 

dseg ; the following should be in data ram



org 0x30
phase_accum: 		ds (2*N_CHANNELS) 	; 16 bit phase accumulators
tunings_words: 		ds (2*N_CHANNELS) 	; 16 bit tuning words
isr_counter:		ds 1				; 8 bit counter for tick divider
note_pointer:		ds 2				; 16 bit counter/pointer for data read address 
event_countdown:	ds 2				; 16 bit countdown for next command 
player_run:			ds 1				; flag for player enabled

cseg

; music_data:			db 	#high(540), #low(540), #high(0), #low(0), 0, #high(330), #low(330), #high(500), #low(500), 1


StartPlayingMusic: ; UNTESTED
	; usage:
	; mov dptr, #(music_data)
	; lcall StartPlayingMusic 
	
	push ar0
	
	; set everything to zero first 
	mov r0, #(isr_counter)
	mov @r0, #0
	
	; set note pointer to dptr 
	mov r0, #(note_pointer)
	mov @r0, dpl 
	inc r0 
	mov @r0, dph 
	
	; set event counter to zero 
	mov r0, #(event_countdown)
	mov @r0, #0
	inc r0 
	mov @r0, #0
	
	; set player_run 
	mov r0, #(player_run)
	mov @r0, #1
	
	pop ar0 
	ret 
	
StopPlayingMusic: 	; UNTESTED
	;cpl P1.5
	push ar0 
	mov r0, #(player_run)
	mov @r0, #0 
	pop ar0 
	ret 







UpdateMIDIPlayer: 	; UNTESTED
	; first check if this function should be doing anything at all
	mov r0, #(player_run)
	mov a, @r0 
	jnz playerIsRunning
	ret 	; exit if the player isn't running 
playerIsRunning:

	push ar0 
	push ar2
	push ar3
	push ar4
	push ar5 
	push ar6 
	push ar7

	; increment the isr counter and AND with bitmask
	mov r0, #(isr_counter)
	inc @r0
	mov a, @r0 
	anl a, #(TICK_DIVIDER)
	mov @r0, a 
	
	
	; see if we should skip this 
	jnz ISRCounter_skip
	; START MIDI PLAYER THINGS
	
	; if event countdown is not zero, then dont run anything (still decrement it tho)
	
	; start check if event countdown is zero 
	mov r0, #(event_countdown)
	mov a, @r0
	mov r2, a 
	inc r0 
	mov a, @r0 
	orl a, r2 
	jnz eventCountdown_skip
	; end check if event countdown is zero
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; this is missing all the shit, just testing rn
	cpl P1.5
	
	
	
	; load note ptr into dptr 
	
	mov r0, #(note_pointer)
	mov a, @r0 
	mov dpl, a 
	inc r0
	mov a, @r0 
	mov dph, a 
	
	
	; put data into registers
	mov a, #0
	movc a, @a+dptr
	mov r2, a 	; counter low 
	mov a, #1
	movc a, @a+dptr
	mov r3, a 	; counter high 
	mov a, #2
	movc a, @a+dptr
	mov r4, a 	; delay low 
	mov a, #3
	movc a, @a+dptr
	mov r5, a 	; delay high
	mov a, #4
	movc a, @a+dptr
	mov r6, a 	; metadata byte  
	mov r7, a 	; also metadata byte 
	; end put data into registers 
	
	; copy metadata into r6 and r7, but different parts 
	mov a, r6 
	anl a, #0x0F
	mov r6, a 	; r6 is the channel 
	
	mov a, r7 
	anl a, #0xF0
	mov r7, a 	; r7 is the command 
	
	; now have the branching parts 
	
	mov a, r7 	; this isnt needed but imma leave it here 
	jnz notNoteOff
	
	; here, run the note off command 
	mov a, r6 
	lcall NoteStop
	
	
	ljmp endConditionalsDJPA
notNoteOff:		
	; now see if it is note on, or data end 
	mov a, r7 	
	subb a, #0b00010000 ; subtract the note on command 
	;jnz notNoteOn ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; bruh
	
	; here, run the note on command 
	; note that r2 and r3 already have the counter values loaded. If they didnt, then we must load them into r2 and r3.
	mov a, r6 	; load channel into a 
	lcall NoteStart
	ljmp endConditionalsDJPA
notNoteOn:
	; here, we stop the execution. 
	lcall StopPlayingMusic
	mov r4, #(N_CHANNELS)
channelClearLoop:
	mov a, r4
	lcall NoteStop
	djnz r4, channelClearLoop
	
	;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	
	
	
endConditionalsDJPA:
	
	; start increment note_pointer
	mov r0, #(note_pointer)
	mov a, @r0 
	add a, #5 ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;; increment by 1 instead of 5 
	mov @r0, a 
	inc r0 
	mov a, @r0 
	addc a, #0 
	mov @r0, a 
	; end increment note_pointer
	
	; start set event countdown
	mov r0, #(event_countdown)
	mov a, r4; 0x01; r4 
	mov @r0, a 
	inc r0 
	mov a, r5
	mov @r0, a 
	; end set event countdown
	
	; END MIDI PLAYER THINGS 
eventCountdown_skip:
	; decrement the event countdown, but FIRST, ENSURE IT IS NOT ZERO!!
	
	mov r0, #(event_countdown)
	mov a, @r0
	mov r2, a 
	inc r0 
	mov a, @r0 
	orl a, r2 
	jz ISRCounter_skip
	
	; start decrement event_countdown 
	mov r0, #(event_countdown)
	mov a, @r0
	clr c 
	subb a, #1
	mov @r0, a 
	
	inc r0 
	mov a, @r0 
	subb a, #0 
	mov @r0, a 
	; end decrement event countdown
	
ISRCounter_skip:
	pop ar7 
	pop ar6
	pop ar5 
	pop ar4
	pop ar3 
	pop ar2 
	pop ar0
	ret 



UpdateFrequencyGenerator:
	push ar0
	push ar1 
	push ar5
	push ar6 
	; START GENERATE FREQUENCY SECTION
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
	; END GENERATE FREQUENCY SECTION
	pop ar6
	pop ar5 
	pop ar1 
	pop ar0 
	ret 

NotesUpdate: ; no parameters ; UNTESTED
	lcall UpdateFrequencyGenerator
	lcall UpdateMIDIPlayer
	ret


NoteStop: ; channel to stop in a	; UNTESTED
	
	push ar1
	push ar4 
	mov b, #2 
	mul ab 
	mov r4, a ; save A 
	
	; set tuning word to zero 
	mov r1, #(tunings_words)
	mov a, r1 
	add a, r4 ; now A has the memory address of tuning word 
	mov r1, a 
	mov @r1, #0 
	inc r1 
	mov @r1, #0
	
	; set phase accumulator to zero 
	mov r1, #(phase_accum)
	mov a, r1 
	add a, r4 ; now A has the memory address of phase_accum
	mov r1, a 
	mov @r1, #0xff 
	inc r1 
	mov @r1, #0xff
	
	pop ar4 
	pop ar1
	ret

NoteStart: ; channel to start in a, tuning word high in r2, low in r3.
	
	push ar1
	push ar4 
	mov b, #2
	mul ab ; cuz 2 byte tuning words 
	mov r4, a ; save A 
	
	mov r1, #(tunings_words)
	mov a, r1 
	add a, r4 ; now A has the memory address of tuning word 
	mov r1, a 
	mov a, r2 
	mov @r1, a 
	inc r1 
	mov a, r3 
	mov @r1, a
	pop ar4 
	pop ar1
	ret


NotesInit:
	push ar0 
	push ar1 
	push ar5 
	
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
	
	; disable player (initialise memory)
	mov r0, #(player_run)
	mov @r0, #0
	
	pop ar5 
	pop ar1 
	pop ar0
	ret

