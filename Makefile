

exectrace: exectrace.c
	   gcc exectrace.c -g -o exectrace -lbfd 
install:
	-cp exectrace /usr/bin/
