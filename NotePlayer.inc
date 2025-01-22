; Written by Max Xiang, Jan 22 2025

; Architecture: 8-channel Direct Digital Synthesizer capable of producing 8 frequencies at a time, range between 0 and 8 kHz, +- 0.25 Hz (Current configuration)


N_CHANNELS 			equ 8  ; dont change this! if you increase it beyond 16, you need to change other stuff!
DUTY_CYCLE			equ 24 ; divide by 256 to get each channel's duty cycle. Total is approx (num of channels enabled) * DUTY_CYCLE / 256.
TICK_DIVIDER		equ 0b00111111 ; resolution of the note player's timebase in terms of samples (bitmask)

; note: I really think a larger duty cycle sounds better for most midi files, I think this is because most of the "important" notes are higher frequency, and a 
; bigger duty cycle lessens the effect of a higher frequency having a shorter on time, since they all have the same duty cycle.

; PROTOCOL: DATA:
; 2 bytes for counter value, 2 bytes for delay after execution, 4 bits of metadata, 4 bits of channel 
; example: 0x1A, 0x02, 0x12, 0x34, 0x2
; 
; metadata: 0000 --> note off; 
; 			0001 --> note on;
; 			0011 --> end of data 

dseg ; the following should be in data ram

org 0x30
phase_accum: 		ds (2*N_CHANNELS) 	; 16 bit phase accumulators
tunings_words: 		ds (2*N_CHANNELS) 	; 16 bit tuning words
isr_counter:		ds 1				; 8 bit counter for tick divider
note_pointer:		ds 2				; 16 bit counter/pointer for data read address 
event_countdown:	ds 2				; 16 bit countdown for next command 
player_run:			ds 1				; flag for player enabled

cseg

;---------------------------------;
; Routine to start the MIDI player;
; usage: mov dptr, #(music_data)  ;
; lcall StartPlayingMusic 		  ;
;---------------------------------;
StartPlayingMusic:
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

;---------------------------------;
; Routine to stop the MIDI player ;
; usage:                          ;
; lcall StopPlayingMusic          ;
;---------------------------------;
StopPlayingMusic:
	;cpl P1.5
	push ar0 
	push ar4
	mov r0, #(player_run)
	mov @r0, #0 
	; now disable all channels
	mov r4, #(N_CHANNELS)
channelClearLoop:
	mov a, r4
	lcall NoteStop
	djnz r4, channelClearLoop
	pop ar4
	pop ar0 
	ret 

;---------------------------------;
; MIDI player implementation      ;
; usage:                          ;
; Don't, it is used internally    ;
; Use NotesUpdate instead         ;
;---------------------------------;
UpdateMIDIPlayer:
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
	clr c
	subb a, #0b00010000 ; subtract the note on command 
	jnz notNoteOn
	; here, run the note on command 
	; note that r2 and r3 already have the counter values loaded. If they didnt, then we must load them into r2 and r3.
	mov a, r6 	; load channel into a 
	lcall NoteStart
	ljmp endConditionalsDJPA
notNoteOn:
	; here, we stop the execution. 
	lcall StopPlayingMusic
endConditionalsDJPA:
	; start increment note_pointer
	mov r0, #(note_pointer)
	mov a, @r0 
	add a, #5
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
	; decrement the event countdown, but FIRST, ENSURE IT IS NOT ZERO!! VERY IMPORTANT
	mov r0, #(event_countdown)
	mov a, @r0
	mov r2, a 
	inc r0 
	mov a, @r0 
	orl a, r2 
	jz ISRCounter_skip ; dont do anything if it is zero already
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


;---------------------------------;
; DDS synthesizer implementation  ;
; usage:                          ;
; Don't, it is used internally    ;
; Use NotesUpdate instead         ;
;---------------------------------;
UpdateFrequencyGenerator:
	push ar0
	push ar1 
	push ar5
	push ar6 
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
	pop ar6
	pop ar5 
	pop ar1 
	pop ar0 
	ret 


;---------------------------------;
; Timer ISR handler               ;
; usage: (call at sample rate)    ;
; lcall NotesUpdate               ;
;---------------------------------;
NotesUpdate: ; no parameters
	lcall UpdateFrequencyGenerator
	lcall UpdateMIDIPlayer
	ret

;---------------------------------;
; Stop a single note              ;
; usage: mov a, #<channel num>    ;
; lcall NoteStop                  ;
;---------------------------------;
NoteStop: ; channel to stop in a
	
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

;---------------------------------;
; Start a single note             ;
; usage: mov a, #<channel num>    ;
; mov r3, #<high(tuning word)>    ;
; mov r2, #<low(tuning word)>	  ;
; lcall NoteStart                 ;
;---------------------------------;
NoteStart: ; channel to start in a, tuning word high in r3, low in r2.
	
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

;---------------------------------;
; Initialize the DDS              ;
; usage:                          ;
; lcall NotesInit                 ;
;---------------------------------;
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

