CXX = g++

CFLAGS := -g
CFLAGS += -std=c++17 -pedantic
CFLAGS += -Wextra -Wall -Wshadow -Wconversion

CFLAGS += -ftrapv -fverbose-asm
CFLAGS += -Wundef -Wcast-align=strict -Wstrict-overflow=5
CFLAGS += -Wwrite-strings -Wcast-qual -Wformat=2
CFLAGS += -Wswitch-enum -Wunreachable-code
CFLAGS += -pthread

FAST_CFLAGS := -O3 -std=c++17 -pedantic -pthread

BENCH_CFLAGS := -pg -fprofile-arcs -ftest-coverage -pthread

CFLAGS_END := -lncurses

DIRTY := *.gcda *.gcno *.gcov *.out error vgcore.*
DIRTY += *.tab.c *.tab.h lex.yy.c y.dot y.output

src := dijk.cpp floor.cpp gen.cpp rand.cpp opal.cpp parse.cpp turn.cpp
hdr = dijk.h floor.h gen.h globs.h parse.h rand.h turn.h
hdr += parse.l parse.y

src_nodep := lex.yy.c y.tab.c

opal: $(src) $(hdr)
	lex --fast parse.l
	yacc -d -l parse.y
	$(CXX) $(FAST_CFLAGS) -o opal.out $(src) $(src_nodep) $(CFLAGS_END)

debug: $(src) $(hdr)
	lex -v -d parse.l
	yacc -v -d parse.y
	$(CXX) $(CFLAGS) -o opal.out $(src) $(src_nodep) $(CFLAGS_END)

bench: $(src) $(hdr)
	lex -d parse.l
	yacc -d parse.y
	$(CXX) $(BENCH_CFLAGS) -o opal.out $(src) $(src_nodep) $(CFLAGS_END)

clean:
	rm -f $(DIRTY)

val:
	valgrind -v --leak-check=full --show-leak-kinds=all --track-origins=yes --log-file=error ./opal.out -z 3656437442
