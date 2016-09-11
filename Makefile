all: mojoloader

mojoloader: mojoloader.c
	$(CC) -std=gnu99 -o $@ $<

clean:
	$(RM) mojoloader
