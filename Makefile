
relays: relays.c
	clang -o relays -l wiringPi -D MG_ENABLE_IPV6=1 -I mongoose mongoose/mongoose.c relays.c
all: relays

clean:
	rm relays
install:
	mkdir -p /opt/relays
	install relays -o root -g root -m 755 /opt/relays
	install relays.service -o root -g root -m 644 /etc/systemd/system/relays.service
	find web_root -type f -exec install {} -o root -g root -m 644 -D /opt/relays/{} \;

uninstall:
	rm /opt/relays
	systemctl stop relays.service || true
	rm /etc/systemd/system/relays.service
	systemctl daemon-reload

