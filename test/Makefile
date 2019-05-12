# test code compile
CC = arm-linux-gnueabi-gcc
ERR = error_test.c
FORK = simple_fork.c
IO = io.c
SLEEP = sleep.c
SIMPLE = simple_change.c
TRIAL = trial.c
RANDOM = random.c

all: error fork io sleep simple trial random
	cp copy2user.sh ./bin

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

trial: $(TRIAL)
	$(CC) $(TRIAL) -o ./bin/$@ -lpthread

random: $(RANDOM)
	$(CC) $(RANDOM) -o ./bin/$@ -lpthread

clean:
	rm ./bin/*
