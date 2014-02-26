function! s:insert_include_guards()
  let uuid = substitute(system("uuidgen | tr 'a-z-' 'A-Z_'"), "\n", "", "")
  let name = "B_HEADER_GUARD_" . uuid
  execute "normal! i#ifndef " . name . "\n#define " . name . "\n#endif"
  normal! k
endfunction
autocmd! BufNewFile *.h call <SID>insert_include_guards()
