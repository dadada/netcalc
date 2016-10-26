FILE = netcalc

all: $(FILE)

.PHONY: clean
clean: 
	rm netcalc

$(FILE): netcalc.c
	gcc $^ -o $@
