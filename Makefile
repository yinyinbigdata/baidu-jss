$CC=gcc
$CFLAGS=-O0 -g2
all: scheduler deq enq stat
	
scheduler: 
	$C -O0 -g2 -o scheduler scheduler.c

deq:
	$C -O0 -g2 -o deq deq.c client.c

enq:
	$C -O0 -g2 -o enq enq.c client.c

stat:
	$C -O0 -g2 -o stat stat.c client.c
	
clean:
	rm -rf scheduler deq enq stat