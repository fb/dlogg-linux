EXECUTABLES = dl-aktuelle-daten dl-aktuelle-daten-no-curses dl-lesen

all: $(EXECUTABLES)

dl-aktuelle-datenx: dl-aktuelle-daten.c dl-lesen.h
		$(CC) -o dl-aktuelle-daten dl-aktuelle-daten.c -lncurses -lpanel

dl-aktuelle-datenx-no-curses: dl-aktuelle-daten-no-curses.c dl-lesen.h
		$(CC) -o dl-aktuelle-daten-no-curses dl-aktuelle-daten-no-curses.c

dl-lesenx: dl-lesen.c dl-lesen.h
	$(CC) -o dl-lesen dl-lesen.c

clean:
	rm -v $(EXECUTABLES)


#fuer SuSE Linux:
#gcc -I/usr/include/ncurses -o dl-aktuelle-datenx dl-aktuelle-datenx.c -lncurses -lpanel
