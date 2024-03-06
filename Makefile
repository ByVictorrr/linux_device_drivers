obj-m += scull_main.o

all:
	make -C /usr/src/$(shell uname -r)  M=$(PWD) modules

clean:
	make -C /usr/src/$(shell uname -r) M=$(PWD) clean