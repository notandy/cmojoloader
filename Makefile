all: mojoloader

mojoloader: mojoloader.c
	$(CC) -std=gnu99 -o $@ $<

clean:
	$(RM) mojoloader

.PHONY: install
install: mojoloader
	install --mode=0755 --owner root --group root --dir $(DESTDIR)/bin
	install --mode=0755 --owner root --group root $^ $(DESTDIR)/bin
