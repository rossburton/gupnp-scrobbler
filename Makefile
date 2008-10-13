CFLAGS = `pkg-config --cflags --libs gupnp-1.0 libxml-2.0 libnotify` -g -Wall

upnp-scrobbler:

clean:
	rm -f upnp-scrobbler
