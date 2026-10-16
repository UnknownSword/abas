" Vim syntax file
" Language:     Abas
" Maintainer:   The Abas project (https://github.com/UnknownSword/abas)
" Last Change:  2026-10-16
" Filenames:    *.abas
"
" This file follows the lexer in src/core/lexer.cpp and the command table in
" src/core/interp.cpp, which are the source of truth for the language. Abas has
" no reserved words in the lexer; its vocabulary is the set of commands the
" interpreter knows, and those are what stands out here.

if exists("b:current_syntax")
  finish
endif

let s:cpo_save = &cpo
set cpo&vim

" --------------------------------------------------------------- comments
" A '#' followed by another '#' opens a block comment that runs up to the next
" '##'; block comments do not nest. Any other '#' comments out the rest of its
" line. The line comment is written first on purpose: where the two would both
" start, the one written later wins, so '##' is read as a block comment.
syn match  abasComment display /#.*$/ contains=abasTodo
syn region abasBlockComment start=/##/ end=/##/ keepend contains=abasTodo
syn keyword abasTodo contained TODO FIXME XXX NOTE

" --------------------------------------------------------------- literals
" A string runs from one '"' to the next; a raw line end is allowed inside it.
" The escapes are \n \t \v \a \b \f \r \0 \\ \' \" and every other \X stands
" for the X alone, so any escaped character is one escape.
syn region abasString start=/"/ skip=/\\./ end=/"/ contains=abasEscape
syn match  abasEscape contained display /\\./

" A character literal holds exactly one character or one escape sequence.
syn match abasChar display /'\%(\\.\|[^'\\]\)'/ contains=abasEscape

" Numbers are whole 64 bit words only, in base ten, sixteen (0x), two (0b) and
" eight (0o). There is no floating point in Abas.
syn match abasNumber display /\<\%(0[xX][0-9a-fA-F]\+\|0[bB][01]\+\|0[oO][0-7]\+\|\d\+\)\>/

" --------------------------------------------------------------- commands
" func, flag and call are followed by a name, loop and endloop may carry one,
" and a jump (jmp, je, jne, jl, jg, jle, jge) is always walked to a flag or a
" func. The name after any of them reads like a function name does elsewhere.
syn keyword abasStatement func flag call loop endloop
      \ jmp je jne jl jg jle jge nextgroup=abasName skipwhite

" The rest of the commands, split into the ones that steer the program and the
" ones that work on its values. The names are the keys of the functions table
" in src/core/interp.cpp.
syn keyword abasStatement break cmp continue endfunc exit include ret
syn keyword abasKeyword add and clear clf clv cls dec div err get
syn keyword abasKeyword gcp grow gsp inc lsh mod mov mul nor not or
syn keyword abasKeyword pop pow print push put rpop rpush rsh ssp sub var xor

syn match abasName contained display /\h\w*/

" ------------------------------------------------------- operators, marks
" '$' points at a slot of the one stack, written $name, $3 or $$$name. The other
" operator characters the lexer knows are kept in one piece even though only
" '+' and '-' are read as the sign in front of a value.
syn match abasPointer  display /\$\+/
syn match abasOperator display /[+*\/%&|^<>=-]/
syn match abasDelimiter display /[,.:;()]/

" --------------------------------------------------------------- highlight
hi def link abasComment      Comment
hi def link abasBlockComment Comment
hi def link abasTodo         Todo
hi def link abasString       String
hi def link abasChar         Character
hi def link abasEscape       SpecialChar
hi def link abasNumber       Number
hi def link abasStatement    Statement
hi def link abasKeyword      Keyword
hi def link abasName         Function
hi def link abasPointer      Operator
hi def link abasOperator     Operator
hi def link abasDelimiter    Delimiter

let b:current_syntax = "abas"

let &cpo = s:cpo_save
unlet s:cpo_save
