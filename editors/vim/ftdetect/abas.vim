" Vim filetype detection for Abas source files.
"
" Abas programs use the .abas extension. The autocommand is put in a named
" group so that sourcing this file twice cannot stack duplicates, and
" setfiletype is used so a filetype the user already set is never overridden.

augroup abas_filetype
  autocmd!
  autocmd BufNewFile,BufRead *.abas setfiletype abas
augroup END
