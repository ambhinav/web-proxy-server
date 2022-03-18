default: proxy

proxy: proxy1.c
	gcc proxy1.c myqueue.c -pthread -o proxy

clean:
	rm proxy
