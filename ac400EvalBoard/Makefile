RM=rm -f
RMFORCE=rm -rf 
SRCS=mdioUtils.go
export CGO_CDFLAGS+=-I.

.PHONY:	all

all: exe 

exe: $(SRCS)
	go build 

clean:guard
	$(RMFORCE) mdioUtils

