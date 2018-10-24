FILE = netcalc

all: $(FILE) tests

.PHONY: clean
clean: 
	rm $(FILE) test/results test/$(FILE).pid

$(FILE): $(FILE).c
	gcc $^ -o $@

test: $(FILE)
	./$(FILE) & echo $$! > test/$(FILE).pid
	./$(FILE) -c < test/cases > test/results
	kill `cat test/$(FILE).pid`
	diff test/results test/expected
