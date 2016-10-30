FILE = netcalc

all: $(FILE) tests

.PHONY: clean
clean: 
	rm $(FILE) netcalc test/results test/$(FILE).pid

$(FILE): $(FILE).c
	gcc $^ -o $@

tests: $(FILE)
	./$(FILE) & echo $$! > test/$(FILE).pid
	./$(FILE) -c < test/cases > test/results
	kill `cat test/$(FILE).pid`
	diff test/results test/expected
