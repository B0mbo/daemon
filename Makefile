#
# filestatus makefile
#

GCC=g++
CLINK=-Wall -std=c++11 -lpthread -lz
PRJNAME=file_status

all:
	$(GCC) -o ./bin/$(PRJNAME) ./src/*.cpp $(CLINK)

clean:
	rm -v ./bin/$(PRJNAME)
