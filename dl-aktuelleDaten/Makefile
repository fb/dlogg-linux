EXECUTABLES = dl-aktuelle-datenx dl-aktuelle-datenx-no-curses

all: $(EXECUTABLES)

dl-aktuelle-datenx: dl-aktuelle-datenx.c ../dl-lesen.h
		gcc -o dl-aktuelle-datenx dl-aktuelle-datenx.c -lncurses -lpanel

dl-aktuelle-datenx-no-curses: dl-aktuelle-datenx-no-curses.c ../dl-lesen.h
		gcc -o dl-aktuelle-datenx-no-curses dl-aktuelle-datenx-no-curses.c

clean:
	rm -v $(EXECUTABLES)


#fuer SuSE Linux:
#gcc -I/usr/include/ncurses -o dl-aktuelle-datenx dl-aktuelle-datenx.c -lncurses -lpanel
