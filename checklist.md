# Rash checklist

- [x] Command history
- [x] Current working directory
- [x] PATH searching: check directories in PATH for executables
- [x] Relative and absolute path resolution for commands and executables
- [ ] Builtin commands:
    - [x] cd
    - [x] exit
    - [x] prevd     cycle to the previous directory
    - [x] nextd     cycle to the next directory
    - [x] dirh      show directory history
- [x] prevd and nextd: cycle through a history of directories
    - [ ] also bind this to keys
- [ ] Cycle through command history
    - [ ] also bind this to commands
- [ ] Proper tokenizer
    - [x] Allow quoted arguments
        - [x] empty string
    - [ ] Allow escape characters
- [ ] input redirection '<'
- [ ] output redirection '>'
- [ ] piping
- [ ] Builtin keybindings for the most useful commands
- [ ] Environment variable handling
- [ ] Background jobs with &
- [ ] Should relative and absolute pathing should be resolved at the tokenization/parsing level?








c1: 0, 2, file

ls
-a
idiot moron
>
file






ls -a > file > rat; cd ..

ls       command 0, as = 0, ae = 0
-a       c = 0, as = 0, ae = 1
>        
file     c0.fd = file
>
rat      c0.fd = rat
;        command 1
cd
..
