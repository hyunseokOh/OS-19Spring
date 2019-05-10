# test code compile
CC = arm-linux-gnueabi-gcc
ERR = error_test.c
FORK = simple_fork.c
IO = io.c
SLEEP = sleep.c
SIMPLE = simple_change.c

all: error fork io sleep simple

error: $(ERR)
	$(CC) $(ERR) -o ./bin/$@

fork: $(FORK)
	$(CC) $(FORK) -o ./bin/$@

io: $(IO)
	$(CC) $(IO) -o ./bin/$@

sleep: $(SLEEP)
	$(CC) $(SLEEP) -o ./bin/$@

simple: $(SIMPLE)
	$(CC) $(SIMPLE) -o ./bin/$@

clean:
	rm ./bin/*
