SOURCES_ARGON=argon2/src/argon2.c argon2/src/core.c argon2/src/thread.c argon2/src/ref.c argon2/src/encoding.c argon2/src/blake2/blake2b.c
relays: relays.c
	clang -o relays -l wiringPi -lsystemd -g -lm -I argon2/include -D MG_ENABLE_IPV6=1 -I mongoose mongoose/mongoose.c relays.c ${SOURCES_ARGON}
all: relays

clean:
	rm relays
install:
	mkdir -p /opt/relays
	touch /opt/relays/users.csv
	install relays -o root -g root -m 755 /opt/relays
	install relays.service -o root -g root -m 644 /etc/systemd/system/relays.service
	find web_root -type f -exec install {} -o root -g root -m 644 -D /opt/relays/{} \;

uninstall:
	rm -rf /opt/relays
	systemctl stop relays.service || true
	rm /etc/systemd/system/relays.service
	systemctl daemon-reload

