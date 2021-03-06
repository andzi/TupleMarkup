## Tuple Markup Language Specification

TML is a minimalist all-purpose markup language: nested lists with bracket-minimizing syntax.

This file provides a formal language specification intended to guide TML parser implementations.
It is NOT meant as a tutorial or manual for TML - refer to the main TML documentation for that.

### Line Comments

Keep in mind the EBNF grammar below does not take `||` line comments
into account. It's assumed that these will be removed as though by a preprocessor:
Any character sequence `||` (excepting `\||`) found within the code should be
ignored, plus every character up to and including the next line-feed character.
	

### EBNF Grammar Specification (Not Including Line Comments)

	syntax = grouped;

	grouped = (open, list, close) | (open, nested, close) ;
	nested = (list, divider, list) | (list, divider, nested) ;
	list = (item, {space}, list) | "" ;
	item = grouped | word ;

	open = {space}, "[", {space} ;
	close = {space}, "]", {space} ;
	divider = {space}, "|", {space} ;

	word = word-char, {word-char} ;
	word-char = "\s" | "\t" | "\r" | "\n" | "\[" | "\]" | "\|" | "\\" | "\*" | "\?"
			| (char - space - "[" - "]" - "|" - "\") ;

	space = ? white space characters ? ;
	char = ? all visible characters ? ;


### Escape Codes

Escape codes found within words should be converted to the appropriate UTF8/ASCII code they represent.

* `\s` : space character.
* `\t` : tab character.
* `\r` : carriage return character.
* `\n` : newline character.
* `\[` : "[" character.
* `\]` : "]" character.
* `\|` : "|" character.
* `\\` : "\" character.
* `\?` : ASCII 0x01
* `\*` : ASCII 0x02

### Informal Notes

TML parsers generate nested lists of lists or word strings. In the EBNF grammar
above, only the "list" and "word" nodes make up the resulting TML tree.

Words are contiguous streams of arbitrary characters except whitespace and
the reserved symbols `[`, `|`, `]`, and `\` (escape code). Therefore, words are
separated by whitespace or by a reserved symbol (whitespace is not included in
parsed words). Whitespace can be added to words only through escape codes
`\s`, `\t`, etc.

### License

Copyright (C) 2012 John Judnich. Released as open-source under The MIT Licence.
