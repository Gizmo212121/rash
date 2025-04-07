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
    - [x] path      displays paths in PATH if given no args. Otherwise, attempts to add all additional arguments as paths to PATH
- [x] prevd and nextd: cycle through a history of directories
    - [ ] also bind this to keys
- [ ] Cycle through command history
    - [ ] also bind this to commands
- [ ] Proper tokenizer
    - [x] Allow quoted arguments
        - [x] empty string
    - [ ] Allow escape characters
- [ ] input redirection '<'
- [x] output redirection '>'
- [ ] piping
- [ ] Builtin keybindings for the most useful commands
- [ ] Environment variable handling
- [ ] Background jobs with &
- [ ] Implement tilde-expansion

- [ ] Should relative and absolute pathing should be resolved at the tokenization/parsing level?
    - It would reduce a lot of repetion for built in commands
    - What about relative/absolute pathing for commands in A
    - If at the tokenization level, would an error be able to tell you which caused it?
        - Yes, you just printf the word at the current index minus one (given we're currently on the path token)
            So if tokens contains: [ls, /baihtpiiiheapit], since the path token contains an invalid path, you can just index to the previous token to printf("%s: invalid path\n", tokens[i - 1])
            - But what about something like mkdir? You can't validate paths at the tokenization level for this function because the idea is it takes CREATES the directory if the path doesn't exist (given the correct flags)
            - The idea is, when a command gets a path, I don't want the command to have to validate whether the syntax of the path is correct or to perform any pathname expansions. It should be given a syntactically-perfect path
              that may or may not exist (THAT is up to the command to determine)
                - Thus, I need a function that can validate the syntax of a path, resolve exansions (., .., ~), WITHOUT checking whether the path exists
                - In summary, I need a function that does syntactic path validation and resolution, not semantic path validation
                - Actually, error handling would be harder this way


- [ ] When using input redirection (meta: ./rash < file), gives invalid argument for getline
