all: clean iio_client run

iio_client: ad9361-iiostream.c
	$(CC) -o $@ $^ -liio
clean:
	rm -f *.o  iio_client
run:
	./iio_client
