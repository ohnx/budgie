OUTPUT:=budgie
CFLAGS+=-Wall -Werror -Iinclude/ -ansi -pedantic -Idist/librudolph/include -DRUDOLF_USE_STDLIB
LIBS=rudolph
LDFLAGS+=-Ldist/
SRCS:=$(wildcard src/*.c)
OBJS:=$(patsubst src/%.c, objs/%.o, $(SRCS))
LDFLAGS+=$(patsubst %, -l%, $(LIBS))

.PHONY: default
default: dist/librudolph.a
default: $(OUTPUT)

dist/librudolph.a:
	make -C dist/librudolph/
	cp dist/librudolph/librudolph.a dist/

objs/%.o: src/%.c
	@mkdir -p objs/
	$(CC) -c -o $@ $< $(CFLAGS)

$(OUTPUT): $(OBJS)
	$(CC) $^ -o $(OUTPUT) $(CFLAGS) $(LDFLAGS)

.PHONY: clean
clean:
	rm -rf objs/ $(OUTPUT)

.PHONY: debug
debug: CFLAGS += -D__DEBUG -g -O0
debug: default
