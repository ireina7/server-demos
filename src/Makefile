CC = gcc
CFLAGS = -w -lpthread
LDIR = out
COMMONS = common.c utils.c

outdir:
	$(shell mkdir -p $(LDIR))


client: \
	outdir \
	$(COMMONS) \
	test_client.c
	$(CC) $(CFLAGS) test_client.c $(COMMONS) -o $(LDIR)/test_client.o

iterate: \
	outdir \
	$(COMMONS) \
	iterate_server.c
	$(CC) $(CFLAGS) iterate_server.c $(COMMONS) -o $(LDIR)/iterate_server.o

multiprocess: \
	outdir \
	$(COMMONS) \
	multiprocess_server.c
	$(CC) $(CFLAGS) multiprocess_server.c $(COMMONS) -o $(LDIR)/multiprocess_server.o

multithread: \
	outdir \
	$(COMMONS) \
	multithread_server.c
	$(CC) $(CFLAGS) multithread_server.c $(COMMONS) -o $(LDIR)/multithread_server.o

IOmultiplexing: \
	outdir \
	$(COMMONS) \
	IOmultiplexing_server.c
	$(CC) $(CFLAGS) IOmultiplexing_server.c $(COMMONS) -o $(LDIR)/IOmultiplexing_server.o

threadpool: \
	outdir \
	$(COMMONS) \
	threadpool_server.c
	$(CC) $(CFLAGS) threadpool_server.c $(COMMONS) -o $(LDIR)/threadpool_server.o

threadpoolmutex: \
	outdir \
	$(COMMONS) \
	threadpoolmutex_server.c
	$(CC) $(CFLAGS) threadpoolmutex_server.c $(COMMONS) -o $(LDIR)/threadpoolmutex_server.o

septhreadpool: \
	outdir \
	$(COMMONS) \
	septhreadpool_server.c
	$(CC) $(CFLAGS) septhreadpool_server.c $(COMMONS) -o $(LDIR)/septhreadpool_server.o

index.html: outdir
	cp index.html out/

all: \
	client \
	iterate \
	multiprocess \
	multithread \
	IOmultiplexing \
	threadpool \
	threadpoolmutex \
	septhreadpool \
	index.html

clean:
	rm -rf $(LDIR)
