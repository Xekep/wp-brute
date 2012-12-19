
; definitions used with and by "ctypemap.inc"

;PRINT      = GRAPH
;           | " ".

;GRAPH      = ALNUM
;           | PUNCT.

;ALNUM      = ALPHA
;           | DIGIT.

;ALPHA      = LOWER
;           | UPPER.

;PUNCT      = "!"|'"'|"#"|"%"|"&"|"'"|"("|")"|";"
;           | "<"|"="|">"|"?"|"["|"\"|"]"|"*"|"+"
;           | ","|"-"|"."|"/"|":"|"^"|"_"|"{"|"|"
;           | "}"|"~"|"`"|"@".

;DIGIT      = "1"|"2"|...|"9".

;LOWER      = "a"|"b"|...|"z".

;UPPER      = "A"|"B"|...|"C".

;CNTRL      = 0|1|...|31|127.

;BLANK      = " "|9.

;SPACE      = 9|10|11|12|13|" ".

;XDIGIT     = "a"|"b"|...|"f"
;           | "A"|"B"|...|"F"
;           | DIGIT.

CNTRL	equ   1
BLANK	equ   2
SPACE	equ   4
LOWER	equ   8
UPPER	equ  16
DIGIT	equ  32
XDIGIT	equ  64
PUNCT	equ 128

ALPHA	equ (LOWER or UPPER)
ALNUM	equ (ALPHA or DIGIT)
GRAPH	equ (ALNUM or PUNCT)

