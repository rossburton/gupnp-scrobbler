PKGS = gupnp-1.0 libxml-2.0 libnotify
CPPFLAGS = `pkg-config --cflags $(PKGS)` -g -Wall
LDFLAGS = `pkg-config --libs $(PKGS)`

upnp-scrobbler:

clean:
	rm -f upnp-scrobbler
