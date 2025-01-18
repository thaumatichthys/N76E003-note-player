N_CHANNELS 			equ 8
DUTY_CYCLE			equ 16 



dseg ; the following should be in data ram




; org ACCUMULATOR_ADDR ; set the start address 
phase_accum: 		ds (2*N_CHANNELS) ; 16 bit phase accumulators
tunings_words: 		ds (2*N_CHANNELS) ; 16 bit tuning words


bseg ; the following should be in bit addressable ram 

noteplayer_out:	dbit 1 ; 1 bit output


cseg

$NOLIST
$include(math_things.inc) ; math thingies
$LIST

NotesUpdate: ; no parameters
	
	mov r5, #(N_CHANNELS)
	mov r0, #(phase_accum)
	mov r1, #(tunings_words)
	
	mov r6, #0 ; output flag 
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
	jc outputOff
	
	mov r6, #0xFF

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

NoteStart: ; channel to start in a 
	ret


NotesInit:
	mov r5, #(N_CHANNELS)
	mov r0, #(phase_accum)
	mov r1, #(tunings_words)
notesUpdateLoop1:
	mov @r0, #0
	mov @r1, #low(1760)
	inc r0
	inc r1 
	mov @r0, #0
	mov @r1, #high(1760)
	inc r0
	inc r1 
	
	djnz r5, notesUpdateLoop1
	ret
