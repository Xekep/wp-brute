
; error messages returned by regcomp()

err_states_str		db "pattern too long",10
err_states_len		=  $ - err_states_str

err_null_sre_str	db "empty pattern",10
err_null_sre_len	=  $ - err_null_sre_str

err_inv_char_str	db "illegal (8-bit) character in pattern",10
err_inv_char_len	=  $ - err_inv_char_str

err_internal_str	db "internal error (bug)",10
err_internal_len	=  $ - err_internal_str

err_un_op_str		db "misplaced *, + or ?",10
err_un_op_len		=  $ - err_un_op_str

err_open_cset_str	db "unmatched []",10
err_open_cset_len	=  $ - err_open_cset_str

err_inv_range_str	db "invalid [] range",10
err_inv_range_len	=  $ - err_inv_range_str

err_empty_cset_str	db "empty []",10
err_empty_cset_len	=  $ - err_empty_cset_str

err_union_op_str	db "misplaced |",10
err_union_op_len	=  $ - err_union_op_str

err_rparen_str		db "misplaced )",10
err_rparen_len		=  $ - err_rparen_str

err_open_paren_str	db "unbalanced ()",10
err_open_paren_len	=  $ - err_open_paren_str

err_trail_bs_str	db "trailing \",10
err_trail_bs_len	=  $ - err_trail_bs_str
