
; Character classification table - to be included into the data section.

;CNTRL   equ   1
;BLANK   equ   2
;SPACE   equ   4
;LOWER   equ   8
;UPPER   equ  16
;DIGIT   equ  32
;XDIGIT  equ  64
;PUNCT   equ 128

;ALPHA   equ (LOWER or UPPER)
;ALNUM   equ (ALPHA or DIGIT)
;GRAPH   equ (ALNUM or PUNCT)


; 7-bit ASCII table (128 bytes)

IF USED ctype_map

LABEL	ctype_map BYTE
	times  9  db (CNTRL)			; 0 - 8
		  db (CNTRL or BLANK or SPACE)	; 9
	times  4  db (CNTRL or SPACE)		; 10 - 13
	times 18  db (CNTRL)			; 14 - 31
		  db (BLANK or SPACE)		; 32
	times 15  db (PUNCT)			; 33 - 47
	times 10  db (DIGIT or XDIGIT)		; 48 - 58
	times  7  db (PUNCT)			; 59 - 64
	times  6  db (UPPER or XDIGIT)		; 65 - 71
	times 20  db (UPPER)			; 72 - 90
	times  6  db (PUNCT)			; 91 - 96
	times  6  db (LOWER or XDIGIT)		; 97 - 102
	times 20  db (LOWER)			; 103 - 122
	times  4  db (PUNCT)			; 123 - 126
		  db (CNTRL)			; 127

END IF