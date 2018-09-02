httpd: httpd.c
	gcc -o $@ $^ -pthread

.PHONY: clean

clean:
	rm httpd
