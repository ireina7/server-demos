CC = gcc
CFLAGS = -w -lpthread
LDIR = out

outdir:
	$(shell mkdir -p $(LDIR))


client: \
	outdir \
	common.c \
	test_client.c
	$(CC) $(CFLAGS) test_client.c common.c -o $(LDIR)/test_client.o

iterate: \
	outdir \
	common.c \
	iterate_server.c
	$(CC) $(CFLAGS) iterate_server.c common.c -o $(LDIR)/iterate_server.o

multiprocess: \
	outdir \
	common.c \
	multiprocess_server.c
	$(CC) $(CFLAGS) multiprocess_server.c common.c -o $(LDIR)/multiprocess_server.o

multithread: \
	outdir \
	common.c \
	multithread_server.c
	$(CC) $(CFLAGS) multithread_server.c common.c -o $(LDIR)/multithread_server.o

IOmultiplexing: \
	outdir \
	common.c \
	IOmultiplexing_server.c
	$(CC) $(CFLAGS) IOmultiplexing_server.c common.c -o $(LDIR)/IOmultiplexing_server.o

threadpool: \
	outdir \
	common.c \
	threadpool_server.c
	$(CC) $(CFLAGS) threadpool_server.c common.c -o $(LDIR)/threadpool_server.o

threadpoolmutex: \
	outdir \
	common.c \
	threadpoolmutex_server.c
	$(CC) $(CFLAGS) threadpoolmutex_server.c common.c -o $(LDIR)/threadpoolmutex_server.o

septhreadpool: \
	outdir \
	common.c \
	septhreadpool_server.c
	$(CC) $(CFLAGS) septhreadpool_server.c common.c -o $(LDIR)/septhreadpool_server.o

all: \
	client \
	iterate \
	multiprocess \
	multithread \
	IOmultiplexing \
	threadpool \
	threadpoolmutex \
	septhreadpool

clean:
	rm -rf $(LDIR)
