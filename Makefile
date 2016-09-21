CC = gcc
CFLAGS = -Wall -g -O0
CFLAGS = -I.

#targets
all: ac400FWUpgrade

ac400FWUpgrade: ac400FWUpgrade.o mdioUtils.o
	$(CC) -o ac400FWUpgrade ac400FWUpgrade.o mdioUtils.o

ac400FWUpgrade.o: ac400FWUpgrade.c
	$(CC) $(CFLAGS) -c ac400FWUpgrade.c

mdioUtils.o: mdioUtils.c
	$(CC) $(CFLAGS) -c mdioUtils.c


clean:
	rm -rf *.o ac400FWUpgrade
