CFLAGS	+= -Wall -Werror
SRC	:= source/lu.c
TARGET	:= lu

all	: $(TARGET)

lu	: $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

install	: $(TARGET)
	install -m755 -D --target-directory=$(DESTDIR)/$(PREFIX)/sbin $(TARGET)
