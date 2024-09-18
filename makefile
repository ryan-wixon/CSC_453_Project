make: phase1.c phase1.h process.c process.h
	gcc -Wall -lm phase1.c process.c -o phase1
clean:
	rm phase1
	
