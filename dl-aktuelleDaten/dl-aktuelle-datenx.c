/*****************************************************************************
 * Daten der UVR1611 lesen vom Datenlogger und im Winsol-Format speichern    *
 * read data of UVR1611 from Datanlogger and save it in format of Winsol     *
 * (c) 2006 - 2009 H. Roemer / C. Dolainsky                                  *
 *                                                                           *
 * This program is free software; you can redistribute it and/or             *
 * modify it under the terms of the GNU General Public License               *
 * as published by the Free Software Foundation; either version 2            *
 * of the License, or (at your option) any later version.                    *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU General Public License for more details.                              *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with this program; if not, see <http://www.gnu.org/licenses/>.      *
 *                                                                           *
 * Version 0.1		14.10.2006  erste Testversion                            *
 * Version 0.6a		26.10.2006                                               *
 * Version 0.6b		11.01.2007                                               *
 * Version 0.6c		22.01.2007                                               *
 * 					08.02.2007  Christoph:                                   *
 * 						select um Haenger zu vermeiden, verbessertes retry   *
 * 						(v.a. fuer USB)                                      *
 * 					10.02.2007  Holger:                                      *
 * 						erste Version mit funkionierendem IP Lesen           *
 * 						(socket open/close bei jedem Transfer)               *
 * 					15.02.2007  Christoph:                                   *
 * 						retry timer verbessert, csv-mode: output minimiert,  *
 * 						segfault bei falscher parametereingabe behoben       *
 * Version 0.7		27.02.2007                                               *
 * 					11.03.2007  Monatswechsel fuer csv-Datei eingefuegt      *
 * 					21.06.2007  Raumtemp. und digit. Pegel eingefuegt        *
 * 					22.06.2007  Zeitoption 0 eingefuegt -> einen Datensatz   *
 * 								lesen und Programmende                       *
 * 					22.10.2007  Unterstuetzung fuer UVR61-3                  *
 * 					   12.2007  Unterstuetzung 2DL-Modus                     *
 * Version 0.8.0	13.01.2008  Wechsel der Anzeige zwischen Geraet 1 und 2  *
 * 								mit Taste 'w'                                *
 * Version 0.8.1	24.01.2008  Fehlerkorrektur Momentanleistung in	         *
 * 								csv-Ausgabe                                  *
 * Version 0.8.2	25.02.2008  --rrd Unterstuetzung                         *
 * Version 0        xx.xx.2010  CAN-Logging                                  *
 * Version 0.9.3    26.11.2012                                               *
 * Version 0.9.4    05.01.2013  Anpassung CAN-Logging                        *
 * Version 0.9.5    24.02.2013  Ueberarbeitung USB-Zugriff                   *
*  $Id$               *
 *****************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <curses.h>
#include <panel.h>
#include <time.h>
#include <ctype.h>
#include <locale.h>
#include <getopt.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "../dl-lesen.h"

#define BAUDRATE B115200
#define BUFFER_SIZE 1024
#define UVR61_3 0x90
#define UVR1611 0x80

// #define DEBUG 4

extern char *optarg;
extern int optind, opterr, optopt;

void init_usb(void);
int start_socket(WINDOW *fenster1, WINDOW *fenster2);
int check_arg_getopt(int arg_c, char *arg_v[]);
int check_pruefsumme(void);
void print_pruefsumme_not_ok(UCHAR pruefz_berech, UCHAR pruefz_read);
void close_usb(void);
int do_cleanup(WINDOW *fenster1, WINDOW *fenster2);
int erzeugeLogfileName(int for_csv, char * pLogFileName, int anz_regler);
int open_logfile(char LogFile[], FILE **fp);
int close_logfile(FILE * fp);
void check_kennung(int received_Bytes);
int ip_handling(int sock);  /* IP-Zugriff - 05.02. neu */
void func_csv_output(int anz_regler, time_t daten_zeitpunkt, WINDOW *fenster1, WINDOW *fenster2);
int write_header2CSV(int regler, FILE *fp);
void write_CSVFile(int regler, FILE *fp, time_t datapoint_time);
void write_CSVCONSOLE(int regler, time_t datapoint_time);
void write_rrd(int regler);
void write_list(int regler);
void berechne_werte(int anz_regler);
void temperaturanz(int regler);
void ausgaengeanz(int regler);
void drehzahlstufenanz(int regler);
void waermemengenanz(int regler);
void testfunktion(void);
void zeitstempel(void);
float berechnetemp(UCHAR lowbyte, UCHAR highbyte, int sensor);
float berechnevol(UCHAR lowbyte, UCHAR highbyte);
int eingangsparameter(UCHAR highbyte);
int clrbit( int word, int bit );
int tstbit( int word, int bit );
int xorbit( int word, int bit );
int setbit( int word, int bit );
void bildschirmausgabe(WINDOW *fenster);
void reset_bildschirm(WINDOW *fenster);
void keine_neuen_daten(WINDOW *fenster);
void set_attribut(int zaehler, WINDOW *fenster);
void test_farbe(void);
int lies_conf(void);
int get_modulmodus(void);

FILE *fp_logfile=NULL, *fp_varlogfile=NULL, *fp_csvfile=NULL, *fp_csvfile_2=NULL;
struct termios oldtio; /* will be used to save old port settings */
int fd;
int write_erg, bool_farbe; /* filediskriptor, anz_datensaetze, Farbe moeglich */
struct sockaddr_in SERVER_sockaddr_in; /* fuer ip-Zurgriff */

/* Sensoren */
int SENS_Art[17];/* Das erste Element [0] wird nicht beruecksichtigt, Art es EingangParameter*/
float SENS[17];
char  *pBez_S[17];
/* Ausgaenge */
int AUSG[14], DZR[8], DZStufe[8];   /* beruecksichtigt! S[1] ist Sensor1  */
char *pBez_A[14], *pBez_DZR[8], *pBez_DZS[8];
/* Waermezaehler */
float  Mlstg[3],W_kwh[3], W_Mwh[3];
int WMReg[3]; /* Das erste Element beginnt bei [1] */

char LogFileName[12], varLogFile[22];
char sDatum[11], sZeit[11];

//UCHAR akt_daten[58];
UCHAR akt_daten[116];
bool ext_bezeichnung;

/* Uebergebener Paramter USB-Port, repeat-time */
char  dlport[13];  /* -p */

struct tm *merk_zeit;
time_t dauer;  /* -t */
int csv_output;
int rrd_output;
int list_output;
int ip_zugriff;
int usb_zugriff;
int sock;
int c;
int merk_monat;
UCHAR uvr_modus, uvr_typ=0, uvr_typ2=0;  /* uvr_typ2 -> 2. Geraet bei 2DL */
UCHAR datenrahmen;

char Version[]="Version 0.9.5 vom 27.02.2013";

WINDOW *fenster1=NULL, *fenster2=NULL;
PANEL  *panel1=NULL, *panel2=NULL;

int main(int argc, char *argv[])
{
  fp_logfile=NULL; fp_varlogfile=NULL; fp_csvfile=NULL; fp_csvfile_2=NULL;
  csv_output=0;rrd_output=0;list_output=0;
  int in_bytes=0;

  UCHAR uvr_modus_tmp, sendbuf[1], sendbuf_can[2];    /*  sendebuffer fuer die Request-Commandos*/
  unsigned char empfbuf[256];
  int send_bytes = 0, erg_check_arg ,result, ip_first;
  int pruefz_ok = 0, t_count, anzahl_can_rahmen = 0;
  int i, j=2, sr=0;

  time_t beginn, jetzt, diff_zeit;

  datenrahmen=0x01;
  result=0;
  dauer=-1; /* Vorbelegung Anzahl sek zum erneuten Lesen der Daten */
  ip_zugriff = 0;
  usb_zugriff = 0;
  ip_first = 1;
  erg_check_arg = check_arg_getopt(argc, argv);

  if( erg_check_arg != 1 )
    exit(-1);

  if(dauer==-1)
    {
      dauer = 30;
      if ( (!rrd_output) && (!list_output) )
        fprintf(stderr," kein update-Zeitintervall angegeben - auf %d gesetzt\n",(int)dauer);
    }

  /*************************************************************************/
  /* aktiviert die locale Umgebung Komma oder Punkt als Dezimaltrenner :   */
  if ( (!rrd_output) && (!list_output) )
    setlocale(LC_ALL, "");

  if ( (dauer != 0) && (!rrd_output) && (!list_output))
  {
    if ( lies_conf() == 1)
      ext_bezeichnung = TRUE;
    else
    {
      ext_bezeichnung=FALSE;
      bool_farbe=FALSE;
    }
  }
  else
  {
    ext_bezeichnung=FALSE;
    bool_farbe=FALSE;
  }

  if ((!csv_output) && (dauer != 0) && (!rrd_output)  && (!list_output))
  {
    initscr();                                /* initialisieren von ncurses */
    if (bool_farbe)
      test_farbe();                        /* Koennen Farben aktiviert werden */
    clear();
    refresh();
    fenster1= newwin(25,80,0,0);
    fenster2= dupwin(fenster1);
    panel1=new_panel(fenster1);
    panel2=new_panel(fenster2);
  }

  t_count=1;
  diff_zeit=-1;
  c=0;

  /************************************************************************/
  /* output files fuer csv/rrd  */
  /* LogFile zum Speichern von Ausgaben aus diesem Programm hier */
#if 0
  pvarLogFile = varLogFile;
  sprintf(pvarLogFile,"/var/log/dl-lesenx.log");
#endif

  /* IP-Zugriff  - IP-Adresse und Port sind manuell gesetzt!!! */
  if (ip_zugriff && !usb_zugriff)
  {
	sr = start_socket(fenster1, fenster2);
	if (sr > 1)
	{
		return sr;
	}
  } /* Ende IP-Zugriff */
  else  if (usb_zugriff && !ip_zugriff)
  {
	init_usb();	  
  }
  else
  {
    fprintf(stderr, " da lief was falsch ....\n %s \n", argv[0]);
    do_cleanup(fenster1, fenster2);
  }

  uvr_modus = get_modulmodus(); /* Welcher Modus 
                                0xA8 (1DL) / 0xD1 (2DL) / 0xDC (CAN) */
  fprintf(stderr, " CAN-Logging: uvr_modus -> %2X \n", uvr_modus);								

  if ( uvr_modus == 0xDC )
  {
	sendbuf[0]=KONFIGCAN;
    write_erg=send(sock,sendbuf,1,0);
    if (write_erg == 1)    /* Lesen der Antwort */
      result  = recv(sock,empfbuf,18,0);

	anzahl_can_rahmen = empfbuf[0];	
    fprintf(stderr, " CAN-Logging: anzahl_can_rahmen -> %d \n", anzahl_can_rahmen);	
  }
  close_usb();
  
  /********************************************************************/
  /* aktuelle Daten lesen                                             */
  /********************************************************************/
  int kennung_ok = 1;
  time_t  daten_zeitpunkt;

  do
  {
    kennung_ok = 1;
    sendbuf_can[0]=AKTUELLEDATENLESEN;
    sendbuf_can[1]=datenrahmen;      /* 1. Datenrahmen vorbelegt */
    sendbuf[0]=AKTUELLEDATENLESEN;   /* Senden Request aktuelle Daten */
//    bzero(akt_daten,58); /* auf 116 Byte fuer 2DL erweitert */
    //bzero(akt_daten,116);
    memset( akt_daten, 0, 116 );
    /* select um rauszufinden ob ready to read !  siehe man select */
    fd_set rfds;
    struct timeval tv;
    int retval=0;
    int retry=0;
    int init_retry=0;
    int send_retry=0;
    int retry_interval=10; /* wie Winsol Display aktuelle Daten */

    if (usb_zugriff)
    {
	  init_usb();
	  uvr_modus = get_modulmodus();
	  if ( uvr_modus == 0xDC )
	  {
		fprintf(stderr,"USB-Abfrage bei CAN-Logging nicht implementiert!\n"); 
		return(1);
	  }
      retry_interval=5;
      write_erg=write(fd,sendbuf,1);
      if (write_erg == 1)    /* Lesen der Antwort*/
      {
        retry=0;
        do
        {
	      in_bytes=0;		
          FD_ZERO(&rfds);  /* muss jedes Mal gesetzt werden */
          FD_SET(fd, &rfds);
          tv.tv_sec = retry_interval; /* Wait up to five seconds. */
          tv.tv_usec = 0;
          retval = select(fd+1, &rfds, NULL, NULL, &tv);
          /* Don't rely on the value of tv now -  will contain remaining time or so */

          zeitstempel();

          if (retval == -1)
            perror("select(fd)");
          else if (retval)
          {
#ifdef DEBUG
            fprintf(stderr,"Data is available now. %d.%d\n",(int)tv.tv_sec,(int)tv.tv_usec);
#endif
            //result=read(fd,akt_daten,57); /* Lesen von 115 Byte ohne Fehler??? fuer 2DL */
	      if (FD_ISSET(fd,&rfds))
           {
		     ioctl(fd, FIONREAD, &in_bytes);
#ifdef DEBUG
		     fprintf(stderr,"Bytes im Puffer: %d\n",in_bytes);
#endif
		     if (uvr_modus == 0xA8)
		     {
			  switch(in_bytes)
			  {
				case 28: result=read(fd,akt_daten,115);
						ioctl(fd, FIONREAD, &in_bytes); 
			               break;
				case 57: result=read(fd,akt_daten,115);
		                                ioctl(fd, FIONREAD, &in_bytes);
			               break;
			  }
		     }
		     if (uvr_modus == 0xD1)
		     {
			   switch(in_bytes)
			   {
				case 55: result=read(fd,akt_daten,115);
					        ioctl(fd, FIONREAD, &in_bytes); 
			               break;
				case 113: result=read(fd,akt_daten,115);
		                                  ioctl(fd, FIONREAD, &in_bytes); 
			               break;
			   }
		     }
		     
	       }
#ifdef DEBUG
        if (akt_daten[0] != 0xAB)
        {
          fprintf(stderr,"\nDie ersten 27 Byte Rohdaten vom D-LOGG USB:\n");
          for (i=0;i<27;i++)
            fprintf(stderr,"0x%2x; ", akt_daten[i]);
          fprintf(stderr,"\n");
        }
#endif
#ifdef DEBUG
            fprintf(stderr,"r%d s%d received bytes=%d \n",retry,send_retry,result);
            if (result == 1)
              fprintf(stderr," buffer: %X\n",akt_daten[0]);
              /* FD_ISSET(socket, &rfds) will be true. */
#endif
          }
          else
          {
#ifdef DEBUG
            fprintf(stderr,"%s - No data within %d seconds.r%d s%d\n",sZeit,retry_interval,
            retry,send_retry);
#endif
            sleep(retry_interval);
	    retry++;
          }
          
        }
        while( retry < 3 && in_bytes != 0);
      }
#ifdef DEBUG
      else
        fprintf(stderr," send_bytes=%d - not ok! \n",send_bytes);
#endif
#ifdef DEBUG      
      fprintf(stderr,"Nach while... \n");
#endif
      /* if (result>0 && result <=57)  */
      if (result>27 && result <=115)
        akt_daten[result]='\0'; /* mit /000 abschliessen!! */
      else if (retry == 3)
      {
	    fprintf(stderr,"Keine Daten von %s erhalten. \n",dlport);
	    do_cleanup(fenster1, fenster2);
        exit(-1);
      }
	  close_usb();
    } /* Ende if (usb_zugriff) */

    if (ip_zugriff)
    {
      if (!ip_first)
      {
	  fprintf(stderr, " CAN-Logging: IP initialisieren.\n");
        sock = socket(PF_INET, SOCK_STREAM, 0);
        if (sock == -1)
        {
          perror("socket failed()");
          do_cleanup(fenster1, fenster2);
          return 2;
        }
        if (connect(sock, (const struct sockaddr *)&SERVER_sockaddr_in, sizeof(SERVER_sockaddr_in)) == -1)
        {
          perror("connect failed()");
          do_cleanup(fenster1, fenster2);
          return 3;
        }
		if (ip_handling(sock) == -1)
		{
			fprintf(stderr, "%s: Fehler im Initialisieren der IP-Kommunikation\n", argv[0]);
			do_cleanup(fenster1, fenster2);
			return 4;
		}
      }
      do
      {
        do
        {
		  uvr_modus = get_modulmodus();
		  if ( uvr_modus == 0xDC )
		  {
			sendbuf[0]=KONFIGCAN;
			write_erg=send(sock,sendbuf,1,0);
			if (write_erg == 1)    /* Lesen der Antwort */
				result  = recv(sock,empfbuf,18,0);
			uvr_modus_tmp = get_modulmodus();
			if ( anzahl_can_rahmen < datenrahmen )
			{
				fprintf(stderr,"Falsche Parameterangabe: -r %d , setze auf 1. Datenrahmen!\n",datenrahmen);
				datenrahmen = 1;
				sendbuf_can[1] = datenrahmen;
			}
			send_bytes=send(sock,sendbuf_can,2,0);
		  }
		  else
			send_bytes=send(sock,sendbuf,1,0);
		  
          if ( (send_bytes == 1 && uvr_modus != 0xDC) || (send_bytes == 2 && uvr_modus == 0xDC) )    /* Lesen der Antwort */
          {
			  if ( uvr_modus == 0xDC )
			  {
			    i = 1;
				do
				{
					result  = recv(sock,akt_daten,115,0);
					fprintf(stderr, " CAN-Logging: Response Kennung -> %02X  Wartezeit -> %02d Sec  Byte3: %02X \n", akt_daten[0], akt_daten[1], akt_daten[2]);
					if ( akt_daten[0] == 0xBA )
					{
						if ( akt_daten[2] == (akt_daten[0] + akt_daten[1]) % 0x100 )
						{
							zeitstempel();
							fprintf(stderr, "%s CAN-Logging: %d. Schlafenszeit fuer %d Sekunden\n",sZeit ,i , akt_daten[1]);
							if ( shutdown(sock,SHUT_RDWR) == -1 ) /* IP-Socket schliessen */
							{
								zeitstempel();
								fprintf(stderr, "\n %s Fehler beim Schliessen der IP-Verbindung!\n", sZeit);
							}
							sleep(akt_daten[1]);
							sr = start_socket(fenster1, fenster2);
							if (sr > 1)
							{
								return sr;
							}
							i++;
							sleep(akt_daten[1]);
							uvr_modus_tmp = get_modulmodus();
							if ( uvr_modus_tmp == 0xDC )
								send_bytes=send(sock,sendbuf_can,2,0);
						}
					}
					if ( i > 3 )
					{
						fprintf(stderr, " CAN-Logging: keine Daten empfangen.\n");
						return -1;
					}
				}
				while( akt_daten[0] == 0xBA && i < 4 );
			  }
			  else
			  {
				do
				{
					/* muss jedesmal gesetzt werden! */
					FD_ZERO(&rfds);
					FD_SET(sock, &rfds);
					/* Wait up to five seconds. */
					tv.tv_sec = retry_interval;
					tv.tv_usec = 0;
					retval = select(sock+1, &rfds, NULL, NULL, &tv);
					/* Don't rely on the value of tv now -  will contain remaining time or so */
					zeitstempel();

					if (retval == -1)
						perror("select(sock)");
					else if (retval)
						{
#ifdef DEBUG
						fprintf(stderr,"Data is available now. %d.%d\n",(int)tv.tv_sec,(int)tv.tv_usec);
#endif
						result  = recv(sock,akt_daten,115,0);

#ifdef DEBUG
						fprintf(stderr,"r%d s%d received bytes=%d \n",retry,send_retry,result);
						if (result == 1)
							fprintf(stderr," buffer: %X\n",akt_daten[0]);
#endif
                /* FD_ISSET(socket, &rfds) will be true. */
						}
					else
					{
#ifdef DEBUG
						fprintf(stderr,"%s - No data within %d seconds.r%d s%d\n",sZeit,retry_interval,retry,send_retry);
#endif
						sleep(retry_interval);
					}
				retry++;
				}
				while( retry < 3 && result < 28);
			  }

            retry=0;
          }
#ifdef DEBUG
        else
          fprintf(stderr," send_bytes=%d - not ok! \n",send_bytes);
#endif
        send_retry++;
        }
        while ( send_retry <= 2 && result < 28 );
        send_retry=0;
        init_retry++;
      }
      while ( init_retry <= 2 && result < 28 );

      close(sock); /* IP-Socket schliessen */
      ip_first = 0;

//      if (result>0 && result <=57)
        if (result>0 && result <=115)
          akt_daten[result]='\0'; /* mit /000 abschliessen!! */
    } /* end IP */

    check_kennung(result);

    daten_zeitpunkt=time(0);
    beginn=daten_zeitpunkt;
    zeitstempel();
#ifdef DEBUG
    fprintf(stderr,"aktuelle Kennung: %X\n",akt_daten[0]);
#endif

    /********************************************************************************************/
    /* Es sind KEINE aktuelle Daten vorhanden                                                         */
    /********************************************************************************************/
    if (akt_daten[0] == 0xAB)  // keine neuen Daten
    {
      if((!csv_output) && (dauer != 0))
        curs_set(0);

        kennung_ok = 0;
        if((!csv_output) && (dauer != 0))
        {
          if (bool_farbe)
            attrset(A_BOLD | COLOR_PAIR(1));
          else
            attrset(A_BOLD);

          mvprintw(23,1,"Es liegen keine aktuellen Daten vor.                         ");
          attrset(A_NORMAL);
        }
        else
          fprintf(stderr,"%s -  keine aktuellen Daten1 - %d  \n",sZeit,(int)beginn);

      do
      {
        if((!csv_output) && (dauer != 0))
        {
          halfdelay(10);
          c=getch();
        }
        if (dauer > 0)
        {
          jetzt=time(0);
          diff_zeit = difftime(jetzt, beginn);
#ifdef DEBUG
          zeitstempel();
#endif
          if((!csv_output) && (dauer != 0))
            mvprintw(23,79-(strlen(sZeit)),"%s",sZeit);
          else
          {
            diff_zeit+= retry_interval;
            sleep(retry_interval);
#if DEBUG>2
            if( diff_zeit%10==0)
              fprintf(stderr," %s - keine aktuellen Daten, diffzeit: %d \n",sZeit,(int)diff_zeit);
#endif
          }
          /* schneller nochmal probieren */
          if (csv_output)
          {
            float  part_time = 5.5 * (float)diff_zeit;
            diff_zeit = (time_t)part_time;
          }
        }
        else
        {
          sleep(retry_interval);
          c = 'q';
        }
      }
      while( (c != 'q') && (diff_zeit < dauer ) ); /* nach Anzahl "dauer" Sekunden erneutes Lesen */
    } /* if (akt_daten[0] == 0xAB)  // keine neuen Daten */

    /********************************************************************************************/
    /* Es sind aktuelle Daten vorhanden                                                         */
    /********************************************************************************************/
    else if (akt_daten[0] == UVR1611 || akt_daten[0] == UVR61_3) /*  Daten sind vorhanden (akt_daten[0] != 0xAB) */
    {
      pruefz_ok=check_pruefsumme(); /* Pruefsumme ok? */

      if (pruefz_ok==1) /* Pruefziffer ok */
      {
        beginn=time(0);
        merk_zeit = localtime(&beginn);
        if ((!csv_output) && (dauer != 0))
        {
          move(23,1);
          deleteln();
        }

#ifdef DEBUG
        if (akt_daten[0] == UVR61_3)
        {
          fprintf(stderr,"\nRohdaten der UVR61-3:\n");
          for (i=0;i<27;i++)
            printf("%2x; ", akt_daten[i]);
        }
#endif
        /* Berechnung der Werte zum Anzeigen bzw. Schreiben in eine Datei */
        berechne_werte(1);

        if (dauer == 0)
        {
          write_CSVCONSOLE(1, daten_zeitpunkt );
          if (uvr_modus == 0xD1)  /* zwei Regler vorhanden */
          {
            berechne_werte(2);
            write_CSVCONSOLE(2, daten_zeitpunkt );
          }
          c = 'q';
        }
        if (rrd_output)
        {
          write_rrd(1);
          if (uvr_modus == 0xD1)  /* zwei Regler vorhanden */
          {
            berechne_werte(2);
            write_rrd(2);
          }
          dauer = 0;
          c = 'q';
        }
        if (list_output)
        {
          write_list(1);
          if (uvr_modus == 0xD1)  /* zwei Regler vorhanden */
          {
            berechne_werte(2);
            write_list(2);
          }
          dauer = 0;
          c = 'q';
        }
        if (csv_output)  /* in Datei schreiben */
        {
          func_csv_output(1, daten_zeitpunkt, fenster1, fenster2);
          if (uvr_modus == 0xD1)  /* zwei Regler vorhanden */
          {
            berechne_werte(2);
            func_csv_output(2, daten_zeitpunkt, fenster1, fenster2);
          }
        } /* Ende if (csv_output)  */
        else if(dauer !=0)
        {
          bildschirmausgabe(fenster1);
          zeitstempel();
          mvwprintw(fenster1,23,79-(strlen(sZeit)),"%s",sZeit);
          mvwprintw(fenster1,22,79-(23+(strlen(sZeit))),"letzte Aktualisierung: %s",sZeit);
          if (uvr_modus == 0xD1)  /* zwei Regler vorhanden */
            mvwprintw(fenster1,21,36,"Geraet 1");
          mvwprintw(fenster1,21,76,"%i",t_count);
          wattrset(fenster1,A_NORMAL);
          clearok(fenster1, TRUE);
          wrefresh(fenster1);
          top_panel(panel1);
          update_panels();
          doupdate();
          if (uvr_modus == 0xD1)  /* zwei Regler vorhanden */
          {
            berechne_werte(2);
            bildschirmausgabe(fenster2);
            zeitstempel();
            mvwprintw(fenster2,23,79-(strlen(sZeit)),"%s",sZeit);
            mvwprintw(fenster2,22,79-(23+(strlen(sZeit))),"letzte Aktualisierung: %s",sZeit);
            mvwprintw(fenster2,21,36,"Geraet 2");
            mvwprintw(fenster2,21,76,"%i",t_count);
            wattrset(fenster2,A_NORMAL);
            clearok(fenster2, TRUE);
            wrefresh(fenster2);
            top_panel(panel1);
            update_panels();
            doupdate();
            j=2;
          }
        }
        t_count++;
      }
      for (i=0;i<115;i++) /* alles wieder auf 0 setzen */
        akt_daten[i]=0x0;
	  if (dauer == 0)
	    break;
    } /* else if (akt_daten[0] == UVR1611 || akt_daten[0] == UVR61_3) */
    else
    { /* some other problem - retry immediately */
      kennung_ok=0;
    }
    do
    {
      if ((!csv_output) && (dauer != 0))
      {
        halfdelay(10);
        c=getch();
      }
      if (dauer >= 0)
      {
        jetzt=time(0);
        diff_zeit=difftime(jetzt, beginn);
        if (dauer >= 0)
        {
          zeitstempel();
          if ((!csv_output) && (dauer != 0))
            mvprintw(23,79-(strlen(sZeit)),"%s",sZeit);
          else
          {
            diff_zeit+=retry_interval;
            sleep(retry_interval);
#ifdef DEBUG
            if( diff_zeit % 10 == 0 )
            {
              if(kennung_ok && !pruefz_ok)
                fprintf(stderr,"%s - pruefziffer   diff_zeit:%d  dauer:%d \n",sZeit,(int)diff_zeit,(int)dauer);
              else if(!kennung_ok)
                fprintf(stderr,"%s - K%d PZ%d  KENN%X    diff_zeit:%d  dauer:%d \n",sZeit,kennung_ok,pruefz_ok,akt_daten[0],(int)diff_zeit,(int)dauer);
#if DEBUG>2
              else
                fprintf(stderr,"%s - K%d PZ%d  KENN%X    diff_zeit:%d  dauer:%d \n",sZeit,kennung_ok,pruefz_ok,akt_daten[0],(int)diff_zeit,(int)dauer);
#endif
            }
#endif
          }
        }

        if ( !pruefz_ok || !kennung_ok   )      // Pruefziffer stimmt nicht ueberein
        {/* schneller nochmal probieren */
          float  part_time = 10 * (float)diff_zeit;
          diff_zeit = (time_t)part_time;
        }
      }
      if ( c == 'w')
      {
        if (j == 1) /* erstes Fenster (Geraet) */
        {
//          mvwprintw(fenster1,21,16,"Taste w: j=1");
          clear();
          hide_panel(panel2);
          mvwprintw(fenster1,21,36,"Geraet 1");
          wrefresh(fenster1);
          show_panel(panel1);
          top_panel(panel1);
          update_panels();
          doupdate();
          j = 2;
        }
        else if (uvr_modus == 0xD1)
        {
//          mvwprintw(fenster2,21,16,"Taste w: j=2");
          clear();
          hide_panel(panel1);
          mvwprintw(fenster2,21,36,"Geraet 2");
          wrefresh(fenster2);
          show_panel(panel2);
          top_panel(panel2);
          update_panels();
          doupdate();
          j = 1;
        }
      }
    }
    while( (c != 'q') && (diff_zeit < dauer ) );     /* ende Daten vorhanden */
  }       /* reader loop */
  while(c != 'q');

  if((!csv_output) && (dauer != 0))
  {
    reset_bildschirm(fenster1);
    reset_bildschirm(fenster2);
  }

  /*************************************************************************/
  /* close all open files  */
  /* restore the old port settings before quitting */
  if(!do_cleanup(fenster1, fenster2))
    return (-1);

  return(0);
} /* Ende main() */


/* Init USB-Zugriff */
void init_usb(void)
{
  struct termios newtio;  /* will be used for new port settings */

    /************************************************************************/
    /*  open the serial port for reading and writing */
    fd = open(dlport, O_RDWR | O_NOCTTY); // | O_NDELAY);
    if (fd < 0)
    {
      perror(dlport);
      do_cleanup(fenster1, fenster2);
      exit(-1);
    }
    /* save current port settings */
    tcgetattr(fd,&oldtio);
    /* initialize the port settings structure to all zeros */
    //bzero(&newtio, sizeof(newtio));
    memset( &newtio, 0, sizeof(newtio) );
    /* then set the baud rate, handshaking and a few other settings */
    cfsetspeed(&newtio, BAUDRATE);
    newtio.c_cflag &= ~PARENB;
    newtio.c_cflag &= ~CSTOPB;
    newtio.c_cflag &= ~CSIZE;
    newtio.c_cflag |= CS8;
    newtio.c_cflag |= (CLOCAL | CREAD);
    newtio.c_cflag &= ~CRTSCTS;
    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;
    newtio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    newtio.c_oflag &= ~OPOST;          /* setze "raw" Input */
    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 1;   /* blocking read until first char received */
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd,TCSANOW,&newtio);
}

/* socket erzeugen und Verbindung aufbauen */
int start_socket(WINDOW *fenster1, WINDOW *fenster2)
{
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
      perror("socket failed()");
      do_cleanup(fenster1, fenster2);
      return 2;
    }

    if (connect(sock, (const struct sockaddr *)&SERVER_sockaddr_in, sizeof(SERVER_sockaddr_in)) == -1)
    {
      perror("connect failed()");
      do_cleanup(fenster1, fenster2);
      return 3;
    }

    if (ip_handling(sock) == -1)
    {
      fprintf(stderr, "Fehler im Initialisieren der IP-Kommunikation!\n");
      do_cleanup(fenster1, fenster2);
      return 4;
    }
	
	return 1;
}

/* USB-Port schliessen */
void close_usb(void)
{
  if ( fd > 0 )
    {
      // reset and close
      tcflush(fd, TCIFLUSH);
      tcsetattr(fd,TCSANOW,&oldtio);
    }
}

/* Aufraeumen und alles Schliessen */
int do_cleanup(WINDOW *fenster1, WINDOW *fenster2)
{
  if (fp_csvfile != NULL)
    fclose(fp_csvfile);
  if (fp_csvfile_2 != NULL)
    fclose(fp_csvfile_2);
  if (fp_logfile!=NULL)
    fclose(fp_logfile);
  if(fp_varlogfile!=NULL)
    fclose(fp_varlogfile);

  close_usb();

  if((!csv_output) && (dauer != 0))
  {
    reset_bildschirm(fenster1);
    reset_bildschirm(fenster2);
    endwin();
  }

  if (ip_zugriff)
    close(sock); /* IP-Socket schliessen */

  return 1;
}

/* Statische Bildschirmausgabe / Hilfetext */
static int print_usage()
{
  fprintf(stderr,"\n    UVR1611 / UVR61-3 aktuelle Daten lesen vom D-LOGG USB oder BL-NET\n");
  fprintf(stderr,"    %s \n",Version);
  fprintf(stderr,"\ndl-aktuelle-datenx (-p USB-Port | -i IP:Port) [-t sek] [-r DR] [-h] [-v] [--csv] [--rrd] [--list] \n");
  fprintf(stderr,"    -p USB-Port -> Angabe des USB-Portes,\n");
  fprintf(stderr,"                   an dem der D-LOGG angeschlossen ist.\n");
  fprintf(stderr,"    -i IP:Port  -> Angabe der IP-Adresse und des Ports,\n");
  fprintf(stderr,"                   an dem der BL-Net angeschlossen ist.\n");
  fprintf(stderr,"        -t sek  -> Angabe der Zeit in Sekunden (min. 30 max. 3600),\n");
  fprintf(stderr,"                   nach denen die Daten erneut gelesen werden sollen\n");
  fprintf(stderr,"                   Default: 30 Sek. (ohne Paramter -t) \n\n");
  fprintf(stderr,"                   Sonderfall 0 Sekunden (-t 0): es wird nur ein Datensatz\n");
  fprintf(stderr,"                   gelesen, das Programm beendet und die Daten im csv-Format\n");
  fprintf(stderr,"                   am Bildschirm ausgegeben.\n\n");
  fprintf(stderr,"        -r DR   -> Angabe des auszulesenden DatenRahmens (1 - 8, Default: 1)\n");
  fprintf(stderr,"                   (Nur zutreffend bei CAN-Logging.)\n\n");
  fprintf(stderr,"          --csv -> im CSV-Format speichern\n");
  fprintf(stderr,"          --rrd -> output eines RRD tool strings\n");
  fprintf(stderr,"         --list -> output einer Liste\n");
  fprintf(stderr,"          -h    -> diesen Hilfetext\n");
  fprintf(stderr,"          -v    -> Versionsangabe\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"Beispiel: dl-aktuelle-datenx -p /dev/ttyUSB0 -t 30 \n");
  fprintf(stderr,"          Liest alle 30s die aktuellen Daten vom USB-Port 0 .\n");
  fprintf(stderr,"          dl-aktuelle-datenx -i 192.168.1.1:40000 -t 30 \n");
  fprintf(stderr,"          Liest alle 30s die aktuellen Daten von IP 192.168.1.1 Port 40000 .\n");
  fprintf(stderr,"          dl-aktuelle-datenx -i 192.168.1.1:40000 -t 0 -r 2\n");
  fprintf(stderr,"          Liest einmal die aktuellen Daten von IP 192.168.1.1 Port 40000,\n");
  fprintf(stderr,"          2. Datenrahmen.\n");
  fprintf(stderr,"\n");
  return 0;
}

/* Ueberpreufung der Argumente */
int check_arg_getopt(int arg_c, char *arg_v[])
{
  int j=0;
  int c = 0;
  int p_is_set=-1;
  int i_is_set=-1;
  char trennzeichen[] = ":";

  /* arbeitet alle argumente ab  */
  while (1)
  {
    int option_index = 0;
    static struct option long_options[] =
    {
      {"csv", 0, 0, 0},
      {"rrd", 0, 0, 0},
      {"list", 0, 0, 0},
      {0, 0, 0, 0}
    };

    c = getopt_long(arg_c, arg_v, "hvi:p:t:r:", long_options, &option_index);

    if (c == -1)  /* ende des Parameter-processing */
    {
      if (optind <2)
      {
        print_usage(); /* -p is a non-optional argument */
        return -1;
      }
      break;
    }

    switch(c)
    {
      case 'v':
      {
        fprintf(stderr,"\n    UVR1611 / UVR61-3 aktuelle Daten lesen vom D-LOGG USB oder BL-NET\n");
        fprintf(stderr,"    %s \n",Version);
		printf("    $Id$ \n");
        printf("\n");
        return -1;
        }
      case 'h':
      case '?':
        print_usage();
        return -1;
      case 'p':
      {
        if ( optarg && strlen(optarg) < 14 && strlen(optarg) > 3 )
        {
          // sollte /dev/ttyUSBnn sein
          strncpy(dlport, optarg , strlen(optarg));
          fprintf(stderr,"\n port gesetzt: %s\n", dlport);
          p_is_set=1;
          usb_zugriff = 1; /* 05.02. neu */
        }
        else
        {
          fprintf(stderr," portname falsch: %s\n",optarg);
          print_usage();
          return -1;
        }
        break;
      }
      case 'i': /* 05.02. neu */
      {
        struct hostent* hostinfo = gethostbyname(strtok(optarg,trennzeichen));
        if(0 == hostinfo)
        {
          fprintf(stderr," IP-Adresse konnte nicht aufgeloest werden: %s\n",optarg);
          print_usage();
          return -1;
        } 
        else 
        {
          SERVER_sockaddr_in.sin_addr = *(struct in_addr*)*hostinfo->h_addr_list;
          // SERVER_sockaddr_in.sin_port = htons((unsigned short int) atol(strtok(NULL,trennzeichen)));
			char* port_par =  strtok(NULL,trennzeichen);
			if ( port_par == NULL )
				port_par = "40000";
			SERVER_sockaddr_in.sin_port = htons((unsigned short int) atol(port_par));
          SERVER_sockaddr_in.sin_family = AF_INET;
          fprintf(stderr,"\n Adresse:port gesetzt: %s:%d\n", inet_ntoa(SERVER_sockaddr_in.sin_addr),
          ntohs(SERVER_sockaddr_in.sin_port));
          i_is_set=1;
          ip_zugriff = 1;
		}
        break;
      }
      case 't':
      {
        for(j=0;j<strlen(optarg);j++)
        {
          if(isalpha(optarg[j]))
          {
            fprintf(stderr," Falsche Parameterangabe bei -t: %s\n", optarg);
            print_usage();
            return -1;
          }
        }
        dauer=atoi(optarg);
        if (((dauer < 30) && (dauer != 0)) || (dauer > 3600))
        {
          fprintf(stderr,"Falsche Parameterangabe:  -t %d \n",(int)dauer);
          print_usage();
          return -1;
        }
        break;
      }
      case 0:
      {
        if (  strncmp( long_options[option_index].name, "csv", 3) == 0 ) /* csv-Format ist gewuenscht */
        {
          csv_output = 1;
          fprintf(stderr," csv output aktiviert!\n");
        }
        if (  strncmp( long_options[option_index].name, "rrd", 3) == 0 )
        {
          rrd_output = 1;  /* RRD */
        }
        if (  strncmp( long_options[option_index].name, "list", 4) == 0 )
        {
          list_output = 1;  /* Liste */
        }
        break;
      case 'r':
      {
        for(j=0;j<strlen(optarg);j++)
        {
          if(isalpha(optarg[j]))
          {
            fprintf(stderr," Falsche Parameterangabe bei -t: %s\n", optarg);
            print_usage();
            return -1;
          }
        }
        datenrahmen=atoi(optarg);
        if ( (datenrahmen < 1)  || (datenrahmen > 8) )
        {
          fprintf(stderr,"Falsche Parameterangabe:  -r %d \n",(int)datenrahmen);
          print_usage();
          return -1;
        }
        break;
      }
      }
      default:
        fprintf(stderr,"?? input mit character code 0%o ??\n", c);
        fprintf(stderr,"Falsche Parameterangabe!\n");
        print_usage();
        return( -1 );
        break;
    }
  }

  /* Restliche non-option arguments */

  if (( p_is_set < 1) && ( i_is_set < 1 ) )
  {
    fprintf(stderr," USB-Port- oder IP-Angabe fehlt!\n");
    print_usage();
    return -1;
  }
  if (( p_is_set  > 0) && ( i_is_set > 0 ) )
  {
    fprintf(stderr," Auslesen nicht gleichzeitig von USB-Port- und IP-Port moeglich!\n");
    print_usage();
    return -1;
  }

  /* falls du noch andere argumente prozessieren willst bzw. junk hinten dran.... */

  if (optind < arg_c)
  {
    fprintf(stderr,"non-option ARGV-elements: ");
    while (optind < arg_c)
      fprintf(stderr,"%s ", arg_v[optind++]);
    fprintf(stderr,"\n");
    return -1;
  }

  return 1;
}

/* Pruefsumme berechnen und kontrollieren */
int check_pruefsumme(void)
{
  int i, anzByte_uvr1 = 0, anzByte_uvr2 = 0, pruefz_ok = 1;
  UCHAR pruefz_read = 0, pruefz = 0;

  uvr_typ = akt_daten[0];
  switch(uvr_typ)
  {
    case UVR1611: anzByte_uvr1 = 55; break; /* UVR1611 */
    case UVR61_3: anzByte_uvr1 = 26; break; /* UVR61-3 */
  }

  if (uvr_modus == 0xA8)
  {
    for(i=0;i<=anzByte_uvr1;i++)
      pruefz = pruefz + akt_daten[i];
    if ((pruefz % 0x100) != akt_daten[anzByte_uvr1 + 1] )
    {
      pruefz_ok=0;
      pruefz_read = akt_daten[anzByte_uvr1 + 1];
    }
  }

  if (uvr_modus == 0xD1)  /* Modus 2 DL */
  {
    uvr_typ2 = akt_daten[anzByte_uvr1+1]; /* Welcher Typ an DL 2 ? */
    switch(uvr_typ2)
    {
      case UVR1611: anzByte_uvr2 = 55; break; /* UVR1611 */
      case UVR61_3: anzByte_uvr2 = 26; break; /* UVR61-3 */
    }

    for(i=0;i<=(anzByte_uvr1 + 1 + anzByte_uvr2);i++)
      pruefz = pruefz + akt_daten[i];

    if ((pruefz % 0x100) != akt_daten[anzByte_uvr1 + anzByte_uvr2 + 2] )
    {
      pruefz_ok=0;
      pruefz_read = akt_daten[56];
    }
  }
  if (pruefz_ok == 0)
    print_pruefsumme_not_ok(pruefz,pruefz_read);

  return pruefz_ok;
}

/* Ausgabe des Fehlers "Pruefsumme falsch" */
void print_pruefsumme_not_ok(UCHAR pruefz_berech, UCHAR pruefz_read)
{
  if ((!csv_output) && (dauer != 0))
  {
    curs_set(0);
    mvprintw (22, 1, "Beenden -> Taste >q< druecken");
    if (bool_farbe)
      attrset(A_BOLD | COLOR_PAIR(1));
    else
      attrset(A_BOLD);
    mvprintw(23,1,"Pruefziffern stimmen nicht ueberein! berechnet: %x gelesen: %x",pruefz_berech,pruefz_read);
    attrset(A_NORMAL);
  }
  else
    fprintf(stderr,"%s -  Pruefziffern stimmen nicht ueberein! berechnet: %x gelesen: %x\n",sZeit,pruefz_berech,pruefz_read);
  zeitstempel();
  if ((!csv_output) && (dauer != 0))
  {
    mvprintw(23,79-(strlen(sZeit)),"%s",sZeit);
    halfdelay(30);
    c=getch();
  }
}

/* Der Name des Logfiles wird aus dem eigelesenen Wert fuer Jahr und Monat erzeugt */
int erzeugeLogfileName(int for_csv, char * pLogFileName, int anz_regler)
{
  /*    Monat und Jahr aus dem ersten gelesenen Datensatz ermittelt */
  int erg = 0;
  char csv_endung[] = ".csv", winsol_endung[] = ".log";

  char  monat[256],jahr[256];
  struct tm *zeit;
  time_t sekunde;

  time(&sekunde);
  zeit = localtime(&sekunde);

  strftime(monat,10, "%m", zeit);
  strftime(jahr, 10, "%y", zeit);

  if ( for_csv ==  1)
  {/* LogDatei im CSV-Format schreiben */
    if (anz_regler == 1)
      erg=sprintf(pLogFileName,"E%s%s%s",jahr,monat,csv_endung);
    else if (anz_regler == 2)
      erg=sprintf(pLogFileName,"E%s%s_2%s",jahr,monat,csv_endung);
  }
  else  /* LogDatei im Winsol-Format schreiben */
  {
    erg=sprintf(pLogFileName,"Y20%s%s%s",jahr,monat,winsol_endung);
  }

  fprintf(stderr," logfilename: %s\n",pLogFileName);

  return erg;
}

/* Logdatei oeffnen / erstellen */
int open_logfile(char LogFile[], FILE **fp)
{
  if ( ( *fp = fopen(LogFile,"r")) == NULL) /* wenn Logfile noch nicht existiert */
  {
    if (( *fp =fopen(LogFile,"w")) == NULL) /* dann Neuerstellung der Logdatei */
    {
      fprintf(stderr,"Log-Datei %s konnte nicht erstellt werden\n",LogFile);
      *fp = NULL;
      return (-1);
    }
    return (1);
  }
  else /* das Logfile existiert schon */
  {
    fclose(*fp);
    if ((*fp=fopen(LogFile,"a")) == NULL) /* schreiben ab Dateiende */
    {
      fprintf(stderr,"Log-Datei %s konnte nicht geoeffnet werden\n",LogFile);
      *fp=NULL;
      return (-1);
    }
  }

  return(0);
}

/* Logdatei schliessen */
int close_logfile(FILE * fp)
{
  int i = -1;
  if ( fp != NULL )
    i=fclose(fp_logfile);

  return(i);
}

/* Check auf Uebereinstimmung von Kennung und Anzahl gelesener Bytes */
void check_kennung(int received_Bytes)
{
#ifdef DEBUG
  fprintf(stderr," akt_daten[0] %X\n",akt_daten[0]);
#endif

  if (akt_daten[0] == 0xAB)
    return;

  /* In Abhaengigkeit des Modus muss eine entsprechende Anzahl */
  /* von Bytes gelesen worden sein -> sonst fehlerhaftes Datenlesen */
  /* akt_daten[0] = 0x0; -> keine UVR(1611/61-3) -> keine Weiterverabeitung! */
  if (uvr_modus == 0xD1)
  {
    if (akt_daten[0] == UVR1611)
    {
      if (received_Bytes != 113 && received_Bytes != 84)
        akt_daten[0]=0x0;
      return;
    }
    if (akt_daten[0] == UVR61_3)
    {
      if (received_Bytes != 84 && received_Bytes != 55)
        akt_daten[0]=0x0;
      return;
    }
  }

  if (uvr_modus == 0xA8)
  {
    if (akt_daten[0] == UVR1611)
    {
      if (received_Bytes != 57)
        akt_daten[0]=0x0;
      return;
    }
    if (akt_daten[0] == UVR61_3)
    {
      if (received_Bytes != 28)
        akt_daten[0]=0x0;
      return;
    }
  }
}

/* Abfrage per IP */
int ip_handling(int sock)
{
//  unsigned char empfbuf[256];
  UCHAR empfbuf[256];	
  int send_bytes, recv_bytes;
  UCHAR sendbuf[1];
//  int sendbuf[1];

  sendbuf[0] = 0x81; /* Modusabfrage */

  send_bytes = send(sock, sendbuf,1,0);
  if (send_bytes == -1)
    return -1;

  recv_bytes = recv(sock, empfbuf,1,0);
  if (recv_bytes == -1)
    return -1;

  uvr_modus = empfbuf[0];
  //  printf("\nModus gelesen: %.2hX \n", uvr_modus);

  return 0;
}

/* Funktion zur Ausgabe der Daten in csv-Datei */
void func_csv_output(int anz_regler, time_t daten_zeitpunkt, WINDOW *fenster1, WINDOW *fenster2)
{
  char csvFileName[256];
  time_t beginn;

  if (anz_regler == 1)
  {
    if (fp_csvfile == NULL)  /* csv-Datei noch nicht geoeffnet */
    {
      erzeugeLogfileName(csv_output, &csvFileName[0],1);
      beginn = time(0);
      merk_zeit = localtime(&beginn);
      merk_monat = merk_zeit->tm_mon;
      int ret_logfile=open_logfile(&csvFileName[0], &fp_csvfile );
      if ( ret_logfile < 0 )
      {
        do_cleanup(fenster1, fenster2);
        exit(-1);
      }
      else if ( ret_logfile == 1 ) /* 1=new file 0=existing file */
      {
        if( write_header2CSV(anz_regler, fp_csvfile) < 0)
        {
          do_cleanup(fenster1, fenster2);
          exit(-1);
        }
      }
    }
    /* Hat der Monat gewechselt? Wenn ja, neues LogFile erstellen. */
    if ( merk_monat != merk_zeit->tm_mon)
    {
      printf("Monatswechsel!\n");
      fclose(fp_csvfile);
      if ( (erzeugeLogfileName(csv_output, &csvFileName[0],1)) == 0 )
      {
        printf("Fehler beim Monatswechsel: Der Logfile-Name konnte nicht erzeugt werden!\n");
        exit(-1);
      }
      else
      {
        int ret_logfile=open_logfile(&csvFileName[0], &fp_csvfile );
        if ( ret_logfile < 0 )
        {
          printf("Fehler beim Monatswechsel: Das LogFile kann nicht geoeffnet werden!\n");
          do_cleanup(fenster1, fenster2);
          exit(-1);
        }
        else if ( ret_logfile == 1 ) /* 1=new file 0=existing file */
        {
          if( write_header2CSV(anz_regler, fp_csvfile) < 0)
          {
            do_cleanup(fenster1, fenster2);
            exit(-1);
          }
        }
      }
    } /* Ende: if ( merk_monat != merk_zeit->tm_mon) */
    merk_monat = merk_zeit->tm_mon;
    write_CSVFile(anz_regler, fp_csvfile, daten_zeitpunkt );
    fprintf(stderr," %s - korrekter csv Daten output! (von %d)\n",sZeit,(int)daten_zeitpunkt);
  } /* Ende if (anz_regler == 1) */
  else if (anz_regler == 2) /* Daten des zweiten Reglers schreiben */
  {
    if (fp_csvfile_2 == NULL)  /* csv-Datei noch nicht geoeffnet */
    {
      erzeugeLogfileName(csv_output, &csvFileName[0],2);
      beginn = time(0);
      merk_zeit = localtime(&beginn);
      merk_monat = merk_zeit->tm_mon;
      int ret_logfile=open_logfile(&csvFileName[0], &fp_csvfile_2 );
      if ( ret_logfile < 0 )
      {
        do_cleanup(fenster1, fenster2);
        exit(-1);
      }
      else if ( ret_logfile == 1 ) /* 1=new file 0=existing file */
      {
        if( write_header2CSV(anz_regler, fp_csvfile_2) < 0)
        {
          do_cleanup(fenster1, fenster2);
          exit(-1);
        }
      }
    }
    /* Hat der Monat gewechselt? Wenn ja, neues LogFile erstellen. */
    if ( merk_monat != merk_zeit->tm_mon)
    {
      printf("Monatswechsel!\n");
      fclose(fp_csvfile_2);
      if ( (erzeugeLogfileName(csv_output, &csvFileName[0],2)) == 0 )
      {
        printf("Fehler beim Monatswechsel: Der Logfile-Name konnte nicht erzeugt werden!\n");
        exit(-1);
      }
      else
      {
        int ret_logfile=open_logfile(&csvFileName[0], &fp_csvfile_2 );
        if ( ret_logfile < 0 )
        {
          printf("Fehler beim Monatswechsel: Das LogFile kann nicht geoeffnet werden!\n");
          do_cleanup(fenster1, fenster2);
          exit(-1);
        }
        else if ( ret_logfile == 1 ) /* 1=new file 0=existing file */
        {
          if( write_header2CSV(anz_regler, fp_csvfile_2) < 0)
          {
            do_cleanup(fenster1, fenster2);
            exit(-1);
          }
        }
      }
    } /* Ende: if ( merk_monat != merk_zeit->tm_mon) */
    merk_monat = merk_zeit->tm_mon;
    write_CSVFile(anz_regler, fp_csvfile_2, daten_zeitpunkt );
    fprintf(stderr," %s - korrekter csv Daten output! (von %d)\n",sZeit,(int)daten_zeitpunkt);
  }

}

/* Aufruf der Funktionen zur Berechnung der Werte */
void berechne_werte(int anz_regler)
{
  int i, j, anz_bytes_1 = 0, anz_bytes_2 = 0;

  /* anz_regler = 1 : nur ein Regler vorhanden oder der erste Regler bei 0xD1 */
  if (anz_regler == 2)
  {
    i = 0;
    switch(akt_daten[0])
    {
      case UVR1611: anz_bytes_1 = 56; break; /* UVR1611 */
      case UVR61_3: anz_bytes_1 = 27; break; /* UVR61-3 */
    }
    switch(akt_daten[anz_bytes_1])
    {
      case UVR1611: anz_bytes_2 = 56; break; /* UVR1611 */
      case UVR61_3: anz_bytes_2 = 27; break; /* UVR61-3 */
    }

#ifdef DEBUG
    fprintf(stderr,"Anzahl Bytes Geraet-1: %d Anzahl Bytes Geraet-1: %d\n",anz_bytes_1,anz_bytes_2);
    for (i=0;i<(anz_bytes_1+anz_bytes_2);i++) // Testausgabe 2DL
    {
        fprintf(stderr,"%2x; ", akt_daten[i]);
    }
    fprintf(stderr,"\n");
#endif
    i = 0;
    for (j=anz_bytes_1;j<(anz_bytes_1+anz_bytes_2) ;j++)
    {
      akt_daten[i] = akt_daten[j];
      i++;
    }
    akt_daten[i]='\0'; /* mit /000 abschliessen!! */
#ifdef DEBUG
    for (i=0;i<anz_bytes_2;i++) // Testausgabe 2DL
    {
        fprintf(stderr,"%2x; ", akt_daten[i]);
    }
    fprintf(stderr,"\n");
#endif
  }

  temperaturanz(anz_regler);
  ausgaengeanz(anz_regler);
  drehzahlstufenanz(anz_regler);
  waermemengenanz(anz_regler);

}

/* Bearbeitung der Temperatur-Sensoren */
void temperaturanz(int regler)
{
  int i, j, anzSensoren=16;
  UCHAR temp_uvr_typ=0;
  temp_uvr_typ=uvr_typ;
  for(i=1;i<=anzSensoren;i++)
    SENS_Art[i]=0;

  if (regler == 2)  /* 2. Geraet vorhanden */
    uvr_typ = uvr_typ2;

  switch(uvr_typ)
  {
    case UVR1611: anzSensoren = 16; break; /* UVR1611 */
    case UVR61_3: anzSensoren = 6; break; /* UVR61-3 */
  }

  /* vor berechnetemp() die oberen 4 Bit des HighByte auswerten!!!! */
  /* Wichtig fuer Minus-Temp. */
  j=1;
  for(i=1;i<=anzSensoren;i++)
    {
      SENS_Art[i] = eingangsparameter(akt_daten[j+1]);
      switch(SENS_Art[i])
  {
  case 0: SENS[i] = 0; break;
  case 1: SENS[i] = 0; break; // digit. Pegel (AUS)
  case 2: SENS[i] = berechnetemp(akt_daten[j],akt_daten[j+1],SENS_Art[i]); break; // Temp.
  case 3: SENS[i] = berechnevol(akt_daten[j],akt_daten[j+1]); break;
  case 6: SENS[i] = berechnetemp(akt_daten[j],akt_daten[j+1],SENS_Art[i]); break; // Strahlung
  case 7: SENS[i] = berechnetemp(akt_daten[j],akt_daten[j+1],SENS_Art[i]); break; // Raumtemp.
  case 9: SENS[i] = 1; break; // digit. Pegel (EIN)
  case 10: SENS[i] = berechnetemp(akt_daten[j],akt_daten[j+1],SENS_Art[i]); break; // Minus-Temperaturen
  case 15: SENS[i] = berechnetemp(akt_daten[j],akt_daten[j+1],SENS_Art[i]); break; // Minus-Raumtemp.
  }
      j=j+2;
    }
  uvr_typ=temp_uvr_typ;
}

/* Bearbeitung der Ausgaenge */
void ausgaengeanz(int regler)
{
  //  int ausgaenge[13];
  // Ausgnge 2byte: low vor high
  // Bitbelegung:
  // AAAA AAAA
  // xxxA AAAA
  //  x ... dont care
  // A ... Ausgang (von low nach high zu nummerieren)
  int z;
  UCHAR temp_uvr_typ=0;
  temp_uvr_typ=uvr_typ;

  for (z=0;z<14;z++)
    AUSG[z] = 0;

  if (regler == 2)  /* 2. Geraet vorhanden */
    uvr_typ = uvr_typ2;

  if (uvr_typ == UVR1611) /* UVR1611 */
  {
    /* Ausgaenge belegt? */
    for(z=1;z<9;z++)
    {
        if (tstbit( akt_daten[33], z-1 ) == 1)
          AUSG[z] = 1;
        else
          AUSG[z] = 0;
    }
    for(z=1;z<6;z++)
    {
      if (tstbit( akt_daten[34], z-1 ) == 1)
        AUSG[z+8] = 1;
      else
        AUSG[z+8] = 0;
     }
  }
  else if (uvr_typ == UVR61_3) /* UVR61-3 */
  {
    for(z=1;z<4;z++)
    {
      if (tstbit( akt_daten[13], z-1 ) == 1)
        AUSG[z] = 1;
      else
        AUSG[z] = 0;
     }
  }
  uvr_typ=temp_uvr_typ;
}

/* Bearbeitung Drehzahlstufen */
void drehzahlstufenanz(int regler)
{
  UCHAR temp_uvr_typ=0;
  temp_uvr_typ=uvr_typ;

  if (regler == 2)  /* 2. Geraet vorhanden */
    uvr_typ = uvr_typ2;
  if (uvr_typ == UVR1611) /* UVR1611 */
  {
    if ( ((akt_daten[35] & UVR1611) == 0 ) && (akt_daten[35] != 0 ) ) /* ist das hochwertigste Bit gesetzt ? */
    {
      DZR[1] = 1;
      DZStufe[1] = akt_daten[35] & 0x1F; /* die drei hochwertigsten Bits loeschen */
    }
    else
    {
      DZR[1] = 0;
      DZStufe[1] = 0;
    }
    if ( ((akt_daten[36] & UVR1611) == 0 )  && (akt_daten[36] != 0 ) )/* ist das hochwertigste Bit gesetzt ? */
    {
      DZR[2] = 1;
      DZStufe[2] = akt_daten[36] & 0x1F; /* die drei hochwertigsten Bits loeschen */
    }
    else
    {
      DZR[2] = 0;
      DZStufe[2] = 0;
    }
    if ( ((akt_daten[37] & UVR1611) == 0 )  && (akt_daten[37] != 0 ) )/* ist das hochwertigste Bit gesetzt ? */
    {
      DZR[6] = 1;
      DZStufe[6] = akt_daten[37] & 0x1F; /* die drei hochwertigsten Bits loeschen */
    }
    else
    {
      DZR[6] = 0;
      DZStufe[6] = 0;
    }
    if (akt_daten[38] != UVR1611 ) /* angepasst: bei UVR1611 ist die Pumpe aus! */
    {
      DZR[7] = 1;
      DZStufe[7] = akt_daten[38] & 0x1F; /* die drei hochwertigsten Bits loeschen */
    }
    else
    {
      DZR[7] = 0;
      DZStufe[7] = 0;
    }
  }
    else if (uvr_typ == UVR61_3) /* UVR61-3 */
  {
    if ( ((akt_daten[14] & UVR61_3) == 0 ) && (akt_daten[14] != 0 ) ) /* ist das hochwertigste Bit gesetzt ? */
    {
      DZR[1] = 1;
      DZStufe[1] = akt_daten[14] & 0x1F; /* die drei hochwertigsten Bits loeschen */
    }
    else
    {
      DZR[1] = 0;
      DZStufe[1] = 0;
    }
  }
  uvr_typ=temp_uvr_typ;
}

/* Bearbeitung Waermemengen-Register und -Zaehler */
void waermemengenanz(int regler)
{
  //  float momentLstg1, kwh1, mwh1, momentLstg2, kwh2, mwh2;
  WMReg[1] = 0;
  WMReg[2] = 0;
  int tmp_wert;
  UCHAR temp_uvr_typ=0;
  temp_uvr_typ=uvr_typ;

  if (regler == 2)  /* 2. Geraet vorhanden */
    uvr_typ = uvr_typ2;

  if (uvr_typ == UVR1611) /* UVR1611 */
  {
    switch(akt_daten[39])
    {
    case 1: WMReg[1] = 1; break; /* Waermemengenzaehler1 */
    case 2: WMReg[2] = 1; break; /* Waermemengenzaehler2 */
    case 3: WMReg[1] = 1;        /* Waermemengenzaehler1 */
            WMReg[2] = 1;  break; /* Waermemengenzaehler2 */
    }

    if (WMReg[1] == 1)
    {
      if ( akt_daten[43] > 0x7f ) /* negtive Wete */
      {
        tmp_wert = (10*((65536*(float)akt_daten[43]+256*(float)akt_daten[42]+(float)akt_daten[41])-65536)-((float)akt_daten[40]*10)/256);
        tmp_wert = tmp_wert | 0xffff0000;
        Mlstg[1] = tmp_wert / 100;
      }
      else
        Mlstg[1] = (10*(65536*(float)akt_daten[43]+256*(float)akt_daten[42]+(float)akt_daten[41])+((float)akt_daten[40]*10)/256)/100;

      W_kwh[1] = ( (float)akt_daten[45]*256 + (float)akt_daten[44] )/10;
      W_Mwh[1] = (akt_daten[47]*0x100 + akt_daten[46]);
    }
    if (WMReg[2] == 1)
    {
      if ( akt_daten[51] > 0x7f )  /* negtive Wete */
      {
        tmp_wert = (10*((65536*(float)akt_daten[51]+256*(float)akt_daten[50]+(float)akt_daten[49])-65536)-((float)akt_daten[48]*10)/256);
        tmp_wert = tmp_wert | 0xffff0000;
        Mlstg[2] = tmp_wert / 100;
      }
      else
        Mlstg[2] = (10*(65536*(float)akt_daten[51]+256*(float)akt_daten[50]+(float)akt_daten[49])+((float)akt_daten[48]*10)/256)/100;

      W_kwh[2] = ((float)akt_daten[53]*256 + (float)akt_daten[52])/10;
      W_Mwh[2] = (akt_daten[55]*0x100 + akt_daten[54]);
    }
  }

  if (uvr_typ == UVR61_3) /* UVR61-3 */
  {
    if (akt_daten[16] == 1) /* ( tstbit(akt_daten[16],0) == 1)  akt_daten[17] und akt_daten[18] sind Volumenstrom */
      WMReg[1] = 1;
    if (WMReg[1] == 1)
    {
      Mlstg[1] = (256*(float)akt_daten[20]+(float)akt_daten[19])/10;
      W_kwh[1] = ( (float)akt_daten[22]*256 + (float)akt_daten[21] )/10;
      W_Mwh[1] = akt_daten[26]*0x1000000 + akt_daten[25]*0x10000 + akt_daten[24]*0x100 + akt_daten[23];
    }
  }
  uvr_typ=temp_uvr_typ;
}

/* Funktion zum Testen */
void testfunktion(void)
{
  int result;
  UCHAR sendbuf[1];       /*  sendebuffer fuer die Request-Commandos*/
//  int sendbuf[1];       /*  sendebuffer fuer die Request-Commandos*/
  UCHAR empfbuf[256];

  sendbuf[0]=VERSIONSABFRAGE;   /* Senden der Versionsabfrage */
  write_erg=write(fd,sendbuf,1);
  if (write_erg == 1)    /* Lesen der Antwort*/
    result=read(fd,empfbuf,1);
  fprintf(stderr,"Vom DL erhalten Version: %x\n",empfbuf[0]);

  sendbuf[0]=FWABFRAGE;    /* Senden der Firmware-Versionsabfrage */
  write_erg=write(fd,sendbuf,1);
  if (write_erg == 1)    /* Lesen der Antwort*/
    result=read(fd,empfbuf,1);
  fprintf(stderr,"Vom DL erhalten Version FW: %x\n",empfbuf[0]);

  sendbuf[0]=MODEABFRAGE;    /* Senden der Modus-abfrage */
  write_erg=write(fd,sendbuf,1);
  if (write_erg == 1)    /* Lesen der Antwort*/
    result=read(fd,empfbuf,1);
  fprintf(stderr,"Vom DL erhalten Modus: %x\n",empfbuf[0]);
}

/* aktuelles Datum und aktuelle Zeit als String fuer Log erzeugen*/
void zeitstempel(void)
{
  struct tm *zeit;
  time_t sekunde;

  time(&sekunde);
  zeit = localtime(&sekunde);

  strftime(sDatum, 10, "%x", zeit);
  strftime(sZeit, 10, "%X", zeit);
}

/* Berechne die Temperatur / Strahlung */
float berechnetemp(UCHAR lowbyte, UCHAR highbyte, int sensor)
{
  UCHAR temp_highbyte;
  int z;
  short wert;

  temp_highbyte = highbyte;
  wert = lowbyte | ((highbyte & 0x0f)<<8);

  if( (highbyte & UVR1611) != 0 )
    {
      wert = wert ^ 0xfff;
      wert = -wert -1 ;
      return ((float) wert / 10);
    }
  else
    {
      if( sensor == 7)  // Raumtemperatur
      {
        if( (highbyte & 0x01) != 0 )
          return((256 + (float)lowbyte) / 10); /* Temperatur in C */
        else
          return(((float)lowbyte) / 10); /* Temperatur in C */
      }
      else
      {
        for(z=5;z<9;z++)
          temp_highbyte = temp_highbyte & ~(1 << z); /* die oberen 4 Bit auf 0 setzen */
        if (sensor == 6)
          return(((float)temp_highbyte*256) + (float)lowbyte); /* Strahlung in W/m2 */
        else
          return((((float)temp_highbyte*256) + (float)lowbyte) / 10); /* Temperatur in C */
      }
    }
}

/* Berechne Volumenstrom */
float berechnevol(UCHAR lowbyte, UCHAR highbyte)
{
  UCHAR temp_highbyte;
  int z;
  short wert;

  temp_highbyte = highbyte;
  wert = lowbyte | ((highbyte & 0x0f)<<8);

  //  if( (highbyte & UVR1611) != 0 )
  if( (highbyte & UVR1611) == 0 ) /* Volumenstrom kann nicht negativ sein*/
    {
      for(z=5;z<9;z++)
  temp_highbyte = temp_highbyte & ~(1 << z); /* die oberen 4 Bit auf 0 setzen */

      wert = (lowbyte | ((temp_highbyte & 0x0f)<<8)) * 4;
      return ((float) wert );
    }
  else
    {
      return( 0 ); /* Volumenstrom in l/h */
    }
}

/* Ermittle die Art des Eingangsparameters */
int eingangsparameter(UCHAR highbyte)
{

#if 0
  /*
    Eing"ange 2 byte,  low vor high
    Bitbelegung:
    TTTT TTTT
    VEEE TTTT
    T ... Eingangswert
    V ... Vorzeichen (1=Minus)
    E ... Type (Einheit) des Eingangsparameters:
    x000 xxxx  Eingang unbenutzt
    D001 xxxx  digitaler Pegel (Bit D)
    V010 TTTT  Temperatur (in 1/10 C)
    V011 TTTT  Volumenstrom (in 4 l/h)
    V110 TTTT  Strahlung (in 1 W/m)
    V111 xRRT  Temp. Raumsensor(in 1/10 C)

  */
#endif

  int bitbyte;
  bitbyte=0;

  if (tstbit(highbyte,4) == 1)
    bitbyte=setbit(bitbyte,0);

  if (tstbit(highbyte,5) == 1)
    bitbyte=setbit(bitbyte,1);

  if (tstbit(highbyte,6) == 1)
    bitbyte=setbit(bitbyte,2);

  if (tstbit(highbyte,7) == 1)
    bitbyte=setbit(bitbyte,3);

  return bitbyte;
}

/* Loesche ein bestimmtes Bit */
int clrbit( int word, int bit )
{
  return (word & ~(1 << bit));
}

/* Teste, ob ein bestimmtes Bit 0 oder 1 ist */
int tstbit( int word, int bit )
{
  return ((word & (1 << bit)) != 0);
}

/* Setze ein bestimmtes Bit */
int setbit( int word, int bit )
{
  return (word | (1 << bit));
}

/* Invertiere ein bestimmtes Bit */
int xorbit( int word, int bit )
{
  return (word ^ (1 << bit));
}

/* Ausgabe der Werte auf den Bildschirm unter Nutzung von ncurses */
void bildschirmausgabe(WINDOW *fenster)
{
  int i, j, c, unsichtbar;
  UCHAR temp_byte = 0;

  c=0;
  j = 1;
  i = 1;
  unsichtbar = 1;
  clear();
  if (bool_farbe)
    wattrset(fenster,A_NORMAL | COLOR_PAIR(11));
  else
    wattron(fenster,A_UNDERLINE);
  mvwprintw(fenster,0,35," Sensoren ");
  wattroff(fenster,A_UNDERLINE);
  j = 1;
  i = 1;
  for(j=1;j<6;j++)          /* Temperaturen */
  {
    set_attribut(j,fenster);
    if (ext_bezeichnung)
    {
      mvwprintw(fenster,j,1,"%s",pBez_S[i]);
      if ( strcmp(pBez_S[i],"               " ) > 0 )
        unsichtbar = 0;
    }
    else
      mvwprintw(fenster,j,1,"S%d",i);
    if ( !unsichtbar || !ext_bezeichnung )
    {
      switch(SENS_Art[i])
      {
        case 0: mvwprintw(fenster,j,17,": ---",SENS[i]); break;
        case 1: mvwprintw(fenster,j,17,": AUS"); break;
        case 2: mvwprintw(fenster,j,17,": %.1f C",SENS[i]); break;
        case 3: mvwprintw(fenster,j,17,": %.0f l/h",SENS[i]); break;
        case 6: mvwprintw(fenster,j,17,": %.0f W/m2 ",SENS[i]); break;
        case 7: mvwprintw(fenster,j,17,": %.1f C",SENS[i]); break;
        case 9: mvwprintw(fenster,j,17,": EIN"); break;
        case 10: mvwprintw(fenster,j,17,": %.1f C",SENS[i]); break;
        case 15: mvwprintw(fenster,j,17,": %.1f C",SENS[i]); break;
      }
    }
    i++;
    unsichtbar = 1;
    if (ext_bezeichnung)
    {
      mvwprintw(fenster,j,28,"%s",pBez_S[i]);
      if ( strcmp(pBez_S[i],"               " ) > 0 )
        unsichtbar = 0;
    }
    else
      mvwprintw(fenster,j,28,"S%d",i);
    if ( !unsichtbar || !ext_bezeichnung )
    {
      switch(SENS_Art[i])
      {
        case 0: mvwprintw(fenster,j,44,": ---",SENS[i]); break;
        case 1: mvwprintw(fenster,j,44,": AUS"); break;
        case 2: mvwprintw(fenster,j,44,": %.1f C",SENS[i]); break;
        case 3: mvwprintw(fenster,j,44,": %.0 fl/h",SENS[i]); break;
        case 6: mvwprintw(fenster,j,44,": %.0f W/m2",SENS[i]); break;
        case 7: mvwprintw(fenster,j,44,": %.1f C",SENS[i]); break;
        case 9: mvwprintw(fenster,j,44,": EIN"); break;
        case 10: mvwprintw(fenster,j,44,": %.1f C",SENS[i]); break;
        case 15: mvwprintw(fenster,j,44,": %.1f C",SENS[i]); break;
      }
    }
    i++;
    unsichtbar = 1;
    if (ext_bezeichnung)
    {
      mvwprintw(fenster,j,55,"%s",pBez_S[i]);
      if ( strcmp(pBez_S[i],"               " ) > 0 )
        unsichtbar = 0;
    }
    else
      mvwprintw(fenster,j,55,"S%d",i);
    if ( !unsichtbar || !ext_bezeichnung )
    {
      switch(SENS_Art[i])
      {
        case 0: mvwprintw(fenster,j,71,": ---",SENS[i]); break;
        case 1: mvwprintw(fenster,j,71,": AUS"); break;
        case 2: mvwprintw(fenster,j,71,": %.1f C",SENS[i]); break;
        case 3: mvwprintw(fenster,j,71,": %.0f l/h",SENS[i]); break;
        case 6: mvwprintw(fenster,j,71,": %fW/m ",SENS[i]); break;
        case 7: mvwprintw(fenster,j,71,": %.1f C",SENS[i]); break;
        case 9: mvwprintw(fenster,j,17,": EIN"); break;
        case 10: mvwprintw(fenster,j,71,": %.1f C",SENS[i]); break;
        case 15: mvwprintw(fenster,j,71,": %.1f C",SENS[i]); break;
      }
    }
    i++;
  unsichtbar = 1;
  }
  set_attribut(j,fenster);
  unsichtbar = 1;
  if (ext_bezeichnung)
  {
    mvwprintw(fenster,j,1,"%s",pBez_S[i]);
    if ( strcmp(pBez_S[i],"               " ) > 0 )
      unsichtbar = 0;
  }
  else
    mvwprintw(fenster,j,1,"S%d",i); /*Sensor S16 - Impulssensor (muss aber nicht sein!) */
//  if(AUSG[4] == 0)
//    SENS[i]=0;
  if ( !unsichtbar || !ext_bezeichnung )
  {
    switch(SENS_Art[i])
    {
      case 0: mvwprintw(fenster,j,17,": ----",SENS[i]); break;
      case 1: mvwprintw(fenster,j,17,": AUS"); break;
      case 2: mvwprintw(fenster,j,17,": %.1f C",SENS[i]); break;
      case 3: mvwprintw(fenster,j,17,": %.0f l/h",SENS[i]); break;
      case 6: mvwprintw(fenster,j,17,": %.0f W/m2 ",SENS[i]); break;
      case 7: mvwprintw(fenster,j,17,": %.1f C",SENS[i]); break;
      case 9: mvwprintw(fenster,j,17,": EIN"); break;
      case 10: mvwprintw(fenster,j,17,": %.1f C",SENS[i]); break;
      case 15: mvwprintw(fenster,j,17,": %.1f C",SENS[i]); break;
    }
  }
  i=1;
  j=j+2;
  if (bool_farbe)
    wattrset(fenster,A_NORMAL  | COLOR_PAIR(11));
  else
    wattron(fenster,A_UNDERLINE);
  mvwprintw(fenster,j-1,35," Ausgaenge ");
  wattroff(fenster,A_UNDERLINE);
  for(j=8;j<12;j++)            /* Ausgaenge */
  {
    set_attribut(j,fenster);
    unsichtbar = 1;
    if (ext_bezeichnung)
    {
      mvwprintw(fenster,j,1,"%s",pBez_A[i]);
      if ( strcmp(pBez_A[i],"               " ) > 0 )
        unsichtbar = 0;
    }
    else
      mvwprintw(fenster,j,1,"A%d",i);
    if ( !unsichtbar || !ext_bezeichnung )
    {
      if(AUSG[i] == 1)
        mvwprintw(fenster,j,17,": ein");
      else
        mvwprintw(fenster,j,17,": aus");
    }
    i++;
    unsichtbar = 1;
    if (ext_bezeichnung)
    {
      mvwprintw(fenster,j,28,"%s",pBez_A[i]);
      if ( strcmp(pBez_A[i],"               " ) > 0 )
        unsichtbar = 0;
    }
    else
      mvwprintw(fenster,j,28,"A%d",i);
    if ( !unsichtbar || !ext_bezeichnung )
    {
      if(AUSG[i] == 1)
        mvwprintw(fenster,j,44,": ein");
      else
        mvwprintw(fenster,j,44,": aus");
    }
    i++;
    unsichtbar = 1;
    if (ext_bezeichnung)
    {
      mvwprintw(fenster,j,55,"%s",pBez_A[i]);
      if ( strcmp(pBez_A[i],"               " ) > 0 )
        unsichtbar = 0;
    }
    else
      mvwprintw(fenster,j,55,"A%d",i);
    if ( !unsichtbar || !ext_bezeichnung )
    {
      if(AUSG[i] == 1)
        mvwprintw(fenster,j,71,": ein");
      else
        mvwprintw(fenster,j,71,": aus");
    }
    i++;
  }
  set_attribut(j,fenster);
  unsichtbar = 1;
  if (ext_bezeichnung)  /* A13 bzw. Analog bei UVR61-3 */
  {
    mvwprintw(fenster,j,1,"%s",pBez_A[i]);
      if ( strcmp(pBez_A[i],"               " ) > 0 )
        unsichtbar = 0;
    }
  else
    mvwprintw(fenster,j,1,"A%d",i);
  if (uvr_typ == UVR1611)
  {
    if ( !unsichtbar || !ext_bezeichnung )
    {
      if(AUSG[i] == 1)
        mvwprintw(fenster,j,17,": ein");
      else
        mvwprintw(fenster,j,17,": aus");
    }
  }
  if (uvr_typ == UVR61_3)
  {
    if ( !unsichtbar || !ext_bezeichnung )
    {
      if (tstbit(akt_daten[16],7) == 1) /* Analogausgang */
      {
        temp_byte = akt_daten[16] & ~(1 << 8); /* oberestes Bit auf 0 setzen */
        mvwprintw(fenster,j,17,": %.1f;",(float)temp_byte / 10);
      }
      else
        mvwprintw(fenster,j,17,": nicht aktiv");
    }
  }

  j=j+2;
  set_attribut(j,fenster);
  unsichtbar = 1;
  /*Drehzahlregler / -stufen */
  if (ext_bezeichnung)
  {
    mvwprintw(fenster,j,1,"%s",pBez_DZR[1]);
    if ( strcmp(pBez_DZR[1],"               " ) > 0 )
      unsichtbar = 0;
  }
  else
    mvwprintw(fenster,j,1,"Drehzahlregler 1");
  if ( !unsichtbar || !ext_bezeichnung )
  {
    if(DZR[1] == 1)
    {
      mvwprintw(fenster,j,34,": ein");
      if (ext_bezeichnung)
        mvwprintw(fenster,j,42,"%s",pBez_DZS[1]);
      else
        mvwprintw(fenster,j,42,"Drehzahlstufe 1");
      mvwprintw(fenster,j,73,": %i",DZStufe[1]);
    }
    else
    {
      mvwprintw(fenster,j,34,": aus");
      if (ext_bezeichnung)
        mvwprintw(fenster,j,42,"%s",pBez_DZS[1]);
      else
        mvwprintw(fenster,j,42,"Drehzahlstufe 1");
      mvwprintw(fenster,j,73,": %i",DZStufe[1]);
    }
  }
  j++;
  set_attribut(j,fenster);
  unsichtbar = 1;
  if (ext_bezeichnung)
  {
    mvwprintw(fenster,j,1,"%s",pBez_DZR[2]);
    if ( strcmp(pBez_DZR[2],"               " ) > 0 )
      unsichtbar = 0;
  }
  else
    mvwprintw(fenster,j,1,"Drehzahlregler 2");
  if ( !unsichtbar || !ext_bezeichnung )
  {
    if(DZR[2] == 1)
    {
      mvwprintw(fenster,j,34,": ein");
      if (ext_bezeichnung)
        mvwprintw(fenster,j,42,"%s",pBez_DZS[2]);
      else
        mvwprintw(fenster,j,42,"Drehzahlstufe 2");
      mvwprintw(fenster,j,73,": %i",DZStufe[2]);
    }
    else
    {
      mvwprintw(fenster,j,34,": aus");
      if (ext_bezeichnung)
        mvwprintw(fenster,j,42,"%s",pBez_DZS[2]);
      else
        mvwprintw(fenster,j,42,"Drehzahlstufe 2");
      mvwprintw(fenster,j,73,": %i",DZStufe[2]);
    }
  }
  j++;
  set_attribut(j,fenster);
  unsichtbar = 1;
  if (ext_bezeichnung)
  {
    mvwprintw(fenster,j,1,"%s",pBez_DZR[6]);
    if ( strcmp(pBez_DZR[6],"               " ) > 0 )
      unsichtbar = 0;
  }
  else
    mvwprintw(fenster,j,1,"Drehzahlregler 6");
  if ( !unsichtbar || !ext_bezeichnung )
  {
    if(DZR[6] == 1)
    {
      mvwprintw(fenster,j,34,": ein");
      if (ext_bezeichnung)
        mvwprintw(fenster,j,42,"%s",pBez_DZS[6]);
      else
        mvwprintw(fenster,j,42,"Drehzahlstufe 6");
      mvwprintw(fenster,j,73,": %i",DZStufe[6]);
    }
    else
    {
      mvwprintw(fenster,j,34,": aus");
      if (ext_bezeichnung)
        mvwprintw(fenster,j,42,"%s",pBez_DZS[6]);
      else
        mvwprintw(fenster,j,42,"Drehzahlstufe 6");
      mvwprintw(fenster,j,73,": %i",DZStufe[6]);
    }
  }
  j++;
  set_attribut(j,fenster);
  unsichtbar = 1;
  if (ext_bezeichnung)
  {
    mvwprintw(fenster,j,1,"%s",pBez_DZR[7]);
    if ( strcmp(pBez_DZR[7],"               " ) > 0 )
      unsichtbar = 0;
  }
  else
    mvwprintw(fenster,j,1,"Drehzahlregler 7");
  if ( !unsichtbar || !ext_bezeichnung )
  {
    if(DZR[7] == 1)
    {
      mvwprintw(fenster,j,34,": ein");
      if (ext_bezeichnung)
        mvwprintw(fenster,j,42,"%s",pBez_DZS[7]);
      else
        mvwprintw(fenster,j,42,"Drehzahlstufe 7");
      mvwprintw(fenster,j,73,": %i",DZStufe[7]);
    }
    else
    {
      mvwprintw(fenster,j,34,": aus");
      if (ext_bezeichnung)
        mvwprintw(fenster,j,42,"%s",pBez_DZS[7]);
      else
        mvwprintw(fenster,j,42,"Drehzahlstufe 7");
      mvwprintw(fenster,j,73,": %i",DZStufe[7]);
    }
  }
  j=j+2;
  set_attribut(j,fenster);
  /* Momentanleistung und Waermemengenzaehler */
  mvwprintw(fenster,j,1,"Momentanleistung 1: %.1f kW",Mlstg[1]);
  mvwprintw(fenster,j,35,"Waermemengenzaehler 1: %i MWh, %.1f kWh",(int)W_Mwh[1],W_kwh[1]);
  j++;
  set_attribut(j,fenster);
  if (uvr_typ == UVR1611)
  {
    mvwprintw(fenster,j,1,"Momentanleistung 2: %.1f kW",Mlstg[2]);
    mvwprintw(fenster,j,35,"Waermemengenzaehler 2: %i MWh, %.1f kWh",(int)W_Mwh[2],W_kwh[2]);
  }

  /* Auskommentiert, da nur zu Anzeigezwecken drin:
     mvwprintw(fenster,18,1,"DRZ1 Solarpumpe: %x - DRZ2:  %x - DRZ3: %x - DRZ4 Heizkreis: %x ",akt_daten[35],akt_daten[36],akt_daten[37],akt_daten[38]);

     mvwprintw(fenster,18,1,"T - E - S - T  ===>  Aussentemp. : 0x%x 0x%x %.1f C",akt_daten[23],akt_daten[24],S[12]);
  */
  noecho();
  wattrset(fenster,A_NORMAL);
  if (bool_farbe)
    wattrset(fenster,A_NORMAL | COLOR_PAIR(4));
  mvwprintw(fenster,22, 1, "Beenden -> Taste >q< druecken");
  wattrset(fenster,A_NORMAL);

  curs_set(0);
  wrefresh(fenster);

}

/* Kopfsatz des CSV-Files schreiben */
int  write_header2CSV(int regler, FILE *fp)
{
  int i=0;
  UCHAR temp_uvr_typ=0;

  temp_uvr_typ=uvr_typ;

  if (regler == 2)  /* 2. Geraet vorhanden */
    uvr_typ = uvr_typ2;

  if (fp)
  {
  //printf("fp ist nicht NULL. uvr_typ: %x\n",uvr_typ);
    if (uvr_typ == UVR1611)  /* UVR1611 */
      fprintf(fp," Datum ; Zeit ; Sens1 ; Sens2 ; Sens3 ; Sens4 ; Sens5 ; Sens6 ; Sens7 ; Sens8 ; Sens9 ; Sens10 ; Sens11 ; Sens12; Sens13 ; Sens14 ; Sens15 ; Sens16 ; Ausg1 ; Drehzst_A1 ; Ausg2 ; Drehzst_A2 ; Ausg3 ; Ausg4 ; Ausg5 ; Ausg6 ; Drehzst_A6 ; Ausg7 ; Drehzst_A7 ; Ausg8 ; Ausg9 ; Ausg10 ; Ausg11 ; Ausg12 ; Ausg13 ; kW1 ; kWh1 ; kW2 ; kWh2 \n");

    if (uvr_typ == UVR61_3)  /* UVR61-3 */
      fprintf(fp," Datum ; Zeit ; Sens1 ; Sens2 ; Sens3 ; Sens4 ; Sens5 ; Sens6 ; Ausg1 ; Drehzst_A1 ; Ausg2 ; Ausg3 ; Analogausg ; Vol-Strom ; Momentanlstg ; MWh ; kWh \n");

    char  kommentar[16384];
    sprintf(kommentar," Datum ; Zeit ;");

    if (ext_bezeichnung && uvr_typ == UVR1611)  /* config-Datei und UVR1611 */
    {
      for(i=1;i<=16;i++) /* sensor-Bezeichnungen */
      {
        if(strlen(kommentar) > 16000 || pBez_S[i] == NULL)
        {
          fprintf(stderr," trouble in logfileheader: S%d - length=%d\n",i,(int)strlen(kommentar));
          return -1;
        }
        sprintf(kommentar+strlen(kommentar)," %s;",pBez_S[i]);
      }
      for (i=1;i<=2;i++)  /* Ausgangs-Bezeichnungen mit Drehzahl */
      {
        if(pBez_A[i] != NULL && pBez_DZS[i]!=NULL)
        {
          if ( (strlen(kommentar) > 16000) || (pBez_A[i] == NULL) || (pBez_DZS[i]==NULL) )
          {
            fprintf(stderr," trouble in logfileheader: A%d - length=%d\n",i,(int)strlen(kommentar));
            return -1;
          }
          sprintf(kommentar+strlen(kommentar),"%s;%s;",pBez_A[i],pBez_DZS[i]);
        }
      }
      for (i=3;i<=5;i++)  /* Ausgangs-Bezeichnungen */
      {
        if((strlen(kommentar) > 16000) || (pBez_A[i] == NULL))
        {
          fprintf(stderr," trouble in logfileheader: A%d - length=%d\n",i,(int)strlen(kommentar));
          return -1;
        }
        sprintf(kommentar+strlen(kommentar),"%s;",pBez_A[i]);
      }
      for (i=6;i<=7;i++)  /* Ausgangs-Bezeichnungen mit Drehzahl */
      {
        if ( (strlen(kommentar) > 16000) || (pBez_A[i] == NULL) || (pBez_DZS[i]==NULL) )
        {
          fprintf(stderr," trouble in logfileheader: A%d - length=%d\n",i,(int)strlen(kommentar));
          return -1;
        }
        sprintf(kommentar+strlen(kommentar),"%s;%s;",pBez_A[i],pBez_DZS[i]);
      }
      for (i=8;i<=13;i++)   /* Ausgangs-Bezeichnungen */
      {
        if( (strlen(kommentar) > 16000) || (pBez_A[i] == NULL) )
        {
          fprintf(stderr," trouble in logfileheader: A%d - length=%d\n",i,(int)strlen(kommentar));
          return -1;
        }
        sprintf(kommentar+strlen(kommentar),"%s;",pBez_A[i]);
      }

      fprintf(fp,"%s kW1; kWh1; kW2; kWh2 \n",kommentar);
    }
    else if (uvr_typ == UVR1611)
    {
      fprintf(fp,"Sens1 ; Sens2 ; Sens3 ; Sens4 ; Sens5 ; Sens6 ; Sens7 ; Sens8 ; Sens9 ; Sens10 ; Sens11 ; Sens12; Sens13 ; Sens14 ; Sens15 ; Sens16 ; Ausg1 ; Drehzst_A1 ; Ausg2 ; Drehzst_A2 ; Ausg3 ; Ausg4 ; Ausg5 ; Ausg6 ; Drehzst_A6 ; Ausg7 ; Drehzst_A7 ; Ausg8 ; Ausg9 ; Ausg10 ; Ausg11 ; Ausg12 ; Ausg13 ; kW1 ; kWh1 ; kW2 ; kWh2 \n");
    }

//---- 24.12.2007 neu Bezeichnung für UVR61-3
    if (ext_bezeichnung && uvr_typ == UVR61_3)  /* config-Datei und UVR61-3 */
    {
      for(i=1;i<=6;i++) /* sensor-Bezeichnungen */
      {
        if(strlen(kommentar) > 16000 || pBez_S[i] == NULL)
        {
          fprintf(stderr," trouble in logfileheader: S%d - length=%d\n",i,(int)strlen(kommentar));
          return -1;
        }
        sprintf(kommentar+strlen(kommentar)," %s;",pBez_S[i]);
      }
      /* Ausgangs-Bezeichnungen mit Drehzahl */
      if(pBez_A[1] != NULL && pBez_DZS[1]!=NULL)
      {
        if ( (strlen(kommentar) > 16000) || (pBez_A[1] == NULL) || (pBez_DZS[1]==NULL) )
        {
          fprintf(stderr," trouble in logfileheader: A%d - length=%d\n",i,(int)strlen(kommentar));
          return -1;
        }
        sprintf(kommentar+strlen(kommentar),"%s;%s;",pBez_A[1],pBez_DZS[1]);
      }
      for (i=2;i<=3;i++)  /* Ausgangs-Bezeichnungen */
      {
        if((strlen(kommentar) > 16000) || (pBez_A[i] == NULL))
        {
          fprintf(stderr," trouble in logfileheader: A%d - length=%d\n",i,(int)strlen(kommentar));
          return -1;
        }
        sprintf(kommentar+strlen(kommentar),"%s;",pBez_A[i]);
      }

      fprintf(fp,"%s Analogausg ; Vol-Strom ; Momentanlstg ; MWh ; kWh \n",kommentar);
    }
//----

    fflush(fp);
    fprintf(stderr," logfile header done\n");
  }
  uvr_typ = temp_uvr_typ;
  return(0);
}

/* Ausgabe der Werte in ein csv-File */
void write_CSVFile(int regler, FILE *fp, time_t datapoint_time)
{
  int i=0;
  int anzSensoren = 16;
  UCHAR temp_byte = 0;
  UCHAR temp_uvr_typ=0;

  temp_uvr_typ=uvr_typ;

  if (regler == 2)  /* 2. Geraet vorhanden */
    uvr_typ = uvr_typ2;

  if (!fp)
    return;

  switch(uvr_typ)
  {
    case UVR1611: anzSensoren = 16; break; /* UVR1611 */
    case UVR61_3: anzSensoren = 6; break; /* UVR61-3 */
  }

  struct tm *zeit;
  zeit = localtime(&datapoint_time);

  char  tag[5],monat[5],jahr[10],uhrzeit[256];
  strftime(tag, 3, "%d", zeit);
  strftime(monat, 3, "%m", zeit);
  strftime(jahr, 3, "%y", zeit);
  strftime(uhrzeit, 10, "%X", zeit);

  /* WINSOL hat kein \n am Ende des ascii output */
  /* deshalb hier */
  fprintf(fp,"\n%s.%s.%s;%s;",tag,monat,jahr,uhrzeit);

  for(i=1;i<=anzSensoren;i++) /* sensor-Bezeichnungen */
  {
    switch(SENS_Art[i])
    {
    case 0: fprintf(fp," ---;"); break; /* nicht belegt */
    case 1: fprintf(fp," %1.0f;",SENS[i]); break; /* digitaler Eingang */
    case 2:
    case 3: fprintf(fp," %5.1f;",SENS[i]); break; /* Temp / flow */
    case 6: fprintf(fp," %5.0f;",SENS[i]); break; /* Strahlungssensor */
    case 7: fprintf(fp," %5.1f;",SENS[i]); break;
    case 9: fprintf(fp," %1.0f;",SENS[i]); break; /* digitaler Eingang */
    case 10: fprintf(fp," %5.1f;",SENS[i]); break;
    case 15: fprintf(fp," %5.1f;",SENS[i]); break;
    }
  }

  if (uvr_typ == UVR1611) /* UVR1611 */
  {
    /* A1/A2 */
    for (i=1;i<=2;i++)  /* Ausgangs-Bezeichnungen mit Drehzahl */
    {
#if DEBUG>3
      fprintf(stderr,"A%d=%d  dzr=%d  dzs=%d\n",i,AUSG[i],DZR[i],DZStufe[i]);
#endif
      if (DZR[i] == 1 )
        fprintf(fp,"%2d;%2d;",AUSG[i],DZStufe[i]);
      else
        fprintf(fp,"%2d;%2d;",AUSG[i],DZStufe[i]);
          /*  fprintf(fp,"%2d; --;",AUSG[i]); */
    }
    /* A3-A5 */
    for (i=3;i<=5;i++)  /* Ausgangs-Bezeichnungen */
    {
#if DEBUG>3
      fprintf(stderr,"A%2d=%2d\n",i,AUSG[i]);
#endif
      fprintf(fp,"%2d;",AUSG[i]);
    }

    for (i=6;i<=7;i++)  /* Ausgangs-Bezeichnungen mit Drehzahl */
    {
#if DEBUG>3
      fprintf(stderr,"A%d=%d dzr=%d  dzs=%d\n",i,AUSG[i],DZR[i],DZStufe[i]);
#endif
      if(DZR[i] == 1)
        fprintf(fp,"%2d;%2d;",AUSG[i],DZStufe[i]);
      else
        fprintf(fp,"%2d;%2d;",AUSG[i],DZStufe[i]);
        /* fprintf(fp,"%2d; --;",AUSG[i]); */
    }

  for (i=8;i<=13;i++)   /* Ausgangs-Bezeichnungen */
    {
#if DEBUG>3
      fprintf(stderr,"A%2d=%2d\n",i,AUSG[i]);
#endif
      fprintf(fp,"%2d;",AUSG[i]);
    }

    for (i=1;i<=2;i++)
    {
      if (WMReg[i] == 1)
//        fprintf(fp," %.1f;%.1f;", W_Mwh[i],W_kwh[i]);
		if (W_Mwh[i] > 0 )
            fprintf(fp," %.1f;%.0f%05.1f;",Mlstg[i], W_Mwh[i],W_kwh[i]);
        else
			fprintf(fp," %.1f;%.0f%.1f;",Mlstg[i], W_Mwh[i],W_kwh[i]);
      else
        fprintf(fp,"  ---;  ---;");
    }
  }

  if (uvr_typ == UVR61_3) /* UVR61-3 */
  {
    fprintf(fp,"%2d;%2d;",AUSG[1],DZStufe[1]);
    fprintf(fp,"%2d;%2d;",AUSG[2],AUSG[3]);

    if (tstbit(akt_daten[15],7) == 0) /* Analogausgang aktiv */
    {
      temp_byte = akt_daten[15] & ~(1 << 8); /* oberestes Bit auf 0 setzen */
      if ( ((float)temp_byte / 10) <= 10 )
        fprintf(fp," %.1f;",(float)temp_byte / 10);
      else
        fprintf(fp," 0,0;");
    }
    else
      fprintf(fp," 0,0;"); /* Analogausgang nicht aktiv */

    if (WMReg[1] == 1)
    {
      fprintf(fp,"%4d;",akt_daten[18]*0x100 + akt_daten[17]); /* Volumenstrom */
      fprintf(fp," %.1f;%.1f;%.1f;",Mlstg[1], W_Mwh[1],W_kwh[1]);
    }
    else
      fprintf(fp,"  ---;  ---;  ---;  ---;");

    fprintf(stdout,"\n");
  }

  fflush(fp);
  uvr_typ = temp_uvr_typ;
}


/* Ausgabe der Werte im csv-Format am Bildschirm */
void write_CSVCONSOLE(int regler, time_t datapoint_time)
{
  int i=0;
  int anzSensoren = 16;
  UCHAR temp_byte = 0;
  UCHAR temp_uvr_typ=0;

  struct tm *zeit;
  zeit = localtime(&datapoint_time);

  char  tag[5],monat[5],jahr[10],uhrzeit[256];
  strftime(tag, 3, "%d", zeit);
  strftime(monat, 3, "%m", zeit);
  strftime(jahr, 3, "%y", zeit);
  strftime(uhrzeit, 10, "%X", zeit);

  fprintf(stdout,"%s.%s.%s;%s;",tag,monat,jahr,uhrzeit);
  temp_uvr_typ = uvr_typ;
  if (regler == 2)  /* 2. Geraet vorhanden */
    uvr_typ = uvr_typ2;
//printf("UVR-Typ: %x\n",uvr_typ);
  switch(uvr_typ)
  {
    case UVR1611: anzSensoren = 16; break; /* UVR1611 */
    case UVR61_3: anzSensoren = 6; break; /* UVR61-3 */
  }

  for(i=1;i<=anzSensoren;i++) /* sensor-Bezeichnungen */
  {
    switch(SENS_Art[i])
    {
      case 0: fprintf(stdout," ---;"); break; /* nicht belegt */
      case 1: fprintf(stdout," %1.0f;",SENS[i]); break; /* digitaler Eingang */
      case 2:
      case 3: fprintf(stdout," %5.1f;",SENS[i]); break; /* Temp / flow */
      case 6: fprintf(stdout," %.0f;",SENS[i]); break;
      case 7: fprintf(stdout," %5.1f;",SENS[i]); break;
      case 9: fprintf(stdout," %1.0f;",SENS[i]); break; /* digitaler Eingang */
      case 10: fprintf(stdout," %5.1f;",SENS[i]); break;
      case 15: fprintf(stdout," %5.1f;",SENS[i]); break;
    }
  }

  if (uvr_typ == UVR1611)
  {
    /* A1/A2 */
    for (i=1;i<=2;i++)  /* Ausgangs-Bezeichnungen mit Drehzahl */
    {
#if DEBUG>3
      fprintf(stderr,"A%d=%d  dzr=%d  dzs=%d\n",i,AUSG[i],DZR[i],DZStufe[i]);
#endif
      if (DZR[i] == 1 )
        fprintf(stdout,"%2d;%2d;",AUSG[i],DZStufe[i]);
      else
        fprintf(stdout,"%2d;%2d;",AUSG[i],DZStufe[i]);
        /*  fprintf(fp,"%2d; --;",AUSG[i]); */
    }
    /* A3-A5 */
    for (i=3;i<=5;i++)  /* Ausgangs-Bezeichnungen */
    {
#if DEBUG>3
      fprintf(stderr,"A%2d=%2d\n",i,AUSG[i]);
#endif
      fprintf(stdout,"%2d;",AUSG[i]);
    }
    for (i=6;i<=7;i++)  /* Ausgangs-Bezeichnungen mit Drehzahl */
    {
#if DEBUG>3
      fprintf(stderr,"A%d=%d dzr=%d  dzs=%d\n",i,AUSG[i],DZR[i],DZStufe[i]);
#endif
      if(DZR[i] == 1)
        fprintf(stdout,"%2d;%2d;",AUSG[i],DZStufe[i]);
      else
        fprintf(stdout,"%2d;%2d;",AUSG[i],DZStufe[i]);
        /* fprintf(fp,"%2d; --;",AUSG[i]); */
    }
    for (i=8;i<=13;i++)   /* Ausgangs-Bezeichnungen */
    {
#if DEBUG>3
      fprintf(stderr,"A%2d=%2d\n",i,AUSG[i]);
#endif
      fprintf(stdout,"%2d;",AUSG[i]);
    }
    for (i=1;i<=2;i++)
    {
      if (WMReg[i] == 1)
		if (W_Mwh[i] > 0 )
			fprintf(stdout," %.1f;%.0f%05.1f;",Mlstg[i], W_Mwh[i],W_kwh[i]);
		else
			fprintf(stdout," %.1f;%.0f%.1f;",Mlstg[i], W_Mwh[i],W_kwh[i]);
      else
        fprintf(stdout,"  ---;  ---;");
      }
  }

  if (uvr_typ == UVR61_3) /* UVR61-3 */
  {
    fprintf(stdout,"%2d;%2d;",AUSG[1],DZStufe[1]);
    fprintf(stdout,"%2d;%2d;",AUSG[2],AUSG[3]);

    if (tstbit(akt_daten[15],7) == 0) /* Analogausgang */
    {
      temp_byte = akt_daten[15] & ~(1 << 8); /* oberestes Bit auf 0 setzen */
      fprintf(stdout," %.1f;",(float)temp_byte / 10);
    }
    else
      fprintf(stdout," 0,0;"); /* Analogausgang nicht aktiv */

    if (WMReg[1] == 1)
    {
      fprintf(stdout,"%4d;",akt_daten[18]*0x100 + akt_daten[17]);
      fprintf(stdout," %.1f;%.0f;%.1f;",Mlstg[1], W_Mwh[1],W_kwh[1]);
    }
    else
      fprintf(stdout,"  ---;  ---;  ---;  ---;");
  }

  fprintf(stdout,"\n");
  uvr_typ = temp_uvr_typ;
}


/* Ausgabe der Werte im rrd-Format am Bildschirm */
void write_rrd(int regler)
{
  int i=0;
  int anzSensoren = 16;
  UCHAR temp_byte = 0;
  UCHAR temp_uvr_typ=0;

  temp_uvr_typ = uvr_typ;
  if (regler == 2)  /* 2. Geraet vorhanden */
    uvr_typ = uvr_typ2;
//printf("UVR-Typ: %x\n",uvr_typ);
  switch(uvr_typ)
  {
    case UVR1611: anzSensoren = 16; break; /* UVR1611 */
    case UVR61_3: anzSensoren = 6; break; /* UVR61-3 */
  }

  for(i=1;i<=anzSensoren;i++) /* sensor-Bezeichnungen */
  {
    switch(SENS_Art[i])
    {
      case 0: fprintf(stdout,"0:"); break; /* nicht belegt */
      case 1: fprintf(stdout,"%f:",SENS[i]); break; /* digitaler Eingang */
      case 2:
      case 3: fprintf(stdout,"%.1f:",SENS[i]); break; /* Temp / flow */
      case 6: fprintf(stdout,"%.0f:",SENS[i]); break;
      case 7: fprintf(stdout,"%.1f:",SENS[i]); break;
      case 9: fprintf(stdout,"%1.0f:",SENS[i]); break; /* digitaler Eingang */
      case 10: fprintf(stdout,"%.1f:",SENS[i]); break;
      case 15: fprintf(stdout,"%.1f:",SENS[i]); break;
    }
  }

  if (uvr_typ == UVR1611)
  {
    /* A1/A2 */
    for (i=1;i<=2;i++)  /* Ausgangs-Bezeichnungen mit Drehzahl */
    {
#if DEBUG>3
      fprintf(stderr,"A%d=%d  dzr=%d  dzs=%d\n",i,AUSG[i],DZR[i],DZStufe[i]);
#endif
      if (DZR[i] == 1 )
        fprintf(stdout,"%d:%d:",AUSG[i],DZStufe[i]);
      else
        fprintf(stdout,"%d:%d:",AUSG[i],DZStufe[i]);
        /*  fprintf(fp,"%2d; --;",AUSG[i]); */
    }
    /* A3-A5 */
    for (i=3;i<=5;i++)  /* Ausgangs-Bezeichnungen */
    {
#if DEBUG>3
      fprintf(stderr,"A%2d=%2d\n",i,AUSG[i]);
#endif
      fprintf(stdout,"%d:",AUSG[i]);
    }
    for (i=6;i<=7;i++)  /* Ausgangs-Bezeichnungen mit Drehzahl */
    {
#if DEBUG>3
      fprintf(stderr,"A%d=%d dzr=%d  dzs=%d\n",i,AUSG[i],DZR[i],DZStufe[i]);
#endif
      if(DZR[i] == 1)
        fprintf(stdout,"%d:%d:",AUSG[i],DZStufe[i]);
      else
        fprintf(stdout,"%d:%d:",AUSG[i],DZStufe[i]);
        /* fprintf(fp,"%2d; --;",AUSG[i]); */
    }
    for (i=8;i<=13;i++)   /* Ausgangs-Bezeichnungen */
    {
#if DEBUG>3
      fprintf(stderr,"A%2d=%2d\n",i,AUSG[i]);
#endif
      fprintf(stdout,"%d:",AUSG[i]);
    }
    for (i=1;i<=2;i++)
    {
      if (WMReg[i] == 1)
        fprintf(stdout,"%.1f:%.0f:%.1f",Mlstg[i], W_Mwh[i],W_kwh[i]);
      else
        fprintf(stdout,"0:0");

      if (i==1)
        fprintf(stdout,":");
    }
  }

  if (uvr_typ == UVR61_3) /* UVR61-3 */
  {
    fprintf(stdout,"%d:%d:",AUSG[1],DZStufe[1]);
    fprintf(stdout,"%d:%d:",AUSG[2],AUSG[3]);

    if (tstbit(akt_daten[15],7) == 0) /* Analogausgang */
    {
      temp_byte = akt_daten[15] & ~(1 << 8); /* oberestes Bit auf 0 setzen */
      fprintf(stdout,"%.1f:",(float)temp_byte / 10);
    }
    else
      fprintf(stdout,"0.0:"); /* Analogausgang nicht aktiv */

    if (WMReg[1] == 1)
    {
      fprintf(stdout,"%d:",akt_daten[18]*0x100 + akt_daten[17]);
      fprintf(stdout,"%.1f:%.1f:%.1f:",Mlstg[1], W_Mwh[1],W_kwh[1]);
    }
    else
      fprintf(stdout,"0:0:0:0:0");
  }

  fprintf(stdout,"\n");
  uvr_typ = temp_uvr_typ;
}


/* Ausgabe der Werte als Liste am Bildschirm */
void write_list(int regler)
{
  int i=0;
  int anzSensoren = 16;
  UCHAR temp_byte = 0;
  UCHAR temp_uvr_typ=0;

  temp_uvr_typ = uvr_typ;
  if (regler == 2)  /* 2. Geraet vorhanden */
    uvr_typ = uvr_typ2;
//printf("UVR-Typ: %x\n",uvr_typ);
  switch(uvr_typ)
  {
    case UVR1611: anzSensoren = 16; break; /* UVR1611 */
    case UVR61_3: anzSensoren = 6; break; /* UVR61-3 */
  }

  for(i=1;i<=anzSensoren;i++) /* sensor-Bezeichnungen */
  {
    switch(SENS_Art[i])
    {
      case 0: fprintf(stdout,"0:"); break; /* nicht belegt */
      case 1: fprintf(stdout,"Sensor%d = %f\n",i,SENS[i]); break; /* digitaler Eingang */
      case 2:
      case 3: fprintf(stdout,"Sensor%d = %.1f\n",i,SENS[i]); break; /* Temp / flow */
      case 6: fprintf(stdout,"Sensor%d = %.0f\n",i,SENS[i]); break;
      case 7: fprintf(stdout,"Sensor%d = %.1f\n",i,SENS[i]); break;
      case 9: fprintf(stdout,"Sensor%d = %1.0f\n",i,SENS[i]); break; /* digitaler Eingang */
      case 10: fprintf(stdout,"Sensor%d = %.1f\n",i,SENS[i]); break;
      case 15: fprintf(stdout,"Sensor%d = %.1f\n",i,SENS[i]); break;
    }
  }

  if (uvr_typ == UVR1611)
  {
    /* A1/A2 */
    for (i=1;i<=2;i++)  /* Ausgangs-Bezeichnungen mit Drehzahl */
    {
#if DEBUG>3
      fprintf(stderr,"A%d=%d  dzr=%d  dzs=%d\n",i,AUSG[i],DZR[i],DZStufe[i]);
#endif
      if (DZR[i] == 1 )
	  {
        fprintf(stdout,"Ausgang%d = %d\n",i,AUSG[i]);
        fprintf(stdout,"Drehzahlstufe%d = %d\n",i,DZStufe[i]);
	  }
      else
	  {
        fprintf(stdout,"Ausgang%d = %d\n",i,AUSG[i]);
        fprintf(stdout,"Drehzahlstufe%d = %d\n",i,DZStufe[i]);
	  }
//        fprintf(stdout,"%d:%d:",AUSG[i],DZStufe[i]);
        /*  fprintf(fp,"%2d; --;",AUSG[i]); */
    }
    /* A3-A5 */
    for (i=3;i<=5;i++)  /* Ausgangs-Bezeichnungen */
    {
#if DEBUG>3
      fprintf(stderr,"A%2d=%2d\n",i,AUSG[i]);
#endif
      fprintf(stdout,"Ausgang%d = %d\n",i,AUSG[i]);
    }
    for (i=6;i<=7;i++)  /* Ausgangs-Bezeichnungen mit Drehzahl */
    {
#if DEBUG>3
      fprintf(stderr,"A%d=%d dzr=%d  dzs=%d\n",i,AUSG[i],DZR[i],DZStufe[i]);
#endif
      if (DZR[i] == 1 )
	  {
        fprintf(stdout,"Ausgang%d = %d\n",i,AUSG[i]);
        fprintf(stdout,"Drehzahlstufe%d = %d\n",i,DZStufe[i]);
	  }
      else
	  {
        fprintf(stdout,"Ausgang%d = %d\n",i,AUSG[i]);
        fprintf(stdout,"Drehzahlstufe%d = %d\n",i,DZStufe[i]);
	  }
    }
    for (i=8;i<=13;i++)   /* Ausgangs-Bezeichnungen */
    {
#if DEBUG>3
      fprintf(stderr,"A%2d=%2d\n",i,AUSG[i]);
#endif
      fprintf(stdout,"Ausgang%d = %d\n",i,AUSG[i]);
    }
    for (i=1;i<=2;i++)
    {
      if (WMReg[i] == 1)
	  {
        fprintf(stdout,"Momentanleistung%d = %.1f\n",i,Mlstg[i]);
        fprintf(stdout,"Leistung%d (MW) = %.0f\n",i, W_Mwh[i]);
        fprintf(stdout,"Leistung%d (kW) = %.1f\n",i,W_kwh[i]);
	  }
      else
	  {
        fprintf(stdout,"Momentanleistung%d = %.1f\n",i,Mlstg[i]);
        fprintf(stdout,"Leistung%d (MW) = %.0f\n",i, W_Mwh[i]);
        fprintf(stdout,"Leistung%d (kW) = %.1f\n",i,W_kwh[i]);
	  }

      if (i==1)
	  {
        fprintf(stdout,"Momentanleistung%d = %.1f\n",i+1,Mlstg[i]);
        fprintf(stdout,"Leistung%d (MW) = %.0f\n",i+1, W_Mwh[i]);
        fprintf(stdout,"Leistung%d (kW) = %.1f\n",i+1,W_kwh[i]);
	  }
    }
  }

  if (uvr_typ == UVR61_3) /* UVR61-3 */
  {
    fprintf(stdout,"%d:%d:",AUSG[1],DZStufe[1]);
    fprintf(stdout,"%d:%d:",AUSG[2],AUSG[3]);

    if (tstbit(akt_daten[15],7) == 0) /* Analogausgang */
    {
      temp_byte = akt_daten[15] & ~(1 << 8); /* oberestes Bit auf 0 setzen */
      fprintf(stdout,"%.1f:",(float)temp_byte / 10);
    }
    else
      fprintf(stdout,"0.0:"); /* Analogausgang nicht aktiv */

    if (WMReg[1] == 1)
    {
      fprintf(stdout,"%d:",akt_daten[18]*0x100 + akt_daten[17]);
      fprintf(stdout,"%.1f:%.1f:%.1f:",Mlstg[1], W_Mwh[1],W_kwh[1]);
    }
    else
      fprintf(stdout,"0:0:0:0:0");
  }

  fprintf(stdout,"\n");
  uvr_typ = temp_uvr_typ;
}

/* Bildschirm wieder zuruecksetzen */
void reset_bildschirm(WINDOW *fenster)
{
  curs_set(1);
  wclear(fenster);
  wrefresh(fenster);
}

/* Ausgabe per ncurses "Keine neuen Daten" */
void keine_neuen_daten(WINDOW *fenster)
{
  wclear(fenster);
  mvwprintw(fenster,12,25,"Es liegen keine neuen Daten vor.");
  wrefresh(fenster);
}

/* Setzen das Bildschirm-Attribut je nach Zeile */
void set_attribut(int zaehler, WINDOW *fenster)
{
  if (bool_farbe)
    {
      if (zaehler%2 == 0)
  wattrset(fenster,A_NORMAL | COLOR_PAIR(6));
      else
  wattrset(fenster,A_BOLD | COLOR_PAIR(2));
    }
  else
    {
      if (zaehler%2 == 0)
  wattrset(fenster,A_NORMAL);
      else
  wattrset(fenster,A_BOLD);
    }
}

/* Pruefen, ob Farben unterstuetzt werden. Wenn ja, dann aktivieren */
/* Definition von Farbpaaren (Schriftfarbe, Hintergrundfarbe */
void test_farbe(void)
{
  if(has_colors()==TRUE)
    {
      start_color();
      bool_farbe = TRUE;
      printf("Farben moeglich.\n");
    }
  else
    {
      bool_farbe = FALSE;
      printf("Keine Farben da. :-(\n");
    }

  if (bool_farbe == TRUE)
    {
      init_pair(1,COLOR_RED,COLOR_BLACK);
      init_pair(2,COLOR_GREEN,COLOR_BLACK);
      init_pair(3,COLOR_BLUE,COLOR_BLACK);
      init_pair(4,COLOR_YELLOW,COLOR_BLACK);
      init_pair(5,COLOR_MAGENTA,COLOR_BLACK);
      init_pair(6,COLOR_CYAN,COLOR_BLACK);
      init_pair(7,COLOR_RED,COLOR_CYAN);
      init_pair(8,COLOR_WHITE,COLOR_CYAN);
      init_pair(10,COLOR_YELLOW,COLOR_CYAN);
      init_pair(11,COLOR_BLACK,COLOR_CYAN);
      init_pair(12,COLOR_BLACK,COLOR_BLACK);
    }
}

/* Liest aus /etc/dl-aktuelle-datenx.conf die Beschriftung */
int lies_conf(void)
{
  FILE *fp_confFile;
  fp_confFile=NULL;

  char confFile[] = "/etc/dl-aktuelle-datenx.conf";
  char line[81], *p_line, *zielstring;
  char *such_farbe = "COLOR", *such_farbe_on = "ON";
  char *such_DZR1 = "DZR1", *such_DZR2 = "DZR2", *such_DZR6 = "DZR6", *such_DZR7 = "DZR7";
  char *such_DZS1 = "DZS1", *such_DZS2 = "DZS2", *such_DZS6 = "DZS6", *such_DZS7 = "DZS7";

  char * Ausgang_conf = "A";
  char * Sensor_conf = "S";

  int i, z, x;

  i=0;
  p_line = line;

  if ((fp_confFile=fopen(confFile,"r")) == NULL)
    {
      fprintf(stderr,"Kann %s nicht oeffnen.\n",confFile);
      return -1;
    }
#if DEBUG>2
  else
    fprintf(stderr," Config file %s wird eingelesen.\n",confFile);
#endif

  x=1; /* Zaehler fuer die Ausgaenge  */
  z=1; /* Zaehler fuer die Sensoren  */
  while (!feof(fp_confFile))
  {
    fgets(line,80,fp_confFile);

    if ((strncmp(line,"#",1) != 0) && (strlen(line) > 3 ))
    {
    /* was ist mit doppelten Zeilen im config!?! */
    /* blank is separator */
      zielstring=strchr(line,' ');
      if ( !zielstring )
        continue;

      zielstring[0]='\000';
      zielstring++;

      char *temp01 = "unsichtbar";  /* Zeile leer schalten */
      if(  strcmp(zielstring,temp01) > 0)
//        fprintf(stderr,"%s:  %s \n",line, zielstring);
        zielstring = "               ";

    /* find end of string */
      char  * blank = "\n";
      char * end_string = strstr(zielstring+1,blank);
      if( end_string != NULL)
        end_string[0]='\000';
      int length = strlen(zielstring);

      if (length <0)
        continue;

      char * tmpstr = (char *) malloc(length + 1);
      //bzero(tmpstr,length+1);
      memset( tmpstr, 0, length+1 );
      strncpy(tmpstr,zielstring,length+1);

#if DEBUG>2
      fprintf(stderr,"%s:  %s length:%d \n",line, zielstring,length);
#endif
      if (strncmp(line,such_farbe,5) == 0 ) /* Farbdarstellung ja/nein */
      {
        //      printf("Jetzt in Farbetest. Zeile: %s\n",zielstring);
        if (strncmp(zielstring,such_farbe_on,2) == 0)
          bool_farbe = TRUE;
        else
          bool_farbe = FALSE;

        if(tmpstr)
          free( tmpstr );
      }
      if (strncmp(line,such_DZR1,4) == 0) pBez_DZR[1]= tmpstr;
      if (strncmp(line,such_DZR2,4) == 0) pBez_DZR[2]= tmpstr;
      if (strncmp(line,such_DZR6,4) == 0) pBez_DZR[6]= tmpstr;
      if (strncmp(line,such_DZR7,4) == 0) pBez_DZR[7]= tmpstr;
      if (strncmp(line,such_DZS1,4) == 0) pBez_DZS[1]=tmpstr;
      if (strncmp(line,such_DZS2,4) == 0) pBez_DZS[2]=tmpstr;
      if (strncmp(line,such_DZS6,4) == 0) pBez_DZS[6]=tmpstr;
      if (strncmp(line,such_DZS7,4) == 0) pBez_DZS[7]=tmpstr;

      char number[3];
      strncpy(number,line+1,2);
      int jj=atoi(number);
#if DEBUG>2
      fprintf(stderr," %s:  index:%d \n",line, jj);
#endif
      if( strncmp(line,Ausgang_conf,1) == 0 )
      {
        if (jj >13)
          fprintf(stderr," Ausgang %d nicht existent! %s\n",jj,line);
        else
        {
          if ( jj != x )
            fprintf(stderr," als Ausgang%d gezaehlt ist  aber Ausgang%d: %s\n", x, jj,line);
          else
          {
            pBez_A[jj]= tmpstr;
            x++;
          }
        }
      }
      else if ( strncmp(line,Sensor_conf,1) == 0)
      {
        if (jj >16)
          fprintf(stderr," Sensor %d nicht existent! %s\n",jj,line);
        else
        {
          if ( jj != z )
            fprintf(stderr," als Sensor%d gezaehlt ist aber Sensor%d: %s\n", x,jj,line);
          else
          {
            pBez_S[jj]= tmpstr;
            z++;
          }
        }
      }
    }
  }
  i=1;

  if (fp_confFile)
    fclose(fp_confFile);
#if DEBUG>2
  fprintf(stderr," Config File %s gelesen.\n", confFile);
#endif
  return i;
}

/* Modulmoduskennung abfragen */
int get_modulmodus(void)
{
  int result;
  UCHAR sendbuf[1];       /*  sendebuffer fuer die Request-Commandos*/
  UCHAR empfbuf[1];
//  int sendbuf[1];       /*  sendebuffer fuer die Request-Commandos*/
//  int empfbuf[1];

  sendbuf[0]=VERSIONSABFRAGE;    /* Senden der Kopfsatz-abfrage */

/* ab hier unterscheiden nach USB und IP */
  if (usb_zugriff)
  {
    write_erg=write(fd,sendbuf,1);
    if (write_erg == 1)    /* Lesen der Antwort*/
      result=read(fd,empfbuf,1);
  }
  if (ip_zugriff)
  {
    write_erg=send(sock,sendbuf,1,0);
     if ( write_erg == 1)    /* Lesen der Antwort */
      result  = recv(sock,empfbuf,1,0);
  }

  return empfbuf[0];
}

