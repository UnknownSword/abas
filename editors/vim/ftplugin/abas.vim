" Vim filetype plugin for Abas.
"
" Abas has no indentation rules of its own; whitespace is skipped by the lexer.
" What is set here is only what the editing tools need to work with the
" language's comments: a line comment starts with '#', and a block comment is
" opened and closed by '##'.

if exists("b:did_ftplugin")
  finish
endif
let b:did_ftplugin = 1

let s:cpo_save = &cpo
set cpo&vim

setlocal comments=:#
setlocal commentstring=#\ %s
setlocal formatoptions-=t
setlocal formatoptions+=cro

let b:undo_ftplugin = "setlocal comments< commentstring< formatoptions<"

let &cpo = s:cpo_save
unlet s:cpo_save
