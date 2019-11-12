run:
	nasm -f elf64 my_print.asm
	g++ main.cpp my_print.o
	rlwrap ./a.out
clean:	
	rm my_print.o a.out
