
; AsmRegEx
; Copyright (c) 2006, Thomas Rauh.
; All rights reserved.

struc	REGEX_T
{
   .atoms	 rd 1	; number of ATOMs of the SRE
   .chars	 rd 1	; number of non-emtpy ATOMs(not implemented yet)
   .props	 rd 1	; properties
   .first	 rd 1	; starting  states
   .last	 rd 1	; accepting states
   .charpos	 rd 256 ; ASCII table
   .follow	 rd 32	; non-deterministic transition map
   .transmap	 rb 2048; deterministic transition map
   .charset	 rb 256 ; used for bracket expressions (character sets)
   .hashspace	 rd 142 ; used for hashing if atoms > 7
   .reserved	 rd 13
}
SIZE_REGEX_T	  equ 4096

atoms@REGEX_T	  equ  0
chars@REGEX_T	  equ (atoms@REGEX_T	 + 4)
props@REGEX_T	  equ (chars@REGEX_T	 + 4)
first@REGEX_T	  equ (props@REGEX_T	 + 4)
last@REGEX_T	  equ (first@REGEX_T	 + 4)
charpos@REGEX_T   equ (last@REGEX_T	 + 4)
follow@REGEX_T	  equ (charpos@REGEX_T	 + 256 * 4)
transmap@REGEX_T  equ (follow@REGEX_T	 + 32  * 4)
charset@REGEX_T   equ (transmap@REGEX_T  + 2048   )
hashspace@REGEX_T equ (charset@REGEX_T	 + 256	  )
reserved@REGEX_T  equ (hashspace@REGEX_T + 142 * 4)

; If the number of (character) positions is greater than 8 then
; REGEX_T.transmap, REGEX_T.charset and REGEX_T.hashspace are
; used as a hashtable.

; The hashtable has 359 entries each of size 8 bytes.

; struc HASH_ENTRY
; {
;     .key   rd 1       ; key == mask of currently active states (!= 0)
;     .val   rd 1       ; val == follow set, i.e., mask of reachable states
; }

REGEX_HASHSIZE	equ 359

; values for REGEX_T.props
REGEX_DFA	equ 1
REGEX_EMPTY	equ 2

; regcomp parser stack TOKENs:
EMPTY		equ   1
ATOM		equ   2
UNION_OP	equ   4
LPAREN		equ   8

; return value of regcomp:
REGEX_ERROR	equ  0	; error, any other value indicates success

; return value of regexec:
REGEX_NOMATCH	equ  0	; no match found, any other value indicates a match

;==============================================================================
;
;   @name  : regcomp
;
;   @type  : PROC, STDCALL
;
;   @desc  : SRE-lexer-parser-compiler.
;
;   @param : regex_t == pointer to a 0-initialized REGEX_T struc
;            pattern == pointer to a 0-terminated SRE
;
;   @return: eax == REGEX_ERROR (== 0)
;             ecx == pointer to error message
;             edx == length  of error message
;            ;------------------------------
;            eax == REGEX_SUCCESS (!= 0)
;             esi == pointer to last input character (== \x0)
;
;   @regs  : all regs destroyed
;
;   @uses  : @file ctypedef.inc
;            @file ctypemap.inc
;             @var ctype_map - a 128 byte character classification table
;
;   @pre   : The input SRE must be 0-terminated.
;            The REGEX_T variable pointed to by regex_t must be 0-initialized.
;            The direction-flag (DF) must be clear.
;
;   @post  : The REGEX_T variable pointed to by regex_t is initialized.
;            (Only if REGEX_SUCCESS was returned)
;
;==============================================================================

; regcomp(REGEX_T *regex_t, char *pattern)

	pattern equ ebp+20
	regex_t equ ebp+16
regcomp:
      ; ret_eip equ ebp+12

	; static local variables
	lparens equ ebp+8
	saved	equ ebp+4

	; dynamic local variables (parser stack)
	prev	equ esp+16
	first	equ esp+12
	last	equ esp+8
	empty	equ esp+4
	token	equ esp

	; init
	xor	eax, eax
	sub	esp, 12
	xor	ebx, ebx
	mov	[esp], ebp
	mov	ebp, esp
	mov	edi, [regex_t]
	sub	esp, 4
	mov	esi, [pattern]
	mov	[lparens], ebx
	mov	dword[token], EMPTY

	; lexer
 .input:
	xor	eax, eax
	xor	ecx, ecx
	cmp	dword[edi+atoms@REGEX_T], 31
	ja   .error_state_overflow
	lodsb
	test	eax, eax
	jz   .check_stack
	test	eax, 80h
	jnz  .error_inv_char
	test	byte[ctype_map+eax], PUNCT
	mov	ebx, eax
	jnz  .is_op_or_charset

  .character:
	; ebx == current character
	; esi == pointer to next input character
	; edi == pointer to REGEX_T
	mov	ecx, [edi+atoms@REGEX_T]
	xor	eax, eax
	shl	ebx, 2
	bts	eax, ecx
	xor	edx, edx
	or	[edi+charpos@REGEX_T+ebx], eax
	; follow[current_position] <- 0
	mov	[edi+follow@REGEX_T+ecx*4],edx
	; first(v) <- current position (bit)
	mov	ebx, eax
	; last(v)  <- current position (bit)
	mov	ecx, eax

   .atom_token:
	; CHAR, CHAR_SET, DOT
	; ebx == first(v)
	; ecx == last(v)
	; edx == empty(v) == FALSE
	; esi == pointer to next input character
	; edi == pointer to REGEX_T
	cmp	byte[esi], "?"
	mov	eax, [token]
	jz   .atom_quest_op
	cmp	byte[esi], "*"
	jz   .atom_star_op
	cmp	byte[esi], "+"
	jnz  .atom_control

	mov	eax, [edi+atoms@REGEX_T]
	; edx <- (-1) -- will be incremented to 0 (empty(v) == FALSE)
	dec	edx

   .atom_star_op:
	; eax == current position (int)
	; ebx == first(v)
	; ecx == last(v)
	; edx == empty(v) == FALSE
	; esi == pointer to "+" or "*"
	; edi == pointer to REGEX_T

	; follow[current_position] <- follow[current_position] | first(v)
	or	[edi+follow@REGEX_T+eax*4], ebx

   .atom_quest_op:
	inc	esi
	mov	eax, [token]
	; empty(v) <- TRUE if "*" or "?" operator or FALSE if "+" operator
	inc	edx

   .atom_control:
	; eax == stack TOKEN
	; ebx == first(v)
	; ecx == last(v)
	; edx == empty(v)
	; esi == pointer to next input character
	; edi == pointer to REGEX_T
	; position++
	inc	dword[edi+atoms@REGEX_T]
	test	eax, (EMPTY or UNION_OP or LPAREN)
	jnz  .shift_atom

	; assert(stack TOKEN == ATOM) -- must be an ATOM but just to be sure
	test	eax, ATOM
	jz   .error_unknown

	; stack TOKEN == ATOM
	; input TOKEN == ATOM
	; --> do factor (== concatenation)

    .do_factor:
	; ebx == first(vr)
	; ecx == last(vr)
	; edx == empty(vr)
	; esi == pointer to next input character
	; edi == pointer to REGEX_T

	mov	[saved], ecx
	; eax <- last(stack ATOM)
	mov	eax, [last]
	xor	ecx, ecx

	; FOR(all positions P in last(vl))
	;       follow[P] <- follow[P] | first(vr)

    .init_factor_follow:
	; eax == last(vl)
	; ebx == first(vr)
	; ecx == position * 4
	shr	eax, 1
	jnc   .next_follow_pos
	or	[edi+follow@REGEX_T+ecx], ebx
	test	eax, eax

    .next_follow_pos:
	jz   .factor_follow_done
	add	ecx, 4
	jmp  .init_factor_follow
	;--------------------

    .factor_follow_done:
	; ecx <- last(vr)
	mov	ecx, [saved]
	; eax <- last(vl)
	mov	eax, [last]
	; edx == empty(vr)
	test	edx, edx
	; last(v) <- last(vr)
	mov	[last], ecx
	jz   .test_empty_vl

	; IF(empty(vr))
	;    THEN last(v) <- last(vr) | last(vl)
	or	[last], eax

    .test_empty_vl:
	test	dword[empty], -1
	jz   .set_empty_v

	; IF(empty(vl))
	;    THEN first(v) <- first(vl) | first(vr)
	or	[first], ebx

    .set_empty_v:
	; empty(v) <- empty(vl) & empty(vr)
	and	[empty], edx
	jmp  .input
	;--------------------

   .shift_atom:
	sub	esp, 16
	mov	eax, ATOM
	mov	[empty], edx
	mov	[last],  ecx
	mov	[first], ebx
	mov	[token], eax
	jmp  .input
	;--------------------

 .is_op_or_charset:
	cmp	eax, "|"
	jz   .union_op
	cmp	eax, "*"
	jz   .error_un_op
	cmp	eax, "?"
	jz   .error_un_op
	cmp	eax, "+"
	jz   .error_un_op
	cmp	eax, "("
	jz   .lparen
	cmp	eax, ")"
	jz   .rparen
	cmp	eax, "["
	jz   .char_set
	cmp	eax, "\"
	jz   .backslash
	cmp	eax, "."
	; ebx <- current character
	mov	ebx, eax
	jnz  .character

  .dot:
	; dot-character-set: all but LF (\x0A) and CR (\x0D) (Windows newline)
	mov	ebx, [edi+atoms@REGEX_T]
	xor	eax, eax
	xor	ecx, ecx
	; first(v) <- current position
	bts	eax, ebx
	mov	edx, 4
	mov	ebx, eax

   .init_dotchars:
	or	[edi+charpos@REGEX_T+ecx], eax

    .skip_lf:
	or	[edi+charpos@REGEX_T+edx], ebx
	add	ecx, 8
	add	edx, 8
	cmp	ecx, (4 * 256)
	jz   .dotchars_done
	cmp	ecx, (4 * 10)
	jnz  .init_dotchars
	; skip \x0A
	add	ecx, 8
	; init \x0B
	or	[edi+charpos@REGEX_T+edx], eax
	; init \x0C
	or	[edi+charpos@REGEX_T+ecx], ebx
	; skip \x0D
	add	edx, 16
	; \x0E
	add	ecx, 8
	jmp  .init_dotchars
	;--------------------

   .dotchars_done:
	mov	eax, [edi+atoms@REGEX_T]
	; empty(v) <- FALSE
	xor	edx, edx
	; last(v)  <- first(v) == current position (bit)
	mov	ecx, ebx
	; follow[current_position] <- 0
	mov	[edi+follow@REGEX_T+eax*4], edx
	; ebx == first(v)
	; ecx == last(v)
	; edx == empty(v)
	; esi == pointer to next input character
	; edi == pointer to REGEX_T
	jmp  .atom_token
	;====================

  .char_set:
	xor	eax, eax
	mov	ebx, edi
	mov	ecx, 64
	lea	edi, [ebx+charset@REGEX_T]
	rep  stosd
	lodsb
	test	eax, eax
	jz   .error_open_charset
	xor	edx, edx
	mov	edi, ebx
	cmp	eax, "^"
	setz	dl
	jnz  .is_rbracket
	lodsb
	test	eax, eax
	jz   .error_open_charset

   .is_rbracket:
	cmp	eax, "]"
	jnz  .check_setchar
	or	byte[edi+charset@REGEX_T+eax], 1
	lodsb
	test	eax, eax
	jz   .error_open_charset

    .check_setchar:
	cmp	eax, "]"
	jz   .end_of_set
	cmp	eax, "\"
	jnz  .setchars
	cmp	byte[esi], 0
	jz   .error_open_charset
	call	regcvtesc
	; eax == converted character
	; esi == pointer to next input character

    .setchars:
	mov	ebx, eax
	; eax == character from charset
	; ebx == eax
	; edx == 1 if first char of charset was "^", 0 otherwise
	; esi == pointer to next input character
	; edi == pointer to REGEX_T
	or	byte[edi+charset@REGEX_T+eax], 1
	lodsb
	test	eax, eax
	jz   .error_open_charset
	cmp	eax, "-"
	jnz  .check_setchar
	cmp	byte[esi], "]"
	jz   .setchars
	lodsb
	test	eax, eax
	jz   .error_open_charset
	cmp	eax, "\"
	jnz  .check_range
	cmp	byte[esi], 0
	jz   .error_open_charset
	call	regcvtesc
	; eax == converted character
	; esi == pointer to next input character

    .check_range:
	cmp	eax, ebx
	jb   .error_inv_range
	jz   .range_done

    .init_range:
	inc	ebx
	or	byte[edi+charset@REGEX_T+ebx], 1
	cmp	ebx, eax
	jb   .init_range

    .range_done:
	lodsb
	test	eax, eax
	jz   .error_open_charset
	cmp	eax, "-"
	jnz  .check_setchar
	cmp	byte[esi], "]"
	jz   .setchars
	cmp	byte[esi], "-"
	jz   .setchars
	jmp  .error_inv_range
	;--------------------

   .end_of_set:
	; eax == "]"
	; ebx == eax
	; edx == 1 if first char of charset was "^", 0 otherwise
	; esi == pointer to next input character
	; edi == pointer to REGEX_T
	xor	ebx, ebx
	mov	eax, [edi+atoms@REGEX_T]
	mov	ecx, 255
	; first(v) <- current position (bit)
	bts	ebx, eax
	xor	edx, 1
	xor	eax, eax

   .init_setchars:
	cmp	[edi+charset@REGEX_T+ecx], dl
	jnz  .next_setchar
	mov	eax, ebx
	or	[edi+charpos@REGEX_T+ecx*4], ebx

    .next_setchar:
	dec	ecx
	jge  .init_setchars
	; empty(v) <- FALSE
	xor	edx, edx
	test	eax, eax
	; last(v) <- first(v) == current position (bit)
	mov	ecx, ebx
	jz   .error_empty_charset
	jmp  .atom_token
	;====================

 .union_op:
	; current TOKEN == UNION_OP
	; test if the UNION_OP is preceeded by an ATOM
	test	dword[token], ATOM
	mov	eax, UNION_OP
	jz   .error_union_op

	; if the stack ATOM is preceeded by a UNION_OP then reduce
	cmp	[prev], eax
	jnz  .push_union_op

	mov	ebx, [first]
	mov	ecx, [last]
	mov	edx, [empty]

  .reduce_union_op:
	; pop ATOM and UNION_OP
	add	esp, (16 + 4)
	; must be an ATOM, otherwise the UNION_OP will not be on the stack
	test	dword[token], ATOM
	jz   .error_unknown
	or	ebx, [first]
	or	ecx, [last]
	or	edx, [empty]
	; test for another pending UNION_OP
	cmp	[prev], eax
	jz   .reduce_union_op
	mov	[first],ebx
	mov	[last], ecx
	mov	[empty],edx

  .push_union_op:
	push	eax
	jmp  .input
	;====================

 .rparen:
	; eax == ")"
	cmp	dword[lparens], 0
	mov	ebx, eax
	; if there is no "(" on the stack then treat ")" as ordinary character
	jz   .character
	dec	dword[lparens]

	; test for an illegal "|)" or an empty parenthesis pair "()"
	test	dword[token], (UNION_OP or LPAREN)
	jnz  .error_rparen

	; assert(stack TOKEN == ATOM) -- must be an ATOM but just to be sure
	test	dword[token], ATOM
	jz   .error_unknown

	; reduce the paren expression (stack TOKEN == ATOM)
	mov	ebx, [first]
	mov	ecx, [last]
	mov	edx, [empty]
	; pop ATOM
	add	esp, 16
	test	dword[token], LPAREN
	jnz  .lookahead_un_op
	test	dword[token], UNION_OP
	jz   .error_unknown

	; pop UNION_OP
	add	esp, 4

	; ebx == first(vr)
	; ecx == last(vr)
	; edx == empty(vr)

	; assert(stack TOKEN == ATOM) -- must be an ATOM but just to be sure
	test	dword[token], ATOM
	jz   .error_unknown

	; stack TOKEN == ATOM
	or	edx, [empty]
	or	ecx, [last]
	or	ebx, [first]
	; pop ATOM
	add	esp, 16
	test	dword[token], LPAREN
	jz   .error_unknown

  .lookahead_un_op:
	; pop LPAREN
	add	esp, 4
	xor	eax, eax
	; ebx == first(v)
	; ecx == last(v)
	; edx == empty(v)
	; esi == pointer to next input character
	; edi == pointer to REGEX_T
	cmp	byte[esi], "?"
	jz   .paren_follow_done
	cmp	byte[esi], "*"
	jz   .do_paren_star
	cmp	byte[esi], "+"
	mov	eax, [token]
	jnz  .have_stack_atom

  .do_paren_star:
	; RPAREN followed by STAR_OP or PLUS_OP

	; eax <- last(v)
	mov	eax, ecx
	xor	edx, edx

	; FOR(all positions P in last(v))
	;       follow[P] <- follow[P] | first(v)

  .init_paren_follow:
	; eax == last(v)
	; ebx == first(v)
	; ecx == last(v)
	; edx == position * 4
	shr	eax, 1
	jnc  .next_paren_pos
	or	[edi+follow@REGEX_T+edx], ebx
	test	eax, eax

   .next_paren_pos:
	jz   .paren_follow_done
	add	edx, 4
	jmp  .init_paren_follow
	;--------------------

   .paren_follow_done:
	; eax == 0
	; ebx == first(v)
	; ecx == last(v)
	; esi == pointer to "*" or "?" or "+"
	; edi == pointer to REGEX_T

	; empty(v) <- FALSE
	mov	edx, eax
	cmp	byte[esi], "+"
	mov	eax, [token]
	; IF("+" operator)
	;    THEN empty(v) <- FALSE
	;    ELSE empty(v) <- TRUE
	setnz	dl
	inc	esi

  .have_stack_atom:
	; eax == stack TOKEN
	; ebx == first(v)
	; ecx == last(v)
	; edx == empty(v)
	; esi == pointer to next input character
	; edi == pointer to REGEX_T
	test	eax, ATOM
	jnz  .do_factor

	; assert stack integrity
	test	eax, (EMPTY or UNION_OP or LPAREN)
	jz   .error_unknown

	; shift ATOM
	sub	esp, 16
	mov	eax, ATOM
	mov	[empty], edx
	mov	[last],  ecx
	mov	[first], ebx
	mov	[token], eax
	jmp  .input
	;====================

 .backslash:
	cmp	byte[esi], 0
	jz   .error_trail_backslash
	call	regcvtesc
	; eax == converted character (esc_seq2ASCII)
	; esi == pointer to next input character
	mov	ebx, eax
	jmp  .character
	;====================

 .lparen:
	sub	esp, 4
	inc	dword[lparens]
	mov	dword[token], LPAREN
	jmp  .input
	;====================

 .check_stack:
	; eax == 0 (SRE-terminator)
	cmp	[lparens], eax
	jnz  .error_open_paren
	test	dword[token], EMPTY
	jnz  .error_empty_sre
	test	dword[token], ATOM
	jz   .error_unknown

	; pop ATOM
	mov	ebx, [first]
	mov	ecx, [last]
	mov	edx, [empty]
	add	esp, 16
	test	dword[token], UNION_OP
	jz   .must_be_empty

	; pop UNION_OP
	add	esp, 4

	; do union
	or	ebx, [first]
	or	ecx, [last]
	or	edx, [empty]

	; pop ATOM
	add	esp, 16

  .must_be_empty:
	test	dword[token], EMPTY
	jz   .error_unknown
	; eax == 0 (SRE-terminator)
	; ebx == first(SRE)
	; ecx == last(SRE)
	; edx == empty(SRE)
	; esi == pointer to character behind terminating 0
	; edi == pointer to REGEX_T
	mov	eax, [edi+atoms@REGEX_T]
	mov	[saved], esi
	shl	edx, 1
	mov	[edi+first@REGEX_T], ebx
	mov	[edi+last@REGEX_T],  ecx
	or	[edi+props@REGEX_T], edx
	; eax <- highest assigned position (int)
	dec	eax
	lea	esi, [edi+follow@REGEX_T]
	mov	ecx, eax

   .init_search_loop:
	or	[esi+ecx*4], ebx
	dec	ecx
	jge  .init_search_loop

	cmp	eax, 8
	jb   .build_dfa

	; zero-initialize the hashtable
	lea	edi, [edi+transmap@REGEX_T]
	mov	ecx, (REGEX_HASHSIZE shl 1)
	xor	eax, eax
	rep  stosd
	inc	eax
	mov	esi, [saved]
	mov	esp, ebp
	dec	esi
	mov	ebp, [esp]
	add	esp, 12
	retn 8
	;====================

  .build_dfa:
	sub	esp, 4
	xor	ecx, ecx
	mov	ebx, [edi+first@REGEX_T]
	dec	ecx
	lea	esi, [edi+transmap@REGEX_T]
	xor	edx, edx

	; initialize the bottom of the "recursion" stack
	mov	[esp], esp

	; ebx == set of states reachable from the active states
	; edx == set of active states (== 0)
	; that means: if no state is active (when there was no transition
	;             possible) then the reachable states are first(SRE)
	;             (restart/reset the automaton to the first states)
	jmp  .reachable_collected
	;--------------------

   .init_transmap:
	mov	edx, eax	; save active_states mask
	xor	ebx, ebx	; reachable states
	xor	ecx, ecx	; bit position counter -- offset into follow

   .collect_reachable:
	shr	eax, 1
	jnc  .next_pos
	or	ebx, [edi+follow@REGEX_T+ecx]
	test	eax, eax

    .next_pos:
	jz   .reachable_collected
	add	ecx, 4
	jmp  .collect_reachable
	;--------------------

   .reachable_collected:
	; ebx == set of states reachable from the active_states
	; edx == set of active_states
	mov	ecx, (255 * 4)
	; transmap[active_states] <- reachable from active_states
	mov	[esi+edx], bl

   .for_all_chars:
	mov	eax, [edi+charpos@REGEX_T+ecx]
	xor	edx, edx
	and	eax, ebx
	; eax == 0 --> none of the reachable states (positions) contains
	;              the current character
	jz   .next_char
	cmp	[esi+eax], dl
	jnz  .next_char
	push	ebx	; reachable_states
	push	ecx	; current_char
	jmp  .init_transmap
	;--------------------

    .next_char:
	sub	ecx, 4
	jge  .for_all_chars

	; all characters tested on reachable_states

	; test if we are done
	cmp	esp, [esp]
	jz   .dfa_built

     .pop_rec_stack:
	pop	ecx	; current_char
	pop	ebx	; reachable_states
	sub	ecx, 4	; next char
	jge  .for_all_chars

	; test if we are done
	cmp	esp, [esp]
	jnz  .pop_rec_stack

   .dfa_built:
	; bottom of stack reached
	xor	eax, eax
	add	esp, 4
	inc	eax
	mov	esi, [saved]
	; eax == 1 == REGEX_DFA
	or	[edi+props@REGEX_T], eax

 .return:
	mov	esp, ebp
	dec	esi
	mov	ebp, [esp]
	add	esp, 12
	retn  8
	;====================

 .error_state_overflow:
	; too many character positions in SRE (more than 32)
	xor	eax, eax
	mov	ecx, err_states_str
	mov	edx, err_states_len
	jmp  .return
	;--------------------

 .error_empty_sre:
	; the empty string (i.e., only a terminating 0) was passed
	dec	esi
	mov	ecx, err_null_sre_str
	mov	edx, err_null_sre_len
	xor	eax, eax
	jmp  .return
	;--------------------

 .error_inv_char:
	; read an invalid character, i.e., a character > 127 (ASCII)
	; eax   == invalid character
	dec	esi
	mov	ecx, err_inv_char_str
	mov	edx, err_inv_char_len
	xor	eax, eax
	jmp  .return
	;--------------------

 .error_unknown:
	; stack TOKEN is unknown, this is a bug (internal error)
	mov	ecx, err_internal_str
	mov	edx, err_internal_len
	xor	eax, eax
	jmp  .return
	;--------------------

 .error_un_op:
	; misplaced * or ? or +
	dec	esi
	mov	ecx, err_un_op_str
	mov	edx, err_un_op_len
	xor	eax, eax
	jmp  .return
	;--------------------

 .error_open_charset:
	; read 0 terminator while lexing/parsing a character set []
	dec	esi
	mov	ecx, err_open_cset_str
	mov	edx, err_open_cset_len
	xor	eax, eax
	jmp  .return
	;--------------------

 .error_inv_range:
	; invalid range expression in character set []:
	;  (1) start > end,  e.g., x-d
	;  (2) double range, e.g., a-f-z
	mov	ecx, err_inv_range_str
	mov	edx, err_inv_range_len
	xor	eax, eax
	jmp  .return
	;--------------------

 .error_empty_charset:
	; empty character set ([^\x00-\xFF])
	mov	ecx, err_empty_cset_str
	mov	edx, err_empty_cset_len
	xor	eax, eax
	jmp  .return
	;--------------------

 .error_union_op:
	; misplaced |
	dec	esi
	mov	ecx, err_union_op_str
	mov	edx, err_union_op_len
	xor	eax, eax
	jmp  .return
	;--------------------

 .error_rparen:
	; misplaced ):
	;  (1) ()
	;  (2) |)
	dec	esi
	mov	ecx, err_rparen_str
	mov	edx, err_rparen_len
	xor	eax, eax
	jmp  .return
	;--------------------

 .error_open_paren:
	; missing )
	mov	ecx, err_open_paren_str
	mov	edx, err_open_paren_len
	xor	eax, eax
	jmp  .return
	;--------------------

 .error_trail_backslash:
	; trailing \
	dec	esi
	xor	eax, eax
	mov	ecx, err_trail_bs_str
	mov	edx, err_trail_bs_len
	jmp  .return
	;--------------------

	restore pattern
	restore regex_t
	restore lparens
	restore saved
	restore prev
	restore first
	restore last
	restore empty
	restore token

	restore EMPTY
	restore ATOM
	restore UNION_OP
	restore LPAREN


;============================================================================
;
;   @name   : regcvtesc
;
;   @type   : PROC
;
;   @desc   : Converts an escape sequence to an 8-bit ASCII code.
;
;   @params : esi == pointer to character behind "\" in the input buffer
;
;   @return : eax == 8-bit ASCII code of the character specified by the
;                    escape sequence
;             esi == pointer to character behind the escape sequence
;
;   @regs   : all non-return registers are preserved
;
;   @uses   : @file ctypedef.inc
;             @file ctypemap.inc
;               @var ctype_map - a 128 byte character classification table
;
;============================================================================

regcvtesc:
	push	edx
	movzx	eax, byte[esi]
	cmp	eax, "x"
	jz   .hex_value
	cmp	eax, "X"
	jz   .hex_value
	test	byte[ctype_map+eax], DIGIT
	jnz  .octal_value
	test	byte[ctype_map+eax], LOWER
	mov	edx, eax
	jz   .done
	mov	eax, 7
	cmp	edx, 'a'
	jz   .done
	inc	eax
	cmp	edx, 'b'
	jz   .done
	inc	eax
	cmp	edx, 't'
	jz   .done
	inc	eax
	cmp	edx, 'n'
	jz   .done
	inc	eax
	cmp	edx, 'v'
	jz   .done
	inc	eax
	cmp	edx, 'f'
	jz   .done
	inc	eax
	cmp	edx, 'r'
	jz   .done
	mov	eax, edx
	pop	edx
	inc	esi
	retn
	;====================

  .octal_value:
	cmp	eax, "7"
	ja   .done
	sub	eax, "0"
	movzx	edx, byte[esi+1]
	test	byte[ctype_map+edx], DIGIT
	jz   .done
	cmp	edx, "7"
	ja   .done
	sub	edx, "0"
	shl	eax, 3
	inc	esi
	add	eax, edx
	movzx	edx, byte[esi+1]
	test	byte[ctype_map+edx], DIGIT
	jz   .done
	cmp	edx, "7"
	ja   .done
	sub	edx, "0"
	shl	eax, 3
	inc	esi
	add	eax, edx
	and	eax, 0FFh
	pop	edx
	inc	esi
	retn
	;====================

  .hex_value:
	movzx	edx, byte[esi+1]
	test	byte[ctype_map+edx], XDIGIT
	jz   .done

	mov	eax, edx
	inc	esi
	test	byte[ctype_map+edx], DIGIT
	jnz  .hdigit1
	test	byte[ctype_map+edx], LOWER
	jz   .upper_hchar1
	sub	eax, ("a" - "A")

   .upper_hchar1:
	sub	eax, 7

   .hdigit1:
	sub	eax, "0"
	movzx	edx, byte[esi+1]
	test	byte[ctype_map+edx], XDIGIT
	jz   .done
	inc	esi
	test	byte[ctype_map+edx], DIGIT
	jnz  .hdigit2
	test	byte[ctype_map+edx], LOWER
	jz   .upper_hchar2
	sub	edx, ("a" - "A")

   .upper_hchar2:
	sub	edx, 7

   .hdigit2:
	sub	edx, "0"
	shl	eax, 4
	add	eax, edx

 .done:
	pop	edx
	inc	esi
	retn
	;====================


;============================================================================
;
;   @name  : regexec
;
;   @type  : PROC, STDCALL
;
;   @desc  : Deterministic regular expression search.
;            (acceptor, leftmost shortest match).
;
;   @param : regex_t == pointer to an initialized REGEX_T struc
;            string  == pointer to 0-terminated text to search for pattern
;
;   @return: eax == 0 if not found
;            ---------------------
;            eax != 0 if found
;             esi == pointer to the character after a match has been confirmed
;
;   @regs  : all regs destroyed
;
;   @pre   : The REGEX_T struc must have been initialized by regcomp.
;
;==============================================================================

; regexec(REGEX_T *regex_t, char *string)


	string	equ  esp+8
	regex_t equ  esp+4
regexec:

	xor	eax, eax
	mov	edi, [regex_t]
	mov	esi, [string]
	inc	eax
	; test if the language of the automaton accepts the empty word
	test	dword[edi+props@REGEX_T], REGEX_EMPTY
	jnz  .match
	test	dword[edi+props@REGEX_T], REGEX_DFA
	jz   .init_hash_dfa_search

	; small dfa with deterministic transmap

	dec	eax
	lea	edx, [edi+transmap@REGEX_T]

 .small_dfa_search:
	movzx	ecx, byte[esi]
	; eax <- set of states reachable from the currently active states
	;        (no matter by which character)
	movzx	eax, byte[edx+eax]
	shl	ecx, 2
	; test for end of the input
	jz   .no_match
	; do transition on the current char
	and	eax, [edi+charpos@REGEX_T+ecx]
	; advance input pointer
	inc	esi
	; test if an accepting (final) state has been reached
	test	eax, [edi+last@REGEX_T]
	jz   .small_dfa_search
	; an accepting (final) state has been reached
	; eax != 0
	retn 8
	;====================


 .init_hash_dfa_search:
	mov	eax, [edi+first@REGEX_T]

  .hash_dfa_search:
	movzx	ecx, byte[esi]
	; "efficient" NOP
	mov	eax, eax
	; test for end of input
	shl	ecx, 2
	jz   .no_match
	; advance input pointer
	inc	esi
	; do transition on the current char
	and	eax, [edi+charpos@REGEX_T+ecx]
	jz   .init_hash_dfa_search
	; test if an accepting (final) state has been reached
	test	eax, [edi+last@REGEX_T]
	jnz  .match
	; hashtable lookup for states reachable from the current states
	mov	ecx, REGEX_HASHSIZE
	mov	ebx, eax
	xor	edx, edx
	div	ecx
	mov	eax, edx
	xor	ecx, ecx
	; eax <- offset of hash table entry
	shl	eax, 3
	; edx <- offset of hash table entry
	shl	edx, 3

  .linear_probing:
	cmp	[edi+transmap@REGEX_T+edx], ebx
	jz   .get_follow
	cmp	[edi+transmap@REGEX_T+edx], ecx
	jz   .construct_follow
	; edx <- offset of next hashtable entry
	add	edx, 8
	; test if all entries already have been searched (after turn around)
	; (eax == offset of first accessed hashtable entry)
	cmp	edx, eax
	jz   .construct_follow
	; test for end of hashtable
	cmp	edx, (REGEX_HASHSIZE * 8)
	jb   .linear_probing
	; turn around at end of hashtable (to start of hashtable)
	xor	edx, edx
	; this extra test is necessary because if the first access was at 0
	; then we would go into an infinite loop (if the hashtable is full)
	test	eax, eax
	jz   .construct_follow
	jmp  .linear_probing
	;--------------------

  .get_follow:
	; eax <- states reachable from the current states
	mov	eax, [edi+transmap@REGEX_T+edx+4]
	jmp  .hash_dfa_search
	;--------------------

  .construct_follow:
	; on the fly subset construction

	; eax == index of first accessed hash entry
	; ebx == mask of active states
	; ecx == 0
	; edx == hash table entry offset
	; edi == pointer to REGEX_T
	; esi == pointer to next input character

	mov	eax, ebx
	; store key (== current states)
	mov	[edi+transmap@REGEX_T+edx], ebx
	mov	ebx, ecx

	test	eax, 0FFFFh
	jz   .advance16
	test	eax, 0FFh
	jz   .advance8
	test	eax, 0Fh
	jz   .advance4

    .test_pos:
	shr	eax, 1
	jnc  .next_pos
	or	ebx, [edi+follow@REGEX_T+ecx]
	test	eax, eax

    .next_pos:
	jz   .set_follow
	add	ecx, 4
	jmp  .test_pos
	;--------------------

   .set_follow:
	; store val (== reachable states)
	mov	[edi+transmap@REGEX_T+edx+4], ebx
	; eax <- states reachable from the current states
	mov	eax, ebx
	jmp  .hash_dfa_search
	;--------------------

   .advance16:
	shr	eax, 16
	add	ecx, 64
	jmp  .test_pos
	;--------------------

   .advance8:
	shr	eax, 8
	add	ecx, 32
	jmp  .test_pos
	;--------------------

   .advance4:
	shr	eax, 4
	add	ecx, 16
	jmp  .test_pos
	;--------------------

 .no_match:
	xor	eax, eax

 .match:
	retn 8
	;====================

	restore regex_t
	restore string