CC = gcc
CFLAGS = -Wall -g -O0
CFLAGS = -I.

#targets
all: ac400FWUpgrade ac400FWUpgradeUtil ac400FWImageSwap

ac400FWImageSwap: ac400FWImageSwap.o mdioUtils.o
	$(CC) -o ac400FWImageSwap ac400FWImageSwap.o mdioUtils.o

ac400FWUpgrade: ac400FWUpgrade.o mdioUtils.o
	$(CC) -o ac400FWUpgrade ac400FWUpgrade.o mdioUtils.o

ac400FWUpgradeUtil: ac400FWUpgradeUtil.o mdioUtils.o
	$(CC) -o ac400FWUpgradeUtil ac400FWUpgradeUtil.o mdioUtils.o

ac400FWUpgradeUtil.o: ac400FWUpgradeUtil.c
	$(CC) $(CFLAGS) -c ac400FWUpgradeUtil.c

ac400FWImageSwap.o: ac400FWImageSwap.c
	$(CC) $(CFLAGS) -c ac400FWImageSwap.c

ac400FWUpgrade.o: ac400FWUpgrade.c
	$(CC) $(CFLAGS) -c ac400FWUpgrade.c

mdioUtils.o: mdioUtils.c
	$(CC) $(CFLAGS) -c mdioUtils.c

clean:
	rm -rf *.o ac400FWUpgrade ac400FWUpgradeUtil ac400FWImageSwap
