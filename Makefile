FILE = netcalc

all: $(FILE) tests

.PHONY: clean
clean: 
	rm netcalc test/results

$(FILE): $(FILE).c
	gcc $^ -o $@

tests:
	./netcalc -c < test/cases > test/results
	diff test/results test/expected
