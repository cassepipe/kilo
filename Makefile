kilo : kilo.c
	clang kilo.c -o kilo -Weverything -Wno-missing-prototypes -Wno-missing-variable-declarations -Wno-missing-noreturn -Wno-sign-conversion -std=c99
abappend : abappend.c 
	clang abappend.c -o abappend -Weverything -Wno-missing-prototypes -Wno-sign-conversion -std=c99
