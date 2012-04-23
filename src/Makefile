NAME = ponies
CFLAGS = -Wall -ggdb -DUSE_DEBUG
LDFLAGS = -lpthread -lsqlite3

OBJS = main.o master.o ydecode.o conf.o misc.o list.o nzb.o nntp_wrap.o    \
       queue.o download.o message.o stats.o dispatch.o dlworker.o yinfo.o  \
       crc32.o sock.o nntp.o socketpool.o filepool.o ui.o sqwraps.o

CC = gcc

all: $(NAME) 

%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@
	
clean:
	rm -f *.o $(NAME) $(CL_NAME)
