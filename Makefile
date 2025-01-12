
relays: relays.c
	clang -o relays -l wiringPi -D MG_ENABLE_IPV6=1 -I mongoose mongoose/mongoose.c relays.c
all: relays

clean:
	rm relays
