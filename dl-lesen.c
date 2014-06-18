/*****************************************************************************
 * Daten der UVR1611 lesen vom Datenlogger und im Winsol-Format speichern    *
 * read data of UVR1611 from Datanlogger and save it in format of Winsol     *
 * (c) 2006 - 2010 H. Roemer / C. Dolainsky  / S. Lechner                    *
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
 * Version 0.1      18.04.2006 erste Testversion                             *
 * Version 0.5a     05.10.2006 Protokoll-Log in /var/dl-lesenx.log speichern *
 * Version 0.6      27.01.2006 C. Dolainsky                                  *
 *                  28.01.2006 Anpassung an geaenderte dl-lesen.h.           *
 *                  18.03.2007 IP                                            *
 * Version 0.7      01.04.2007                                               *
 * Version 0.7.7    26.12.2007 UVR61-3                                       *
 * Version 0.8      13.01.2008 2DL-Modus                                     *
 * Version 0.8.1    04.12.2009 --dir Parameter aufgenommen                   *
 * Version 0.9.0    11.01.2011 CAN-Logging                                   *
 * Version 0.9.3      .06.2012 Test CAN BC                                   *
 * Version 0.9.4    05.01.2013  Anpassung CAN-Logging                        *
 * Version 0.9.5    24.02.2013  Ueberarbeitung USB-Zugriff                   *
 *                  $Id$       *
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
#include <time.h>
#include <getopt.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "dl-lesen.h"

//#define DEBUG 4

#define BAUDRATE B115200
#define UVR61_3 0x5A
#define UVR1611 0x76
/* CAN-BC */
#define CAN_BC 0x78


extern char *optarg;
extern int optind, opterr, optopt;
int can_typ[8];
int start_socket(void);
void close_usb(void);
int do_cleanup(void);
int check_arg(int arg_c, char *arg_v[]);
int check_arg_getopt(int arg_c, char *arg_v[]);
int erzeugeLogfileName(UCHAR ds_monat, UCHAR ds_jahr);
int erzeugeLogfileName_CAN(UCHAR ds_monat, UCHAR ds_jahr, int anzahl_Rahmen);
int create_LogDir(char DirName[]);
int open_logfile(char LogFile[], int geraet);
int open_logfile_CAN(char LogFile[], int datenrahmen, int can_typ[8]);
int close_logfile(void);
int get_modulmodus(void);
int get_modultyp(void);
int kopfsatzlesen(void);
void init_usb(void);
void testfunktion(void);
int copy_UVR2winsol_1611(u_DS_UVR1611_UVR61_3 *dsatz_uvr1611, DS_Winsol *dsatz_winsol );
int copy_UVR2winsol_1611_CAN(s_DS_CAN *dsatz_uvr1611, DS_Winsol  *dsatz_winsol );
int copy_UVR2winsol_1611_CANBC(s_DS_CANBC *dsatz_uvr1611_canbc, DS_CANBC  *dsatz_winsol_canbc );
int copy_UVR2winsol_61_3(u_DS_UVR1611_UVR61_3 *dsatz_uvr61_3, DS_Winsol_UVR61_3 *dsatz_winsol_uvr61_3 );
int copy_UVR2winsol_D1_1611(u_modus_D1 *dsatz_modus_d1, DS_Winsol *dsatz_winsol , int geraet);
int copy_UVR2winsol_D1_61_3(u_modus_D1 *dsatz_modus_d1, DS_Winsol_UVR61_3 *dsatz_winsol_uvr61_3, int geraet);
int datenlesen_A8(int anz_datensaetze);
int datenlesen_D1(int anz_datensaetze);
int datenlesen_DC(int anz_datensaetze);
int berechneKopfpruefziffer_D1(KopfsatzD1 derKopf[] );
int berechneKopfpruefziffer_A8(KopfsatzA8 derKopf[] );
int berechneKopfpruefziffer_DC(KOPFSATZ_DC derKopf[] );
int berechnepruefziffer_uvr1611(u_DS_UVR1611_UVR61_3 ds_uvr1611[]);
int berechnepruefziffer_uvr61_3(u_DS_UVR1611_UVR61_3 ds_uvr61_3[]);
int berechnepruefziffer_uvr1611_CAN(u_DS_CAN ds_uvr1611[], int anzahl_can_rahmen);
int berechnepruefziffer_modus_D1(u_modus_D1 ds_modus_D1[], int anzahl);
int anzahldatensaetze_D1(KopfsatzD1 kopf[]);
int anzahldatensaetze_A8(KopfsatzA8 kopf[]);
int anzahldatensaetze_DC(KOPFSATZ_DC kopf[]);
int reset_datenpuffer_usb(int do_reset );
int reset_datenpuffer_ip(int do_reset );
void zeitstempel(void);
float berechnetemp(UCHAR lowbyte, UCHAR highbyte);
float berechnevol(UCHAR lowbyte, UCHAR highbyte);
int clrbit( int word, int bit );
int tstbit( int word, int bit );
int xorbit( int word, int bit );
int setbit( int word, int bit );
int ip_handling(int sock);

int csv = 0;    /* Speichern im csv-Format (csv = 1) oder winsol-Format (csv = 0) */
int reset = 0;    /* Ruecksetzen des DL nach Lesen der Daten 1=>true, 0=>false */
int fd,  write_erg; /* anz_datensaetze; */
struct sockaddr_in SERVER_sockaddr_in;

int csv_header_done=-1;

FILE *fp_logfile=NULL, *fp_logfile_2=NULL,  *fp_logfile_3=NULL, *fp_logfile_4=NULL,
     *fp_logfile_5=NULL, *fp_logfile_6=NULL,  *fp_logfile_7=NULL, *fp_logfile_8=NULL,
     *fp_varlogfile=NULL, *fp_csvfile=NULL ; /* pointer IMMER initialisieren und vor benutzung pruefen */

char dlport[13]; /* Uebergebener Parameter USB-Port */
char LogFileName[8][255];
char varLogFile[22];
char DirName[241];
char sDatum[11], sZeit[11];
u_DS_UVR1611_UVR61_3 structDLdatensatz;

struct termios oldtio; /* will be used to save old port settings */

int ip_zugriff, ip_first;
int usb_zugriff;
UCHAR uvr_modus, uvr_typ, uvr_typ2;  /* uvr_typ2 -> 2. Geraet bei 2DL */
UCHAR *start_adresse=NULL, *end_adresse=NULL;
KopfsatzD1 kopf_D1[1];
KopfsatzA8 kopf_A8[1];
KOPFSATZ_DC kopf_DC[1];
int sock;

char Version[]="Version 0.9.5 vom 27.02.2013";

int main(int argc, char *argv[])
{
  int i, sr=0, anz_ds=0, erg_check_arg, erg=0; //i_varLogFile, 
  char *pvarLogFile;

  ip_zugriff = 0;
  usb_zugriff = 0;
  ip_first = 1;

  strcpy(DirName,"./");
  erg_check_arg = check_arg_getopt(argc, argv);

#if  DEBUG>1
  fprintf(stderr, "Ergebnis vom Argumente-Check %d\n",erg_check_arg);
  fprintf(stderr, "angegebener Port: %s Variablen: reset = %d csv = %d \n",dlport,reset,csv);
#endif

  if ( erg_check_arg != 1 )
    exit(-1);

  /* LogFile zum Speichern von Ausgaben aus diesem Programm hier */
  pvarLogFile = &varLogFile[0];
  sprintf(pvarLogFile,"./dl-lesen.log");

  if ((fp_varlogfile=fopen(varLogFile,"a")) == NULL)
    {
      printf("Log-Datei %s konnte nicht erstellt werden\n",varLogFile);
//      i_varLogFile = -1;
      fp_varlogfile=NULL;
    }
//  else
//    i_varLogFile = 1;

  if ( csv == 1 && (fp_csvfile=fopen("alldata.csv","a")) == NULL )
  {
    printf("csv file %s konnte nicht erstellt werden\n",varLogFile);

    int want_to_continue_even_if_csv_file_failed=1;
    if (want_to_continue_even_if_csv_file_failed)
      printf("  trotzdem wird versucht ein logfile zu schreiben\n!");
    else
    {
      do_cleanup();
      return -1;
    }
  }

  /* IP-Zugriff  - IP-Adresse und Port sind manuell gesetzt!!! */
  if (ip_zugriff && !usb_zugriff)
  {
        sr = start_socket();
        if (sr > 1)
        {
           return sr;
        }
		else
           uvr_modus = get_modulmodus(); /* Welcher Modus
                      0xA8 (1DL) / 0xD1 (2DL) / 0xDC (CAN) */
  } /* Ende IP-Zugriff */
  else  if (usb_zugriff && !ip_zugriff)
  {
    init_usb();
    uvr_modus = get_modulmodus(); /* Welcher Modus
                      0xA8 (1DL) / 0xD1 (2DL) / 0xDC (CAN) */
    close_usb();
  }
  else
  {
    fprintf(stderr, " da lief was falsch ....\n %s \n", argv[0]);
    do_cleanup();
  }

  switch (uvr_modus)
  {
        case 0xDC: fprintf(stderr, " CAN-Logging erkannt.\n"); break;
        case 0xA8: fprintf(stderr, " 1DL-Logging erkannt.\n"); break;
        case 0xD1: fprintf(stderr, " 2DL-Logging erkannt.\n"); break;
        default:   fprintf(stderr, " Kein Logging erkannt!\n Abbruch!\n");
                   do_cleanup();
                   return ( -1 );
  }

//  Firmware BL-Net:
//  fprintf(stderr, "Firmware: %d \n",get_modultyp());

  /* ************************************************************************   */
  /* Lesen des Kopfsatzes zur Ermittlung der Anzahl der zu lesenden Datensaetze */
  i=1;
  do                        /* max. 5 durchlgaenge */
  {
#ifdef DEBUG
      fprintf(stderr, "\n Kopfsatzlesen - Versuch%d\n",i);
#endif
      anz_ds = kopfsatzlesen();
      i++;
  }
  while((anz_ds == -1) && (i < 6));
  i=1;
  
  while ((anz_ds == -3) && (uvr_modus == 0xDC) && (i < 3))
  {
     if (ip_zugriff && !usb_zugriff)
     {	 
        if ( shutdown(sock,SHUT_RDWR) == -1 ) /* IP-Socket schliessen */
        {
            zeitstempel();
            fprintf(stderr, "\n %s Fehler beim Schliessen der IP-Verbindung!\n", sZeit);
        }
        sleep(3);
        sr = start_socket();
        if (sr > 1)
        {
                return sr;
        }
        uvr_modus = get_modulmodus();
        switch (uvr_modus)
        {
           case 0xDC: fprintf(stderr, " CAN-Logging erkannt.\n"); break;
           case 0xA8: fprintf(stderr, " 1DL-Logging erkannt.\n"); break;
           case 0xD1: fprintf(stderr, " 2DL-Logging erkannt.\n"); break;
           default:   fprintf(stderr, " Kein Logging erkannt!\n Abbruch!\n");
                      do_cleanup();
                      return ( -1 );
        }
        anz_ds = kopfsatzlesen();
        i++;
	 }
	 
	 if (usb_zugriff && !ip_zugriff)
	 {
	    close_usb();
        sleep(3);
		init_usb();
        uvr_modus = get_modulmodus();
        switch (uvr_modus)
        {
           case 0xDC: fprintf(stderr, " CAN-Logging erkannt.\n"); break;
           case 0xA8: fprintf(stderr, " 1DL-Logging erkannt.\n"); break;
           case 0xD1: fprintf(stderr, " 2DL-Logging erkannt.\n"); break;
           default:   fprintf(stderr, " Kein Logging erkannt!\n Abbruch!\n");
                      do_cleanup();
                      return ( -1 );
        }
        anz_ds = kopfsatzlesen();
        i++;
	 }
  }

  switch(anz_ds)
  {
    case -1: printf(" Kopfsatzlesen fehlgeschlagen\n");
             do_cleanup();
             return ( -1 );
    case -2: printf(" Keine Daten vorhanden\n");
             do_cleanup();
             return ( -1 );
    case -3: printf(" CAN-Logging: BL-Net nicht bereit!\n");
             do_cleanup();
             return ( -1 );
  }

  printf(" Kopfsatzlesen erfolgreich!\n\n");
  if (uvr_typ == UVR1611)
    printf(" UVR Typ: UVR1611\n");
  if (uvr_typ == UVR61_3)
    printf(" UVR Typ: UVR61-3\n");

  zeitstempel();
  fprintf(fp_varlogfile,"%s - %s -- %d Datensaetze im D-LOGG\n\n",sDatum, sZeit,anz_ds);

  /* ************************************************************************ */
  /* Daten aus dem D-LOGG lesen und in das Log-File schreiben                 */
  /* falls csv gesetzt ist auch Daten in allcsv.csv dumpen                    */
  switch(uvr_modus)
  {
    case 0xA8: erg = datenlesen_A8(anz_ds); break;
    case 0xD1: erg = datenlesen_D1(anz_ds); break;
    case 0xDC: erg = datenlesen_DC(anz_ds); break;
  }

  printf("\n%d Datensaetze insgesamt geschrieben.\n",erg);
  zeitstempel();
  fprintf(fp_varlogfile,"%s - %s -- Es wurden %d Datensaetze geschrieben.\n",sDatum, sZeit,erg);

  /* ************************************************************************ */
  /* Datenspeicher im D-LOGG zuruecksetzen falls res flag gesetzt */
  if (usb_zugriff && !ip_zugriff)
    reset_datenpuffer_usb( reset );
  else if (ip_zugriff && !usb_zugriff)
    reset_datenpuffer_ip( reset );
  /* ************************************************************************ */

  /* restore the old port settings before quitting */
  //tcsetattr(fd,TCSANOW,&oldtio);

  int retval=0;
  if ( close_logfile() == -1)
  {
    printf("Cannot close logfile!\n");
    retval=-1;
  }

  if ( do_cleanup() < 0 )
    retval=-1;

  return (retval);
}


/* Init USB-Zugriff */
void init_usb(void)
{
  struct termios newtio;  /* will be used for new port settings */

    /************************************************************************/
    /*  open the serial port for reading and writing */
    fd = open(dlport, O_RDWR | O_NOCTTY); // | O_NDELAY);
    if (fd < 0)
    {
      zeitstempel();
      if (fp_varlogfile)
        fprintf(fp_varlogfile,"%s - %s -- kann nicht auf port %s zugreifen \n",sDatum, sZeit,dlport);
        perror(dlport);
        do_cleanup();
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
int start_socket(void)
{
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
      perror("socket failed()");
      do_cleanup();
      return 2;
    }

    if (connect(sock, (const struct sockaddr *)&SERVER_sockaddr_in, sizeof(SERVER_sockaddr_in)) == -1)
    {
      perror("connect failed()");
      do_cleanup();
      return 3;
    }

    if (ip_handling(sock) == -1)
    {
      fprintf(stderr, "Fehler im Initialisieren der IP-Kommunikation!\n");
      do_cleanup();
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
int do_cleanup(void)
{
  int retval=0;
  close_usb();

  if (ip_zugriff)
    close(sock); /* IP-Socket schliessen */

  if (csv==1 && fp_csvfile )
    {
      if ( fclose(fp_csvfile) != 0 )
          {
                printf("Cannot close csvfile %s!\n",varLogFile);
                retval=-1;
      }
      else
                fp_csvfile=NULL;
    }

  if (fp_varlogfile != NULL)
  {
    if ( fclose(fp_varlogfile) != 0 )
        {
                printf("Cannot close %s!\n",varLogFile);
                retval=-1;
        }
    else
                fp_varlogfile=NULL;
  }
  return retval;
}

/* Hilfetext */
static int print_usage()
{

  fprintf(stderr,"\ndl-lesenx (-p USB-Port | -i IP:Port)  [--csv] [--res] [--dir] [-h] [-v]\n");
  fprintf(stderr,"    -p USB-Port -> Angabe des USB-Portes,\n");
  fprintf(stderr,"                   an dem der D-LOGG angeschlossen ist.\n");
  fprintf(stderr,"    -i IP:Port  -> Angabe der IP-Adresse / Hostname und des Ports,\n");
  fprintf(stderr,"                   an dem der BL-Net angeschlossen ist.\n");
  fprintf(stderr,"                   Ohne Port-Angabe wird der Default-Port 40000 verwendet.\n");
  fprintf(stderr,"          --csv -> im CSV-Format speichern (wird noch nicht unterstuetzt)\n");
  fprintf(stderr,"                   Standard: ist WINSOL-Format\n");
  fprintf(stderr,"          --res -> nach dem Lesen Ruecksetzen des DL\n");
  fprintf(stderr,"                   Standard: KEIN Ruecksetzen des DL nach dem Lesen\n");
  fprintf(stderr,"          --dir -> Verzeichnis in dem die Datei angelegt werden soll\n");
  fprintf(stderr,"                   das Verzeichnis wird erzeugt, wenn nicht vorhanden \n");
  fprintf(stderr,"                   (bei gueltigen Rechten)\n");
  fprintf(stderr,"          -h    -> diesen Hilfetext\n");
  fprintf(stderr,"          -v    -> Versionsangabe\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"Beispiel: dl-lesenx -p /dev/ttyUSB0 --res\n");
  fprintf(stderr,"              Liest die Daten vom USB-Port 0 und setzt den D-LOGG zurueck.\n");
  fprintf(stderr,"          dl-lesenx -i blnetz:40000 --res\n");
  fprintf(stderr,"              Liest die Daten vom Host blnetz und setzt den BL-Net zurueck.\n");
  fprintf(stderr,"          dl-lesenx -i 192.168.0.10:40000 \n");
  fprintf(stderr,"              Liest die Daten von der IP-Adresse 192.168.0.10\n");
  fprintf(stderr,"              und setzt den BL-Net nicht zurueck.\n");
  fprintf(stderr,"\n");
  return 0;

}

/* Auswertung der uebergebenen Argumente */
int check_arg_getopt(int arg_c, char *arg_v[])
{
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
      {"res", 0, 0, 0},
      {"dir", 1, 0, 0},
      {0, 0, 0, 0}
    };

    c = getopt_long(arg_c, arg_v, "hvp:i:", long_options, &option_index);
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
        printf("\n    UVR1611/UVR61-3 Daten lesen vom D-LOGG USB / BL-Net \n");
        printf("    %s \n",Version);
        printf("    $Id$ \n");
        return 0;
      }
      case 'h':
      case '?':
        print_usage();
        return -1;
      case 'p':
      {
        if ( strlen(optarg) < 13)
        {
          strncpy(dlport, optarg , strlen(optarg));
          printf("\n port gesetzt: %s\n", dlport);
          p_is_set=1;
          usb_zugriff = 1;
        }
        else
        {
          printf(" portname zu lang: %s\n",optarg);
          print_usage();
          return -1;
        }
        break;
      }
      case 'i':
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
      case 0:
      {
        if (  strncmp( long_options[option_index].name, "csv", 3) == 0 ) /* csv-Format ist gewuenscht */
        {
          csv = 0;
          printf(" Zur Zeit keine csv-Ausgabe!\n");
      //    csv = 1;
     //     printf(" zusaetzlich Ausgabe in csv file\n");
     //  01.2008 - vorerst wieder deaktiviert, muss ueberarbeitet werden
        }
        if (  strncmp( long_options[option_index].name, "res", 3) == 0 )
        {
          reset = 1;  /* Ruecksetzen DL nach Daten lesen gewuenscht */
          printf(" mit  reset des Logger-Speichers! \n");
        }
        if (  strncmp( long_options[option_index].name, "dir", 3) == 0 ) /* Verzeichnis */
        {
          if (strlen(optarg) < (sizeof(DirName)-1)) {
            strcpy(DirName,optarg);
            if (DirName[strlen(DirName)] != '/') {
              strcat(DirName,"/");
            }
          }
          else {
          printf(" Verzeichnis ist zu lang\n");
          }
        }
        break;
      }
      default:
        printf ("?? input mit character code 0%o ??\n", c);
        printf("Falsche Parameterangebe!\n");
        print_usage();
        return( -1 );
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
      printf ("non-option ARGV-elements: ");
      while (optind < arg_c)
  printf ("%s ", arg_v[optind++]);
      printf ("\n");
      return -1;
    }

  return 1;
}


/* Der Name des Logfiles wird aus dem eigelesenen Wert fuer Jahr und Monat erzeugt */
int erzeugeLogfileName(UCHAR ds_monat, UCHAR ds_jahr)
{
  int erg = 0;
  char csv_endung[] = ".csv", winsol_endung[] = ".log", temp_DirName_2[241], temp_DirName_1[241];
  char *pLogFileName=NULL, *pLogFileName_2=NULL;
  pLogFileName = LogFileName[1];
  pLogFileName_2 = LogFileName[2];
  struct stat dir_attrib;

  strcpy(temp_DirName_1,DirName);
  strcpy(temp_DirName_2,DirName);

  if (strlen(temp_DirName_1) > 2 )
  {
    if (stat(temp_DirName_1, &dir_attrib) == -1)  /* das Verz. existiert nicht */
        {
                if ( mkdir(temp_DirName_1, 0777) == -1 )
                {
                        fprintf(stderr,"%s konnte nicht angelegt werden!\n",temp_DirName_1);
                        return erg;
                }
        }
  }

  if (uvr_modus == 0xD1)
  {
    strcat(temp_DirName_1,"Log1/");
    if (stat(temp_DirName_1, &dir_attrib) == -1)  /* das Verz. existiert nicht */
        {
                if ( mkdir(temp_DirName_1, 0777) == -1 )
                {
                        fprintf(stderr,"%s konnte nicht angelegt werden!\n",temp_DirName_1);
                        return erg;
                }
        }
  }

  if (csv ==  1) /* LogDatei im CSV-Format schreiben */
    {
      erg=sprintf(pLogFileName,"%s2%03d%02d%s",temp_DirName_1,ds_jahr,ds_monat,csv_endung);
      if (uvr_modus == 0xD1)
          {
                strcat(temp_DirName_2,"Log2/");
                if (stat(temp_DirName_2, &dir_attrib) == -1)  /* das Verz. existiert nicht */
                {
                        if ( mkdir(temp_DirName_2, 0777) == -1 )
                        {
                                fprintf(stderr,"%s konnte nicht angelegt werden!\n",temp_DirName_2);
                                erg = 0;
                                return erg;
                        }
                }
        erg=sprintf(pLogFileName_2,"%s2%03d%02d%s",temp_DirName_2,ds_jahr,ds_monat,csv_endung);
          }
    }
  else  /* LogDatei im Winsol-Format schreiben */
    {
      erg=sprintf(pLogFileName,"%sY2%03d%02d%s",temp_DirName_1,ds_jahr,ds_monat,winsol_endung);
      if (uvr_modus == 0xD1)
          {
                strcat(temp_DirName_2,"Log2/");
                if (stat(temp_DirName_2, &dir_attrib) == -1)  /* das Verz. existiert nicht */
                {
                        if ( mkdir(temp_DirName_2, 0777) == -1 )
                        {
                                fprintf(stderr,"%s konnte nicht angelegt werden!\n",temp_DirName_2);
                                erg = 0;
                                return erg;
                        }
                }
        erg=sprintf(pLogFileName_2,"%sY2%03d%02d%s",temp_DirName_2,ds_jahr,ds_monat,winsol_endung);
          }
    }

  return erg;
}

/* Der Name der Logfiles wird aus dem eigelesenen Wert fuer Jahr und Monat erzeugt */
int erzeugeLogfileName_CAN(UCHAR ds_monat, UCHAR ds_jahr, int anzahl_Rahmen)
{
  int erg = 0, i = 0;
  char csv_endung[] = ".csv", winsol_endung[] = ".log";
  char temp_DirName[9][241], temp_Log[9][6];
  char *pLogFileName[9];
  pLogFileName[1] = LogFileName[1];
  pLogFileName[2] = LogFileName[2];
  pLogFileName[3] = LogFileName[3];
  pLogFileName[4] = LogFileName[4];
  pLogFileName[5] = LogFileName[5];
  pLogFileName[6] = LogFileName[6];
  pLogFileName[7] = LogFileName[7];
  pLogFileName[8] = LogFileName[8];

  if ( anzahl_Rahmen == 1)
    strcpy(temp_Log[1],"Log/");
  else
    strcpy(temp_Log[1],"Log1/");

  strcpy(temp_Log[2],"Log2/");
  strcpy(temp_Log[3],"Log3/");
  strcpy(temp_Log[4],"Log4/");
  strcpy(temp_Log[5],"Log5/");
  strcpy(temp_Log[6],"Log6/");
  strcpy(temp_Log[7],"Log7/");
  strcpy(temp_Log[8],"Log8/");

  strcpy(temp_DirName[1],DirName);
  strcpy(temp_DirName[2],DirName);
  strcpy(temp_DirName[3],DirName);
  strcpy(temp_DirName[4],DirName);
  strcpy(temp_DirName[5],DirName);
  strcpy(temp_DirName[6],DirName);
  strcpy(temp_DirName[7],DirName);
  strcpy(temp_DirName[8],DirName);

  if ( create_LogDir(temp_DirName[1]) == 0)
        return 0;

  for (i=1;i<=anzahl_Rahmen;i++)
  {
    strcat(temp_DirName[i],temp_Log[i]);
    if ( create_LogDir(temp_DirName[i]) == 0)
                return 0;

        if (csv ==  1)
                erg=sprintf(pLogFileName[i],"%s2%03d%02d%s",temp_DirName[i],ds_jahr,ds_monat,csv_endung);
        else
                erg=sprintf(pLogFileName[i],"%sY2%03d%02d%s",temp_DirName[i],ds_jahr,ds_monat,winsol_endung);
  }

  return erg;
}

/* Log-Verzeichnis bei Bedarf erstellen */
int create_LogDir(char DirName[])
{
        struct stat dir_attrib;

        if (stat(DirName, &dir_attrib) == -1)  /* das Verz. existiert nicht */
        {
                if ( mkdir(DirName, 0777) == -1 )
                {
                        fprintf(stderr,"%s konnte nicht angelegt werden!\n",DirName);
                        return 0;
                }
        }
        return 1;
}

/* Logdatei oeffnen / erstellen */
int open_logfile(char LogFile[], int geraet)
{
  int i, tmp_erg = 0;
  i=-1;  /* es ist kein Logfile geoeffnet (Wert -1) */
  /* bei neuer Logdatei der erste Datensatz: */
  UCHAR kopf_winsol_1611[59]={0x01, 0x02, 0x01, 0x03, 0xF0, 0x0F, 0x00, 0x07, 0xAA, 0xAA, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00,
            0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00,
            0xAA, 0x00, 0xFF, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00};
  UCHAR kopf_winsol_61_3[59]={0x01, 0x02, 0x01, 0x03, 0xF0, 0x0F, 0x00, 0x06, 0xAA, 0xAA, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00,
            0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00,
            0xAA, 0x00, 0xFF, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00};

  if ( geraet == 1)
  {
    if ((fp_logfile=fopen(LogFile,"r")) == NULL) /* wenn Logfile noch nicht existiert */
    {
      if ((fp_logfile=fopen(LogFile,"w")) == NULL) /* dann Neuerstellung der Logdatei */
      {
        printf("Log-Datei %s konnte nicht erstellt werden\n",LogFile);
        i = -1;
      }
      else
      {
        i = 0;
        if (csv == 0)
        {
          // Unterscheidung UVR1611 / UVR61-3 !!!!
          if (uvr_typ == UVR1611)
            tmp_erg = fwrite(&kopf_winsol_1611,59,1,fp_logfile);
          if (uvr_typ == UVR61_3)
            tmp_erg = fwrite(&kopf_winsol_61_3,59,1,fp_logfile);
          if ( tmp_erg != 1)
          {
            printf("Kopfsatz konnte nicht geschrieben werden!\n");
            i= -1;
          }
        }
        else
        {
          if (uvr_typ == UVR1611)  /* UVR1611 */
            fprintf(fp_logfile," Datum ; Zeit ; Sens1 ; Sens2 ; Sens3 ; Sens4 ; Sens5 ; Sens6 ; Sens7 ; Sens8 ; Sens9 ; Sens10 ; Sens11 ; Sens12; Sens13 ; Sens14 ; Sens15 ; Sens16 ; Ausg1 ; Drehzst_A1 ; Ausg2 ; Drehzst_A2 ; Ausg3 ; Ausg4 ; Ausg5 ; Ausg6 ; Drehzst_A6 ; Ausg7 ; Drehzst_A7 ; Ausg8 ; Ausg9 ; Ausg10 ; Ausg11 ; Ausg12 ; Ausg13 ; kW1 ; kWh1 ; kW2 ; kWh2 \n");
          if (uvr_typ == UVR61_3)  /* UVR61-3 */
            fprintf(fp_logfile," Datum ; Zeit ; Sens1 ; Sens2 ; Sens3 ; Sens4 ; Sens5 ; Sens6 ; Ausg1 ; Drehzst_A1 ; Ausg2 ; Ausg3 ; Analogausg ; Vol-Strom ; Momentanlstg ; MWh ; kWh \n");
          csv_header_done=1;
        }
      }
    }
    else /* das Logfile existiert schon */
    {
      fclose(fp_logfile);
      if ((fp_logfile=fopen(LogFile,"a")) == NULL) /* schreiben ab Dateiende */
      {
        printf("Log-Datei %s konnte nicht geoeffnet werden\n",LogFile);
        i = -1;
      }
      else
      {
        csv_header_done = 1;
        i = 0;
      }
    }
  }
  else
  {
    if ((fp_logfile_2=fopen(LogFile,"r")) == NULL) /* wenn Logfile noch nicht existiert */
    {
      if ((fp_logfile_2=fopen(LogFile,"w")) == NULL) /* dann Neuerstellung der Logdatei */
      {
        printf("Log-Datei %s konnte nicht erstellt werden\n",LogFile);
        i = -1;
      }
      else
      {
        i = 0;
        if (csv == 0)
        {
          // Unterscheidung UVR1611 / UVR61-3 !!!!
          if (uvr_typ2 == UVR1611)
            tmp_erg = fwrite(&kopf_winsol_1611,59,1,fp_logfile_2);
          if (uvr_typ2 == UVR61_3)
            tmp_erg = fwrite(&kopf_winsol_61_3,59,1,fp_logfile_2);
          if ( tmp_erg != 1)
          {
            printf("Kopfsatz konnte nicht geschrieben werden!\n");
            i= -1;
          }
        }
        else
        {
          if (uvr_typ2 == UVR1611)  /* UVR1611 */
            fprintf(fp_logfile_2," Datum ; Zeit ; Sens1 ; Sens2 ; Sens3 ; Sens4 ; Sens5 ; Sens6 ; Sens7 ; Sens8 ; Sens9 ; Sens10 ; Sens11 ; Sens12; Sens13 ; Sens14 ; Sens15 ; Sens16 ; Ausg1 ; Drehzst_A1 ; Ausg2 ; Drehzst_A2 ; Ausg3 ; Ausg4 ; Ausg5 ; Ausg6 ; Drehzst_A6 ; Ausg7 ; Drehzst_A7 ; Ausg8 ; Ausg9 ; Ausg10 ; Ausg11 ; Ausg12 ; Ausg13 ; kW1 ; kWh1 ; kW2 ; kWh2 \n");
          if (uvr_typ2 == UVR61_3)  /* UVR61-3 */
            fprintf(fp_logfile_2," Datum ; Zeit ; Sens1 ; Sens2 ; Sens3 ; Sens4 ; Sens5 ; Sens6 ; Ausg1 ; Drehzst_A1 ; Ausg2 ; Ausg3 ; Analogausg ; Vol-Strom ; Momentanlstg ; MWh ; kWh \n");
          csv_header_done=1;
        }
      }
    }
    else /* das Logfile existiert schon */
    {
      fclose(fp_logfile_2);
      if ((fp_logfile_2=fopen(LogFile,"a")) == NULL) /* schreiben ab Dateiende */
      {
        printf("Log-Datei %s konnte nicht geoeffnet werden\n",LogFile);
        i = -1;
      }
      else
      {
        csv_header_done = 1;
        i = 0;
      }
    }
  }

  return(i);
}

/* Logdatei CAN oeffnen / erstellen; int datenrahmen => welcher Datenrahmen wird bearbeitet */
int open_logfile_CAN(char LogFile[], int datenrahmen, int can_typ[8])
{
  FILE *fp_logfile_tmp=NULL;
  int i, tmp_erg = 0;
  i=-1;  /* es ist kein Logfile geoeffnet (Wert -1) */
  /* bei neuer Logdatei der erste Datensatz: */
  UCHAR kopf_winsol_1611[59]={0x01, 0x02, 0x01, 0x03, 0xF0, 0x0F, 0x00, 0x07, 0xAA, 0xAA, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00,
            0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00,
            0xAA, 0x00, 0xFF, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00};
  UCHAR kopf_winsol_1611_canbc[59]={0x01, 0x02, 0x01, 0x03, 0xF0, 0x0F, 0x00, 0x0A, 0xAA, 0xAA, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00,
            0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00,
            0xAA, 0x00, 0xFF, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00};
  if ((fp_logfile_tmp=fopen(LogFile,"r")) == NULL) /* wenn Logfile noch nicht existiert */
  {
    if ((fp_logfile_tmp=fopen(LogFile,"w")) == NULL) /* dann Neuerstellung der Logdatei */
    {
      printf("Log-Datei %s konnte nicht erstellt werden\n",LogFile);
      i = -1;
    }
    else
    {
      i = 0;
      if (csv == 0)
      {

// CAN_BC------------------------------------------------------------------------------------------------------------------
        if (can_typ[datenrahmen-1]==UVR1611)
        {
        tmp_erg = fwrite(&kopf_winsol_1611,59,1,fp_logfile_tmp);
        }
        if (can_typ[datenrahmen-1]==CAN_BC)
        {
        tmp_erg = fwrite(&kopf_winsol_1611_canbc,59,1,fp_logfile_tmp);
        }
// CAN_BC------------------------------------------------------------------------------------------------------------------
        if ( tmp_erg != 1)
        {
          printf("Kopfsatz konnte nicht geschrieben werden!\n");
          i= -1;
        }
      }
      else
      {
        fprintf(fp_logfile_tmp," Datum ; Zeit ; Sens1 ; Sens2 ; Sens3 ; Sens4 ; Sens5 ; Sens6 ; Sens7 ; Sens8 ; Sens9 ; Sens10 ; Sens11 ; Sens12; Sens13 ; Sens14 ; Sens15 ; Sens16 ; Ausg1 ; Drehzst_A1 ; Ausg2 ; Drehzst_A2 ; Ausg3 ; Ausg4 ; Ausg5 ; Ausg6 ; Drehzst_A6 ; Ausg7 ; Drehzst_A7 ; Ausg8 ; Ausg9 ; Ausg10 ; Ausg11 ; Ausg12 ; Ausg13 ; kW1 ; kWh1 ; kW2 ; kWh2 \n");
        csv_header_done=1;
      }
    }
  }
  else /* das Logfile existiert schon */
  {
//  fprintf(stderr,"--> open_logfile_CAN() Logfile existiert, LogFileName: %s - Datenrahmen: %d\n",LogFile,datenrahmen);
    fclose(fp_logfile_tmp);
    if ((fp_logfile_tmp=fopen(LogFile,"a")) == NULL) /* schreiben ab Dateiende */
    {
      printf("Log-Datei %s konnte nicht geoeffnet werden\n",LogFile);
      i = -1;
    }
    else
    {
      csv_header_done = 1;
      i = 0;
    }
  }

  switch( datenrahmen )
  {
    case 1: fp_logfile = fp_logfile_tmp; break;
    case 2: fp_logfile_2 = fp_logfile_tmp; break;
    case 3: fp_logfile_3 = fp_logfile_tmp; break;
    case 4: fp_logfile_4 = fp_logfile_tmp; break;
    case 5: fp_logfile_5 = fp_logfile_tmp; break;
    case 6: fp_logfile_6 = fp_logfile_tmp; break;
    case 7: fp_logfile_7 = fp_logfile_tmp; break;
    case 8: fp_logfile_8 = fp_logfile_tmp; break;
  }

  return(i);
}

/* Logdatei schliessen */
int close_logfile(void)
{
  int i = -1;

  if (fp_logfile == NULL)
  {
        fprintf(stderr, " Kein Logfile offen.\n");
        return(i);
  }

  i=fclose(fp_logfile);
  if (uvr_modus == 0xD1)
  {
        if (fp_logfile_2 == NULL)
    {
          fprintf(stderr, " Kein Logfile offen.\n");
          return(i);
    }
        else
      i=fclose(fp_logfile_2);
  }
  return(i);
}

/*  */
void writeWINSOLlogfile2CSV(FILE * fp_WSLOGcsvfile, const DS_Winsol  *dsatz_winsol, int Jahr ,int Monat )
{
#if 0
  /*  Datum ; Zeit ; Sens1 ; Sens2 ; Sens3 ; Sens4 ; Sens5 ; Sens6 ; Sens7 ; Sens8 ; Sens9 ; Sens10 ; Sens11 ; Sens12; Sens13 ; Sens14 ; Sens15 ; Sens16 ; Ausg1 ; Drehzst_A1 ; Ausg2 ; Drehzst_A2 ; Ausg3 ; Ausg4 ; Ausg5 ; Ausg6 ; Drehzst_A6 ; Ausg7 ; Drehzst_A7 ; Ausg8 ; Ausg9 ; Ausg10 ; Ausg11 ; Ausg12 ; Ausg13 ; kW1 ; kWh1 ; kW2 ; kWh2 */
  /* ;  ; TSp.Unten ; TSp.Mitte ; TSp,Oben ; TBoilerUnten ; TBoilerOben ; Kollektor ; SolarVL ; SolarRL ; TAussen ; TVerteilVL ; TKessel ; TVerteilVL1 ; THeizkrVL ; THeizkrRL ; TZirkuRL ; TKesselVL1 ; Solarpumpe ;  ; Ladepumpe ;  ; ZirkuPumpe ;  ; BrennerAnf ; HeizkrPumpe ;  ;  ;  ; MischerKalt ; MischerWarm ;  ;  ;  ;  ; SolWMZ ;  ;  ;  ; */
  /* 01.01.07;00:00:30;  29,4;  29,7;  45,8;  56,9;  58,2;   8,9;  19,3;  19,0;   6,5;  36,7;  46,7;  26,2;  26,7;  25,9;  28,2;  53,2;
 0; 0; 0; --; 0; 0; 0; 1; --; 0; --; 0; 0; 0; 0; 0; 0;  0,00;  52,0;  ----;  ----; */
  /* 01.01.07;00:01:30;  29,4;  29,7;  45,8;  56,9;  58,1;   9,0;  19,3;  19,0;   6,5;  36,7;  46,6;  26,1;  26,6;  26,0;  28,2;  53,2; 0; 0; 0; --; 0; 0; 0; 1; --; 0; --; 0; 0; 0; 0; 0; 0;  0,00;  52,0;  ----;  ----; */
#endif

  int ii, z, zwi_wmzaehler1, zwi_wmzaehler2;
  int ausgaenge[13];
  float momentLstg1, kwh1, mwh1, momentLstg2, kwh2, mwh2;

  if ( csv_header_done < 0 )
  {
    if (uvr_typ == UVR1611)  /* UVR1611 */
      fprintf(fp_WSLOGcsvfile," Datum ; Zeit ; Sens1 ; Sens2 ; Sens3 ; Sens4 ; Sens5 ; Sens6 ; Sens7 ; Sens8 ; Sens9 ; Sens10 ; Sens11 ; Sens12; Sens13 ; Sens14 ; Sens15 ; Sens16 ; Ausg1 ; Drehzst_A1 ; Ausg2 ; Drehzst_A2 ; Ausg3 ; Ausg4 ; Ausg5 ; Ausg6 ; Drehzst_A6 ; Ausg7 ; Drehzst_A7 ; Ausg8 ; Ausg9 ; Ausg10 ; Ausg11 ; Ausg12 ; Ausg13 ; kW1 ; kWh1 ; kW2 ; kWh2 \n");
//    fprintf(fp_WSLOGcsvfile," Datum ; Zeit ; Sens1 ; Sens2 ; Sens3 ; Sens4 ; Sens5 ; Sens6 ; Sens7 ; Sens8 ; Sens9 ; Sens10 ; Sens11 ; Sens12; Sens13 ; Sens14 ; Sens15 ; Sens16 ; Ausg1 ; Drehzst_A1 ; Ausg2 ; Drehzst_A2 ; Ausg3 ; Ausg4 ; Ausg5 ; Ausg6 ; Drehzst_A6 ; Ausg7 ; Drehzst_A7 ; Ausg8 ; Ausg9 ; Ausg10 ; Ausg11 ; Ausg12 ; Ausg13 ; kW1 ; kWh1 ; kW2 ; kWh2 ");
//    fprintf(fp_WSLOGcsvfile," Datum ; Zeit ; Sens1 ; Sens2 ; Sens3 ; Sens4 ; Sens5 ; Sens6 ; Sens7 ; Sens8 ; Sens9 ; Sens10 ; Sens11 ; Sens12; Sens13 ; Sens14 ; Sens15 ; Sens16 ; Ausg1 ; Drehzst_A1 ; Ausg2 ; Drehzst_A2 ; Ausg3 ; Ausg4 ; Ausg5 ; Ausg6 ; Drehzst_A6 ; Ausg7 ; Drehzst_A7 ; Ausg8 ; Ausg9 ; Ausg10 ; Ausg11 ; Ausg12 ; Ausg13 ; kW1 ; kWh1 ; kW2 ; kWh2 ");

    if (uvr_typ == UVR61_3)  /* UVR61-3 */
      fprintf(fp_WSLOGcsvfile," Datum ; Zeit ; Sens1 ; Sens2 ; Sens3 ; Sens4 ; Sens5 ; Sens6 ; Ausg1 ; Drehzst_A1 ; Ausg2 ; Ausg3 ; Analogausg ; Vol-Strom ; Momentanlstg ; MWh ; kWh \n");
      csv_header_done=1;
  }

  fprintf(fp_WSLOGcsvfile,"%02d.%02d.%02d;%02d:%02d:%02d;", dsatz_winsol[0].tag,Monat,Jahr, dsatz_winsol[0].std,dsatz_winsol[0].min,dsatz_winsol[0].sek);

  /* need to test for: */
  /* used/unused sensor */
  /* type of sensor */
//#ifdef NEW_WSOL_TEMP
  for (ii=0;ii<16;ii++)
    fprintf(fp_WSLOGcsvfile,"%.1f;", berechnetemp(dsatz_winsol[0].sensT[ii][0],dsatz_winsol[0].sensT[ii][1]) );

  /* Ausgaenge */
  /* 1. Byte AAAA AAAA */
  /* 2. Byte xxxA AAAA  */
  /* x  ... dont care   */
  /* A ... Ausgang (von rechts nach links zu nummerieren) */

  for(z=0;z<8;z++)
  {
    if (tstbit( dsatz_winsol[0].ausgbyte1, 7-z ) == 1)
    {
      ausgaenge[z] = 1;
//      printf (" A%d ja\n",z+1);
    }
    else
    {
      ausgaenge[z] = 0;
//      printf (" A%d nein\n",z+1);
    }
  }

  for(z=0;z<5;z++)
  {
    if ( tstbit( dsatz_winsol[0].ausgbyte2, 7-z ) == 1)
    {
//      printf (" A%d  1 \n",z+8);
      ausgaenge[z+7] = 1;
    }
    else
    {
//      printf (" A%d 0\n",z+8);
      ausgaenge[z+7] = 0;
    }
  }

  /* 4 Drehzahlstufe je 1byte */
  /* Bitbelegung      NxxD DDDD  */
  /* N ...Drehzahlregelung aktiv (0) */
  /* D ... Drehzahlstufe (0-30) */
  /* x  ... dont care  */

  for(z=0;z<13;z++)
  {
//    if ( ausgaenge[z] )
//      printf("A%i  Zustand: 1               ",z+1);
//    else
//      printf("A%i  Zustand: 0               ",z+1);

    if (z==0 || z==1 || z==5 || z==6 )
    {
      int index=z;
      if (z>4) index-=3;
//        printf("Drehzahlregler DA%d Zustand: %i     ", z+1,tstbit( dsatz_winsol[0].dza[z], 7 ));

      UCHAR temp_highbyte;
      temp_highbyte = dsatz_winsol[0].dza[index];

#ifdef DEBUG
         fprintf(stderr, " Drehz.-Stufe highbyte: %X  \n", temp_highbyte);
#endif
      int jj=0;
      for(jj=5;jj<9;jj++)
        temp_highbyte = clrbit(temp_highbyte,jj); /* die oberen EEE Bits auf 0 setzen */

//         printf ("  Drehzst_A%d: %d\n",z+1,(int)temp_highbyte);

      if ( tstbit( dsatz_winsol[0].dza[z], 7 ) )
      {
        fprintf(fp_WSLOGcsvfile," %d; %d;", ausgaenge[z],(int)temp_highbyte); /*  DRehz Ausgang mit Drehzahl */
      }
      else
        fprintf(fp_WSLOGcsvfile," %d; --;", ausgaenge[z]); /* Drehz Ausgang als digitaler Ausgang */
    }
    else
      fprintf(fp_WSLOGcsvfile," %d; --;", ausgaenge[z]); /* kein Drehz Ausgang */
  }


  /*    printf("Byte Waermemengenzaehler: %x\n",akt_daten[39]); */
  switch(dsatz_winsol[0].wmzaehler_reg )
  {
    case 1:
      zwi_wmzaehler1 = 1; break; /* Waermemengenzaehler1 */
    case 2:
      zwi_wmzaehler2 = 1; break; /* Waermemengenzaehler2 */
    case 3:
      zwi_wmzaehler1 = 1;        /* Waermemengenzaehler1 */
      zwi_wmzaehler2 = 1;   /* Waermemengenzaehler2 */
      break;
  }

  if (zwi_wmzaehler1 == 1)
  {
    momentLstg1 = (10*(65536*(float)dsatz_winsol[0].mlstg1[3]
         + 256*(float)dsatz_winsol[0].mlstg1[2]
         +(float)dsatz_winsol[0].mlstg1[1])
         +((float)dsatz_winsol[0].mlstg1[0]*10)/256)/100;
//      printf("Momentanleistung 1: %.1f\n",momentLstg1);
    kwh1 = ( (float)dsatz_winsol[0].kwh1[1] *256 + (float)dsatz_winsol[0].kwh1[0] ) / 10;
    mwh1 = (float)dsatz_winsol[0].mwh1[1] *0x100 + (float)dsatz_winsol[0].mwh1[0];
//      printf("Waermemengenzaehler 1: %.0f MWh und %.1f kWh\n",mwh1,kwh1);

    fprintf(fp_WSLOGcsvfile," %.1f; %.1f;",momentLstg1,kwh1);
  }
  else
    fprintf(fp_WSLOGcsvfile," --; --;");

  if (zwi_wmzaehler2 == 1)
  {
    momentLstg2 = (10*(65536*(float)dsatz_winsol[0].mlstg2[3]
       + 256*(float)dsatz_winsol[0].mlstg2[2]
       +(float)dsatz_winsol[0].mlstg2[1])
       +((float)dsatz_winsol[0].mlstg2[0]*10)/256)/100;
//      printf("Momentanleistung 1: %.1f\n",momentLstg2);
    kwh2 = ( (float)dsatz_winsol[0].kwh2[1] *256 + (float)dsatz_winsol[0].kwh2[0] ) / 10;
    mwh2 = (float)dsatz_winsol[0].mwh2[1] *0x100 + (float)dsatz_winsol[0].mwh2[0];
//      printf("Waermemengenzaehler 1: %.0f MWh und %.1f kWh\n",mwh2,kwh2);

    fprintf(fp_WSLOGcsvfile," %.1f; %.1f;",momentLstg2,kwh2);
  }
  else
    fprintf(fp_WSLOGcsvfile," --; --;");

  fprintf(fp_WSLOGcsvfile,"\n");
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
    if (!ip_first)
    {
      sock = socket(PF_INET, SOCK_STREAM, 0);
      if (sock == -1)
      {
        perror("socket failed()");
        do_cleanup();
        return 2;
      }
      if (connect(sock, (const struct sockaddr *)&SERVER_sockaddr_in, sizeof(SERVER_sockaddr_in)) == -1)
      {
        perror("connect failed()");
        do_cleanup();
        return 3;
      }
     }  /* if (!ip_first) */
     write_erg=send(sock,sendbuf,1,0);
     if ( write_erg == 1)    /* Lesen der Antwort */
      result  = recv(sock,empfbuf,1,0);
  }

  return empfbuf[0];
}

/* Modultyp incl. Firmware abfragen */
int get_modultyp(void)
{
  int result; //, i=0, j=0, marker=0;
  UCHAR sendbuf[8];       /*  sendebuffer fuer die Request-Commandos*/
//  int sendbuf[8];       /*  sendebuffer fuer die Request-Commandos*/
  UCHAR empfbuf[5];
//  UCHAR tmp_buf[5];
  unsigned modTeiler = 0x100;

  sendbuf[0]=0x20;
  sendbuf[1]=0x10;
  sendbuf[2]=0x18;
  sendbuf[3]=0x0;
  sendbuf[4]=0x0;
  sendbuf[5]=0x0;
  sendbuf[6]=0x0;

  sendbuf[7]= (sendbuf[0] + sendbuf[1] + sendbuf[2] + sendbuf[3] + sendbuf[4] + sendbuf[5] + sendbuf[6]) % modTeiler;  /* Pruefziffer */

  if (usb_zugriff)
  {
    write_erg=write(fd,sendbuf,8);
    if (write_erg == 1)    /* Lesen der Antwort*/
      result=read(fd,empfbuf,5);
  }
  if (ip_zugriff)
  {
    if (!ip_first)
    {
      sock = socket(PF_INET, SOCK_STREAM, 0);
      if (sock == -1)
      {
        perror("socket failed()");
        do_cleanup();
        return 2;
      }
      if (connect(sock, (const struct sockaddr *)&SERVER_sockaddr_in, sizeof(SERVER_sockaddr_in)) == -1)
      {
        perror("connect failed()");
        do_cleanup();
        return 3;
      }
     }  /* if (!ip_first) */
     write_erg=send(sock,sendbuf,8,0);
     if ( write_erg == 8)    /* Lesen der Antwort */
         {
      result  = recv(sock,empfbuf,5,0);
                // do
                // {
                        // result  = recv(sock,tmp_buf,5,0);
                        // for (j=0;j<result;j++)
                        // {
                                // empfbuf[marker+j] = tmp_buf[j];
                        // }
                        // marker = marker + result;
                // } while ( marker < 5 );
         }
  }

  // for (i=0;i<=result;i++)
        // fprintf(stderr,"Var %d result: %x. \n",i,empfbuf[i]);

  if (empfbuf[0] == 0x21 && empfbuf[1] == 0x43)
        return empfbuf[3];
  else
    return -1;
}

/* Kopfsatz vom DL lesen und Rueckgabe Anzahl Datensaetze */
int kopfsatzlesen(void)
{
  int result, anz_ds, pruefz, merk_pruefz, durchlauf;
  UCHAR sendbuf[1];       /*  sendebuffer fuer die Request-Commandos*/
//  int sendbuf[1];       /*  sendebuffer fuer die Request-Commandos*/
  durchlauf=0;
  int in_bytes=0;

  do
  {
    sendbuf[0]=KOPFSATZLESEN;    /* Senden der Kopfsatz-abfrage */

    if (usb_zugriff)
    {
      init_usb();
      fd_set rfds;
      struct timeval tv;
      int retval=0;
      int retry=0;
      int retry_interval=2; 
	    
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
            if (FD_ISSET(fd,&rfds))
            {
              ioctl(fd, FIONREAD, &in_bytes);
#ifdef DEBUG
  fprintf(stderr,"Bytes im Puffer: %d\n",in_bytes);
#endif
              switch(uvr_modus)
              {
                case 0xD1: if (in_bytes == 14)
                           {
                             result=read(fd,kopf_D1,14);
                             retry=4;
                           }
                           break;
                case 0xA8: if (in_bytes == 13) 
                           {
                              result=read(fd,kopf_A8,13);
                              retry=4;
                            }
                            break;
                case 0xDC: if ((in_bytes == 21) || (in_bytes == 1))
                           {
                              result=read(fd,kopf_DC,21);
                              retry=4;
                            }
                           if (kopf_DC[0].all_bytes[0] == 0xAA)
                             return -3;
                           break;
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
            fprintf(stderr,"%s - No data within %d seconds. r%d s%d\n",sZeit,retry_interval,
            retry,send_retry);
#endif
            sleep(retry_interval);
            retry++;
          }
        }
        while( retry < 3 && in_bytes != 0);
      }
	}

    if (ip_zugriff)
    {
      if (!ip_first)
      {
        sock = socket(PF_INET, SOCK_STREAM, 0);
        if (sock == -1)
        {
          perror("socket failed()");
          do_cleanup();
          return 2;
        }
        if (connect(sock, (const struct sockaddr *)&SERVER_sockaddr_in, sizeof(SERVER_sockaddr_in)) == -1)
        {
          perror("connect failed()");
          do_cleanup();
          return 3;
        }
      }  /* if (!ip_first) */
      write_erg=send(sock,sendbuf,1,0);

      if (write_erg == 1)    /* Lesen der Antwort */
      {
        switch(uvr_modus)
        {
          case 0xD1: result = recv(sock,kopf_D1,14,0); break;
          case 0xA8: result = recv(sock,kopf_A8,13,0); break;
          case 0xDC: result = recv(sock,kopf_DC,21,0);
                     if (kopf_DC[0].all_bytes[0] == 0xAA)
                       return -3;
                     break;
        }
      }
    }

    switch(uvr_modus)
    {
      case 0xD1: pruefz = berechneKopfpruefziffer_D1( kopf_D1 );
                 merk_pruefz = kopf_D1[0].pruefsum;
                 break;
      case 0xA8: pruefz = berechneKopfpruefziffer_A8( kopf_A8 );
                 merk_pruefz = kopf_A8[0].pruefsum;
                 break;
      case 0xDC:
        #ifdef DEBUG
                fprintf(stderr, " CAN-Logging-Test: Anzahl Datenrahmen laut Byte 6: %x\n",kopf_DC[0].all_bytes[5]);
        #endif
                pruefz = berechneKopfpruefziffer_DC( kopf_DC );
                switch(kopf_DC[0].all_bytes[5])
                {
                case 1: merk_pruefz = kopf_DC[0].DC_Rahmen1.pruefsum;
                        #ifdef DEBUG
                        fprintf(stderr,"  Durchlauf #%d  berechnete pruefziffer:%d DC_Rahmen1.pruefsumme:%d\n",durchlauf,pruefz%0x100,kopf_DC[0].DC_Rahmen1.pruefsum);
                        #endif
                        break;
                  case 2: merk_pruefz = kopf_DC[0].DC_Rahmen2.pruefsum;
                        #ifdef DEBUG
                        fprintf(stderr,"  Durchlauf #%d  berechnete pruefziffer:%d DC_Rahmen2.pruefsumme:%d\n",durchlauf,pruefz%0x100,kopf_DC[0].DC_Rahmen2.pruefsum);
                        #endif
                        break;
                  case 3: merk_pruefz = kopf_DC[0].DC_Rahmen3.pruefsum;
                        #ifdef DEBUG
                        fprintf(stderr,"  Durchlauf #%d  berechnete pruefziffer:%d DC_Rahmen3.pruefsumme:%d\n",durchlauf,pruefz%0x100,kopf_DC[0].DC_Rahmen3.pruefsum);
                        #endif
                        break;
                  case 4: merk_pruefz = kopf_DC[0].DC_Rahmen4.pruefsum;
                        #ifdef DEBUG
                        fprintf(stderr,"  Durchlauf #%d  berechnete pruefziffer:%d DC_Rahmen4.pruefsumme:%d\n",durchlauf,pruefz%0x100,kopf_DC[0].DC_Rahmen4.pruefsum);
                        #endif
                        break;
                  case 5: merk_pruefz = kopf_DC[0].DC_Rahmen5.pruefsum;
                        #ifdef DEBUG
                        fprintf(stderr,"  Durchlauf #%d  berechnete pruefziffer:%d DC_Rahmen5.pruefsumme:%d\n",durchlauf,pruefz%0x100,kopf_DC[0].DC_Rahmen5.pruefsum);
                        #endif
                        break;
                  case 6: merk_pruefz = kopf_DC[0].DC_Rahmen6.pruefsum;
                        #ifdef DEBUG
                        fprintf(stderr,"  Durchlauf #%d  berechnete pruefziffer:%d DC_Rahmen6.pruefsumme:%d\n",durchlauf,pruefz%0x100,kopf_DC[0].DC_Rahmen6.pruefsum);
                        #endif
                        break;
                  case 7: merk_pruefz = kopf_DC[0].DC_Rahmen7.pruefsum;
                        #ifdef DEBUG
                        fprintf(stderr,"  Durchlauf #%d  berechnete pruefziffer:%d DC_Rahmen7.pruefsumme:%d\n",durchlauf,pruefz%0x100,kopf_DC[0].DC_Rahmen7.pruefsum);
                        #endif
                        break;
                  case 8: merk_pruefz = kopf_DC[0].DC_Rahmen8.pruefsum;
                        #ifdef DEBUG
                        fprintf(stderr,"  Durchlauf #%d  berechnete pruefziffer:%d DC_Rahmen8.pruefsumme:%d\n",durchlauf,pruefz%0x100,kopf_DC[0].DC_Rahmen8.pruefsum);
                        #endif
                        break;
                  default: fprintf(stderr,"  CAN-Logging-Test:  Kennung %x\n",kopf_DC[0].all_bytes[0]);
                }
        break;
    }

    durchlauf++;
   #ifdef DEBUG
    if ( uvr_modus == 0xD1 )
      fprintf(stderr, "  Durchlauf #%d  berechnete pruefziffer:%d kopfsatz.pruefsumme:%d\n",durchlauf,pruefz%0x100,kopf_D1[0].pruefsum);
    if ( uvr_modus == 0xA8 )
      fprintf(stderr, "  Durchlauf #%d  berechnete pruefziffer:%d kopfsatz.pruefsumme:%d\n",durchlauf,pruefz%0x100,kopf_A8[0].pruefsum);
   #endif
  }
  while (( (pruefz != merk_pruefz )  && (durchlauf < 10)));

  if (pruefz != merk_pruefz )
    {
      fprintf(stderr, " Durchlauf #%i -  berechnete pruefziffer:%d kopfsatz.pruefsumme:%d\n",durchlauf, pruefz, merk_pruefz);
      return -1;
    }
#ifdef DEBUG
  else
    fprintf(stderr, "Anzahl Durchlaeufe Pruefziffer Kopfsatz: %i\n",durchlauf);
#endif

  /* Startadresse der Daten */
  switch(uvr_modus)
  {
    case 0xD1: start_adresse = kopf_D1[0].startadresse;
                 end_adresse = kopf_D1[0].endadresse;
               break;
    case 0xA8: start_adresse = kopf_A8[0].startadresse;
                 end_adresse = kopf_A8[0].endadresse;
               break;
    case 0xDC: switch(kopf_DC[0].all_bytes[5])
               {
                  case 1: start_adresse = kopf_DC[0].DC_Rahmen1.startadresse;
                            end_adresse = kopf_DC[0].DC_Rahmen1.endadresse;
                          break;
                  case 2: start_adresse = kopf_DC[0].DC_Rahmen2.startadresse;
                             end_adresse = kopf_DC[0].DC_Rahmen2.endadresse;
                          break;
                  case 3: start_adresse = kopf_DC[0].DC_Rahmen3.startadresse;
                            end_adresse = kopf_DC[0].DC_Rahmen3.endadresse;
                          break;
                  case 4: start_adresse = kopf_DC[0].DC_Rahmen4.startadresse;
                            end_adresse = kopf_DC[0].DC_Rahmen4.endadresse;
                          break;
                  case 5: start_adresse = kopf_DC[0].DC_Rahmen5.startadresse;
                            end_adresse = kopf_DC[0].DC_Rahmen5.endadresse;
                          break;
                  case 6: start_adresse = kopf_DC[0].DC_Rahmen6.startadresse;
                            end_adresse = kopf_DC[0].DC_Rahmen6.endadresse;
                          break;
                  case 7: start_adresse = kopf_DC[0].DC_Rahmen7.startadresse;
                            end_adresse = kopf_DC[0].DC_Rahmen7.endadresse;
                          break;
                  case 8: start_adresse = kopf_DC[0].DC_Rahmen8.startadresse;
                            end_adresse = kopf_DC[0].DC_Rahmen8.endadresse;
                          break;
                }
                break;
  }

  switch(uvr_modus)
  {
    case 0xD1:
      anz_ds = anzahldatensaetze_D1(kopf_D1);
      break;
    case 0xA8:
      anz_ds = anzahldatensaetze_A8(kopf_A8);
      break;
        case 0xDC:
      anz_ds = anzahldatensaetze_DC(kopf_DC);
#if DEBUG > 3
          switch(kopf_DC[0].all_bytes[5])
          {
            case 1: /* print_endaddr = kopf_DC[0].DC_Rahmen1.endadresse[0]; */
                fprintf(stderr,"%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
                kopf_DC[0].all_bytes[0],kopf_DC[0].all_bytes[1],kopf_DC[0].all_bytes[2],kopf_DC[0].all_bytes[3],
                kopf_DC[0].all_bytes[4],kopf_DC[0].all_bytes[5],kopf_DC[0].all_bytes[6],kopf_DC[0].all_bytes[7],
                kopf_DC[0].all_bytes[8],kopf_DC[0].all_bytes[9],kopf_DC[0].all_bytes[10],kopf_DC[0].all_bytes[11],
                kopf_DC[0].all_bytes[12],kopf_DC[0].all_bytes[13]);
                break;
            case 2: /* print_endaddr = kopf_DC[0].DC_Rahmen2.endadresse[0]; */
                fprintf(stderr,"%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
                kopf_DC[0].all_bytes[0],kopf_DC[0].all_bytes[1],kopf_DC[0].all_bytes[2],kopf_DC[0].all_bytes[3],
                kopf_DC[0].all_bytes[4],kopf_DC[0].all_bytes[5],kopf_DC[0].all_bytes[6],kopf_DC[0].all_bytes[7],
                kopf_DC[0].all_bytes[8],kopf_DC[0].all_bytes[9],kopf_DC[0].all_bytes[10],kopf_DC[0].all_bytes[11],
                kopf_DC[0].all_bytes[12],kopf_DC[0].all_bytes[13],kopf_DC[0].all_bytes[14]);
                break;
            case 3: /* print_endaddr = kopf_DC[0].DC_Rahmen3.endadresse[0]; */
                fprintf(stderr,"%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
                kopf_DC[0].all_bytes[0],kopf_DC[0].all_bytes[1],kopf_DC[0].all_bytes[2],kopf_DC[0].all_bytes[3],
                kopf_DC[0].all_bytes[4],kopf_DC[0].all_bytes[5],kopf_DC[0].all_bytes[6],kopf_DC[0].all_bytes[7],
                kopf_DC[0].all_bytes[8],kopf_DC[0].all_bytes[9],kopf_DC[0].all_bytes[10],kopf_DC[0].all_bytes[11],
                kopf_DC[0].all_bytes[12],kopf_DC[0].all_bytes[13],kopf_DC[0].all_bytes[14],kopf_DC[0].all_bytes[15]);
                break;
            case 4: /* print_endaddr = kopf_DC[0].DC_Rahmen4.endadresse[0]; */
                fprintf(stderr,"%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
                kopf_DC[0].all_bytes[0],kopf_DC[0].all_bytes[1],kopf_DC[0].all_bytes[2],kopf_DC[0].all_bytes[3],
                kopf_DC[0].all_bytes[4],kopf_DC[0].all_bytes[5],kopf_DC[0].all_bytes[6],kopf_DC[0].all_bytes[7],
                kopf_DC[0].all_bytes[8],kopf_DC[0].all_bytes[9],kopf_DC[0].all_bytes[10],kopf_DC[0].all_bytes[11],
                kopf_DC[0].all_bytes[12],kopf_DC[0].all_bytes[13],kopf_DC[0].all_bytes[14],kopf_DC[0].all_bytes[15],
                kopf_DC[0].all_bytes[16]);
                break;
            case 5: /* print_endaddr = kopf_DC[0].DC_Rahmen5.endadresse[0]; */
                fprintf(stderr,"%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
                kopf_DC[0].all_bytes[0],kopf_DC[0].all_bytes[1],kopf_DC[0].all_bytes[2],kopf_DC[0].all_bytes[3],
                kopf_DC[0].all_bytes[4],kopf_DC[0].all_bytes[5],kopf_DC[0].all_bytes[6],kopf_DC[0].all_bytes[7],
                kopf_DC[0].all_bytes[8],kopf_DC[0].all_bytes[9],kopf_DC[0].all_bytes[10],kopf_DC[0].all_bytes[11],
                kopf_DC[0].all_bytes[12],kopf_DC[0].all_bytes[13],kopf_DC[0].all_bytes[14],kopf_DC[0].all_bytes[15],
                kopf_DC[0].all_bytes[16],kopf_DC[0].all_bytes[17]);
                break;
            case 6: /* print_endaddr = kopf_DC[0].DC_Rahmen6.endadresse[0]; */
                fprintf(stderr,"%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
                kopf_DC[0].all_bytes[0],kopf_DC[0].all_bytes[1],kopf_DC[0].all_bytes[2],kopf_DC[0].all_bytes[3],
                kopf_DC[0].all_bytes[4],kopf_DC[0].all_bytes[5],kopf_DC[0].all_bytes[6],kopf_DC[0].all_bytes[7],
                kopf_DC[0].all_bytes[8],kopf_DC[0].all_bytes[9],kopf_DC[0].all_bytes[10],kopf_DC[0].all_bytes[11],
                kopf_DC[0].all_bytes[12],kopf_DC[0].all_bytes[13],kopf_DC[0].all_bytes[14],kopf_DC[0].all_bytes[15],
                kopf_DC[0].all_bytes[16],kopf_DC[0].all_bytes[17],kopf_DC[0].all_bytes[18]);
                break;
            case 7: /* print_endaddr = kopf_DC[0].DC_Rahmen7.endadresse[0]; */
                fprintf(stderr,"%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
                kopf_DC[0].all_bytes[0],kopf_DC[0].all_bytes[1],kopf_DC[0].all_bytes[2],kopf_DC[0].all_bytes[3],
                kopf_DC[0].all_bytes[4],kopf_DC[0].all_bytes[5],kopf_DC[0].all_bytes[6],kopf_DC[0].all_bytes[7],
                kopf_DC[0].all_bytes[8],kopf_DC[0].all_bytes[9],kopf_DC[0].all_bytes[10],kopf_DC[0].all_bytes[11],
                kopf_DC[0].all_bytes[12],kopf_DC[0].all_bytes[13],kopf_DC[0].all_bytes[14],kopf_DC[0].all_bytes[15],
                kopf_DC[0].all_bytes[16],kopf_DC[0].all_bytes[17],kopf_DC[0].all_bytes[18],kopf_DC[0].all_bytes[19]);
                break;
            case 8: /* print_endaddr = kopf_DC[0].DC_Rahmen8.endadresse[0]; */
                fprintf(stderr,"%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
                kopf_DC[0].all_bytes[0],kopf_DC[0].all_bytes[1],kopf_DC[0].all_bytes[2],kopf_DC[0].all_bytes[3],
                kopf_DC[0].all_bytes[4],kopf_DC[0].all_bytes[5],kopf_DC[0].all_bytes[6],kopf_DC[0].all_bytes[7],
                kopf_DC[0].all_bytes[8],kopf_DC[0].all_bytes[9],kopf_DC[0].all_bytes[10],kopf_DC[0].all_bytes[11],
                kopf_DC[0].all_bytes[12],kopf_DC[0].all_bytes[13],kopf_DC[0].all_bytes[14],kopf_DC[0].all_bytes[15],
                kopf_DC[0].all_bytes[16],kopf_DC[0].all_bytes[17],kopf_DC[0].all_bytes[18],kopf_DC[0].all_bytes[19],kopf_DC[0].all_bytes[20]);
                break;
          }
#endif
      break;
  }

#if  DEBUG>5
  fprintf(stderr, "  Errechnete Pruefsumme: %x\n", berechneKopfpruefziffer(kopf) ); printf(" empfangene Pruefsumme: %x\n",kopf[0].pruefsum);
  fprintf(stderr, "empfangen von DL/BL:\n Kennung: %X\n",kopf[0].kennung);
  fprintf(stderr, " Version: %X\n",kopf[0].version);
  fprintf(stderr, " Zeitstempel (hex): %x %x %x\n",kopf[0].zeitstempel[0],kopf[0].zeitstempel[1],kopf[0].zeitstempel[2]);
  fprintf(stderr, " Zeitstempel (int): %d %d %d\n",kopf[0].zeitstempel[0],kopf[0].zeitstempel[1],kopf[0].zeitstempel[2]);

  long secs = (long)(kopf[0].zeitstempel[0]) * 32768 + (long)(kopf[0].zeitstempel[1]) * 256 + (long)(kopf[0].zeitstempel[2]);
  long secsinv = (long)(kopf[0].zeitstempel[0]) + (long)(kopf[0].zeitstempel[1]) * 256 + (long)(kopf[0].zeitstempel[2]) * 32768;

  fprintf(stderr, " Zeitstempel : %ld  %ld\n", secs,secsinv);

  fprintf(stderr, " Satzlaenge: %x\n",kopf[0].satzlaenge);
  fprintf(stderr, " Startadresse: %x %x %x\n",kopf[0].startadresse[0],kopf[0].startadresse[1],kopf[0].startadresse[2]);
  fprintf(stderr, " Endadresse: %x %x %x\n",kopf[0].endadresse[0],kopf[0].endadresse[1],kopf[0].endadresse[2]);
#endif

  if ( uvr_modus == 0xD1 )
  {
    uvr_typ = kopf_D1[0].satzlaengeGeraet1; /* 0x5A -> UVR61-3; 0x76 -> UVR1611 */
    uvr_typ2 = kopf_D1[0].satzlaengeGeraet2; /* 0x5A -> UVR61-3; 0x76 -> UVR1611 */
  }
  else
  {
    uvr_typ = kopf_A8[0].satzlaengeGeraet1; /* 0x5A -> UVR61-3; 0x76 -> UVR1611 */
  }
  if ( uvr_modus == 0xDC )
  {
    uvr_typ = 0x76;  /* CAN-Logging nur mit UVR1611 */
  }

  switch( anz_ds)
  {
    case -1: zeitstempel();
            fprintf(fp_varlogfile,"%s - %s -- Falschen Wert im Low-Byte Endadresse gelesen! Wert: %x\n",sDatum, sZeit,*end_adresse);
            return -1;
    case -2: zeitstempel();
            fprintf(fp_varlogfile,"%s - %s -- Keine Daten vorhanden.\n",sDatum, sZeit);
            return -1;
    default: if (uvr_modus != 0xDC)
                printf(" Anzahl Datensaetze aus Kopfsatz: %i\n",anz_ds);
             break;
  }

  return anz_ds;
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
  printf("Vom DL erhalten Version: %x\n",empfbuf[0]);

  sendbuf[0]=FWABFRAGE;    /* Senden der Firmware-Versionsabfrage */
  write_erg=write(fd,sendbuf,1);
  if (write_erg == 1)    /* Lesen der Antwort*/
    result=read(fd,empfbuf,1);
  printf("Vom DL erhalten Version FW: %x\n",empfbuf[0]);

  sendbuf[0]=MODEABFRAGE;    /* Senden der Modus-abfrage */
  write_erg=write(fd,sendbuf,1);
  if (write_erg == 1)    /* Lesen der Antwort*/
    result=read(fd,empfbuf,1);
  printf("Vom DL erhalten Modus: %x\n",empfbuf[0]);
}

/* Kopieren der Daten (UVR1611) in die Winsol-Format Struktur */
int copy_UVR2winsol_1611(u_DS_UVR1611_UVR61_3 *dsatz_uvr1611, DS_Winsol  *dsatz_winsol )
{
  BYTES_LONG byteslong_mlstg1, byteslong_mlstg2;

  dsatz_winsol[0].tag = dsatz_uvr1611[0].DS_UVR1611.datum_zeit.tag ;
  dsatz_winsol[0].std = dsatz_uvr1611[0].DS_UVR1611.datum_zeit.std ;
  dsatz_winsol[0].min = dsatz_uvr1611[0].DS_UVR1611.datum_zeit.min ;
  dsatz_winsol[0].sek = dsatz_uvr1611[0].DS_UVR1611.datum_zeit.sek ;
  dsatz_winsol[0].ausgbyte1 = dsatz_uvr1611[0].DS_UVR1611.ausgbyte1 ;
  dsatz_winsol[0].ausgbyte2 = dsatz_uvr1611[0].DS_UVR1611.ausgbyte2 ;
  dsatz_winsol[0].dza[0] = dsatz_uvr1611[0].DS_UVR1611.dza[0] ;
  dsatz_winsol[0].dza[1] = dsatz_uvr1611[0].DS_UVR1611.dza[1] ;
  dsatz_winsol[0].dza[2] = dsatz_uvr1611[0].DS_UVR1611.dza[2] ;
  dsatz_winsol[0].dza[3] = dsatz_uvr1611[0].DS_UVR1611.dza[3] ;

  int ii=0;
  for (ii=0;ii<16;ii++)
    {
      dsatz_winsol[0].sensT[ii][0] = dsatz_uvr1611[0].DS_UVR1611.sensT[ii][0] ;
      dsatz_winsol[0].sensT[ii][1] = dsatz_uvr1611[0].DS_UVR1611.sensT[ii][1] ;
    }
  dsatz_winsol[0].wmzaehler_reg = dsatz_uvr1611[0].DS_UVR1611.wmzaehler_reg ;

  if ( dsatz_uvr1611[0].DS_UVR1611.mlstg1[3] > 0x7f ) /* negtive Wete */
  {
    byteslong_mlstg1.long_word = (10*((65536*(float)dsatz_uvr1611[0].DS_UVR1611.mlstg1[3]
    +256*(float)dsatz_uvr1611[0].DS_UVR1611.mlstg1[2]
    +(float)dsatz_uvr1611[0].DS_UVR1611.mlstg1[1])-65536)
    -((float)dsatz_uvr1611[0].DS_UVR1611.mlstg1[0]*10)/256);
    byteslong_mlstg1.long_word = byteslong_mlstg1.long_word | 0xffff0000;
  }
  else
    byteslong_mlstg1.long_word = (10*(65536*(float)dsatz_uvr1611[0].DS_UVR1611.mlstg1[3]
    +256*(float)dsatz_uvr1611[0].DS_UVR1611.mlstg1[2]
    +(float)dsatz_uvr1611[0].DS_UVR1611.mlstg1[1])
    +((float)dsatz_uvr1611[0].DS_UVR1611.mlstg1[0]*10)/256);

  dsatz_winsol[0].mlstg1[0] = byteslong_mlstg1.bytes.lowlowbyte ;
  dsatz_winsol[0].mlstg1[1] = byteslong_mlstg1.bytes.lowhighbyte ;
  dsatz_winsol[0].mlstg1[2] = byteslong_mlstg1.bytes.highlowbyte ;
  dsatz_winsol[0].mlstg1[3] = byteslong_mlstg1.bytes.highhighbyte ;
  dsatz_winsol[0].kwh1[0] = dsatz_uvr1611[0].DS_UVR1611.kwh1[0] ;
  dsatz_winsol[0].kwh1[1] = dsatz_uvr1611[0].DS_UVR1611.kwh1[1] ;
  dsatz_winsol[0].mwh1[0] = dsatz_uvr1611[0].DS_UVR1611.mwh1[0] ;
  dsatz_winsol[0].mwh1[1] = dsatz_uvr1611[0].DS_UVR1611.mwh1[1] ;

  if ( dsatz_uvr1611[0].DS_UVR1611.mlstg2[3] > 0x7f ) /* negtive Wete */
  {
    byteslong_mlstg2.long_word = (10*((65536*(float)dsatz_uvr1611[0].DS_UVR1611.mlstg2[3]
    +256*(float)dsatz_uvr1611[0].DS_UVR1611.mlstg2[2]
    +(float)dsatz_uvr1611[0].DS_UVR1611.mlstg2[1])-65536)
    -((float)dsatz_uvr1611[0].DS_UVR1611.mlstg2[0]*10)/256);
    byteslong_mlstg2.long_word = byteslong_mlstg2.long_word | 0xffff0000;
  }
  else
    byteslong_mlstg2.long_word = (10*(65536*(float)dsatz_uvr1611[0].DS_UVR1611.mlstg2[3]
    +256*(float)dsatz_uvr1611[0].DS_UVR1611.mlstg2[2]
    +(float)dsatz_uvr1611[0].DS_UVR1611.mlstg2[1])
    +((float)dsatz_uvr1611[0].DS_UVR1611.mlstg2[0]*10)/256);

  dsatz_winsol[0].mlstg2[0] = byteslong_mlstg2.bytes.lowlowbyte ;
  dsatz_winsol[0].mlstg2[1] = byteslong_mlstg2.bytes.lowhighbyte ;
  dsatz_winsol[0].mlstg2[2] = byteslong_mlstg2.bytes.highlowbyte ;
  dsatz_winsol[0].mlstg2[3] = byteslong_mlstg2.bytes.highhighbyte ;
  dsatz_winsol[0].kwh2[0] = dsatz_uvr1611[0].DS_UVR1611.kwh2[0] ;
  dsatz_winsol[0].kwh2[1] = dsatz_uvr1611[0].DS_UVR1611.kwh2[1] ;
  dsatz_winsol[0].mwh2[0] = dsatz_uvr1611[0].DS_UVR1611.mwh2[0] ;
  dsatz_winsol[0].mwh2[1] = dsatz_uvr1611[0].DS_UVR1611.mwh2[1] ;

  return 1;
}

/* Kopieren der Daten (UVR1611 - CAN-Logging) in die Winsol-Format Struktur */
int copy_UVR2winsol_1611_CAN(s_DS_CAN *dsatz_uvr1611, DS_Winsol  *dsatz_winsol )
{
  BYTES_LONG byteslong_mlstg1, byteslong_mlstg2;

  dsatz_winsol[0].tag = dsatz_uvr1611[0].datum_zeit.tag ;
  dsatz_winsol[0].std = dsatz_uvr1611[0].datum_zeit.std ;
  dsatz_winsol[0].min = dsatz_uvr1611[0].datum_zeit.min ;
  dsatz_winsol[0].sek = dsatz_uvr1611[0].datum_zeit.sek ;
  dsatz_winsol[0].ausgbyte1 = dsatz_uvr1611[0].ausgbyte1 ;
  dsatz_winsol[0].ausgbyte2 = dsatz_uvr1611[0].ausgbyte2 ;
  dsatz_winsol[0].dza[0] = dsatz_uvr1611[0].dza[0] ;
  dsatz_winsol[0].dza[1] = dsatz_uvr1611[0].dza[1] ;
  dsatz_winsol[0].dza[2] = dsatz_uvr1611[0].dza[2] ;
  dsatz_winsol[0].dza[3] = dsatz_uvr1611[0].dza[3] ;

  int ii=0;
  for (ii=0;ii<16;ii++)
    {
      dsatz_winsol[0].sensT[ii][0] = dsatz_uvr1611[0].sensT[ii][0] ;
      dsatz_winsol[0].sensT[ii][1] = dsatz_uvr1611[0].sensT[ii][1] ;
    }
  dsatz_winsol[0].wmzaehler_reg = dsatz_uvr1611[0].wmzaehler_reg ;

  if ( dsatz_uvr1611[0].mlstg1[3] > 0x7f ) /* negtive Wete */
  {
    byteslong_mlstg1.long_word = (10*((65536*(float)dsatz_uvr1611[0].mlstg1[3]
    +256*(float)dsatz_uvr1611[0].mlstg1[2]
    +(float)dsatz_uvr1611[0].mlstg1[1])-65536)
    -((float)dsatz_uvr1611[0].mlstg1[0]*10)/256);
    byteslong_mlstg1.long_word = byteslong_mlstg1.long_word | 0xffff0000;
  }
  else
    byteslong_mlstg1.long_word = (10*(65536*(float)dsatz_uvr1611[0].mlstg1[3]
    +256*(float)dsatz_uvr1611[0].mlstg1[2]
    +(float)dsatz_uvr1611[0].mlstg1[1])
    +((float)dsatz_uvr1611[0].mlstg1[0]*10)/256);

  dsatz_winsol[0].mlstg1[0] = byteslong_mlstg1.bytes.lowlowbyte ;
  dsatz_winsol[0].mlstg1[1] = byteslong_mlstg1.bytes.lowhighbyte ;
  dsatz_winsol[0].mlstg1[2] = byteslong_mlstg1.bytes.highlowbyte ;
  dsatz_winsol[0].mlstg1[3] = byteslong_mlstg1.bytes.highhighbyte ;
  dsatz_winsol[0].kwh1[0] = dsatz_uvr1611[0].kwh1[0] ;
  dsatz_winsol[0].kwh1[1] = dsatz_uvr1611[0].kwh1[1] ;
  dsatz_winsol[0].mwh1[0] = dsatz_uvr1611[0].mwh1[0] ;
  dsatz_winsol[0].mwh1[1] = dsatz_uvr1611[0].mwh1[1] ;

  if ( dsatz_uvr1611[0].mlstg2[3] > 0x7f ) /* negtive Wete */
  {
    byteslong_mlstg2.long_word = (10*((65536*(float)dsatz_uvr1611[0].mlstg2[3]
    +256*(float)dsatz_uvr1611[0].mlstg2[2]
    +(float)dsatz_uvr1611[0].mlstg2[1])-65536)
    -((float)dsatz_uvr1611[0].mlstg2[0]*10)/256);
    byteslong_mlstg2.long_word = byteslong_mlstg2.long_word | 0xffff0000;
  }
  else
    byteslong_mlstg2.long_word = (10*(65536*(float)dsatz_uvr1611[0].mlstg2[3]
    +256*(float)dsatz_uvr1611[0].mlstg2[2]
    +(float)dsatz_uvr1611[0].mlstg2[1])
    +((float)dsatz_uvr1611[0].mlstg2[0]*10)/256);

  dsatz_winsol[0].mlstg2[0] = byteslong_mlstg2.bytes.lowlowbyte ;
  dsatz_winsol[0].mlstg2[1] = byteslong_mlstg2.bytes.lowhighbyte ;
  dsatz_winsol[0].mlstg2[2] = byteslong_mlstg2.bytes.highlowbyte ;
  dsatz_winsol[0].mlstg2[3] = byteslong_mlstg2.bytes.highhighbyte ;
  dsatz_winsol[0].kwh2[0] = dsatz_uvr1611[0].kwh2[0] ;
  dsatz_winsol[0].kwh2[1] = dsatz_uvr1611[0].kwh2[1] ;
  dsatz_winsol[0].mwh2[0] = dsatz_uvr1611[0].mwh2[0] ;
  dsatz_winsol[0].mwh2[1] = dsatz_uvr1611[0].mwh2[1] ;

  return 1;
}
// CAN_BC-------------------------------------------------------------------------------------------------------------
/* Kopieren der Daten (UVR1611 - CAN-Logging) in die Winsol-Format Struktur */
int copy_UVR2winsol_1611_CANBC(s_DS_CANBC *dsatz_uvr1611_canbc, DS_CANBC  *dsatz_winsol_canbc )
{
  BYTES_LONG byteslong_mlstg1, byteslong_mlstg2, byteslong_mlstg3;

  dsatz_winsol_canbc[0].tag = dsatz_uvr1611_canbc[0].datum_zeit.tag ;
  dsatz_winsol_canbc[0].std = dsatz_uvr1611_canbc[0].datum_zeit.std ;
  dsatz_winsol_canbc[0].min = dsatz_uvr1611_canbc[0].datum_zeit.min ;
  dsatz_winsol_canbc[0].sek = dsatz_uvr1611_canbc[0].datum_zeit.sek ;
  dsatz_winsol_canbc[0].digbyte1 = dsatz_uvr1611_canbc[0].digbyte1 ;
  dsatz_winsol_canbc[0].digbyte2 = dsatz_uvr1611_canbc[0].digbyte2 ;

  int ii=0;
  for (ii=0;ii<12;ii++)
    {
      dsatz_winsol_canbc[0].sensT[ii][0] = dsatz_uvr1611_canbc[0].sensT[ii][0] ;
      dsatz_winsol_canbc[0].sensT[ii][1] = dsatz_uvr1611_canbc[0].sensT[ii][1] ;
    }
  dsatz_winsol_canbc[0].wmzaehler_reg = dsatz_uvr1611_canbc[0].wmzaehler_reg ;

  if ( dsatz_uvr1611_canbc[0].mlstg1[3] > 0x7f ) /* negtive Wete */
  {
    byteslong_mlstg1.long_word = (10*((65536*(float)dsatz_uvr1611_canbc[0].mlstg1[3]
    +256*(float)dsatz_uvr1611_canbc[0].mlstg1[2]
    +(float)dsatz_uvr1611_canbc[0].mlstg1[1])-65536)
    -((float)dsatz_uvr1611_canbc[0].mlstg1[0]*10)/256);
    byteslong_mlstg1.long_word = byteslong_mlstg1.long_word | 0xffff0000;
  }
  else
    byteslong_mlstg1.long_word = (10*(65536*(float)dsatz_uvr1611_canbc[0].mlstg1[3]
    +256*(float)dsatz_uvr1611_canbc[0].mlstg1[2]
    +(float)dsatz_uvr1611_canbc[0].mlstg1[1])
    +((float)dsatz_uvr1611_canbc[0].mlstg1[0]*10)/256);

  dsatz_winsol_canbc[0].mlstg1[0] = byteslong_mlstg1.bytes.lowlowbyte ;
  dsatz_winsol_canbc[0].mlstg1[1] = byteslong_mlstg1.bytes.lowhighbyte ;
  dsatz_winsol_canbc[0].mlstg1[2] = byteslong_mlstg1.bytes.highlowbyte ;
  dsatz_winsol_canbc[0].mlstg1[3] = byteslong_mlstg1.bytes.highhighbyte ;
  dsatz_winsol_canbc[0].kwh1[0] = dsatz_uvr1611_canbc[0].kwh1[0] ;
  dsatz_winsol_canbc[0].kwh1[1] = dsatz_uvr1611_canbc[0].kwh1[1] ;
  dsatz_winsol_canbc[0].mwh1[0] = dsatz_uvr1611_canbc[0].mwh1[0] ;
  dsatz_winsol_canbc[0].mwh1[1] = dsatz_uvr1611_canbc[0].mwh1[1] ;

  if ( dsatz_uvr1611_canbc[0].mlstg2[3] > 0x7f ) /* negtive Wete */
  {
    byteslong_mlstg2.long_word = (10*((65536*(float)dsatz_uvr1611_canbc[0].mlstg2[3]
    +256*(float)dsatz_uvr1611_canbc[0].mlstg2[2]
    +(float)dsatz_uvr1611_canbc[0].mlstg2[1])-65536)
    -((float)dsatz_uvr1611_canbc[0].mlstg2[0]*10)/256);
    byteslong_mlstg2.long_word = byteslong_mlstg2.long_word | 0xffff0000;
  }
  else
    byteslong_mlstg2.long_word = (10*(65536*(float)dsatz_uvr1611_canbc[0].mlstg2[3]
    +256*(float)dsatz_uvr1611_canbc[0].mlstg2[2]
    +(float)dsatz_uvr1611_canbc[0].mlstg2[1])
    +((float)dsatz_uvr1611_canbc[0].mlstg2[0]*10)/256);

  dsatz_winsol_canbc[0].mlstg2[0] = byteslong_mlstg2.bytes.lowlowbyte ;
  dsatz_winsol_canbc[0].mlstg2[1] = byteslong_mlstg2.bytes.lowhighbyte ;
  dsatz_winsol_canbc[0].mlstg2[2] = byteslong_mlstg2.bytes.highlowbyte ;
  dsatz_winsol_canbc[0].mlstg2[3] = byteslong_mlstg2.bytes.highhighbyte ;
  dsatz_winsol_canbc[0].kwh2[0] = dsatz_uvr1611_canbc[0].kwh2[0] ;
  dsatz_winsol_canbc[0].kwh2[1] = dsatz_uvr1611_canbc[0].kwh2[1] ;
  dsatz_winsol_canbc[0].mwh2[0] = dsatz_uvr1611_canbc[0].mwh2[0] ;
  dsatz_winsol_canbc[0].mwh2[1] = dsatz_uvr1611_canbc[0].mwh2[1] ;


  if ( dsatz_uvr1611_canbc[0].mlstg3[3] > 0x7f ) /* negtive Wete */
  {
    byteslong_mlstg3.long_word = (10*((65536*(float)dsatz_uvr1611_canbc[0].mlstg3[3]
    +256*(float)dsatz_uvr1611_canbc[0].mlstg3[2]
    +(float)dsatz_uvr1611_canbc[0].mlstg3[1])-65536)
    -((float)dsatz_uvr1611_canbc[0].mlstg3[0]*10)/256);
    byteslong_mlstg3.long_word = byteslong_mlstg3.long_word | 0xffff0000;
  }
  else
    byteslong_mlstg3.long_word = (10*(65536*(float)dsatz_uvr1611_canbc[0].mlstg3[3]
    +256*(float)dsatz_uvr1611_canbc[0].mlstg3[2]
    +(float)dsatz_uvr1611_canbc[0].mlstg3[1])
    +((float)dsatz_uvr1611_canbc[0].mlstg3[0]*10)/256);

  dsatz_winsol_canbc[0].mlstg3[0] = byteslong_mlstg3.bytes.lowlowbyte ;
  dsatz_winsol_canbc[0].mlstg3[1] = byteslong_mlstg3.bytes.lowhighbyte ;
  dsatz_winsol_canbc[0].mlstg3[2] = byteslong_mlstg3.bytes.highlowbyte ;
  dsatz_winsol_canbc[0].mlstg3[3] = byteslong_mlstg3.bytes.highhighbyte ;
  dsatz_winsol_canbc[0].kwh3[0] = dsatz_uvr1611_canbc[0].kwh3[0] ;
  dsatz_winsol_canbc[0].kwh3[1] = dsatz_uvr1611_canbc[0].kwh3[1] ;
  dsatz_winsol_canbc[0].mwh3[0] = dsatz_uvr1611_canbc[0].mwh3[0] ;
  dsatz_winsol_canbc[0].mwh3[1] = dsatz_uvr1611_canbc[0].mwh3[1] ;

  return 1;
}
/* Kopieren der Daten (UVR61-3) in die Winsol-Format Struktur */
int copy_UVR2winsol_61_3(u_DS_UVR1611_UVR61_3 *dsatz_uvr61_3, DS_Winsol_UVR61_3 *dsatz_winsol_uvr61_3 )
{
  dsatz_winsol_uvr61_3[0].tag = dsatz_uvr61_3[0].DS_UVR61_3.datum_zeit.tag ;
  dsatz_winsol_uvr61_3[0].std = dsatz_uvr61_3[0].DS_UVR61_3.datum_zeit.std ;
  dsatz_winsol_uvr61_3[0].min = dsatz_uvr61_3[0].DS_UVR61_3.datum_zeit.min ;
  dsatz_winsol_uvr61_3[0].sek = dsatz_uvr61_3[0].DS_UVR61_3.datum_zeit.sek ;
  dsatz_winsol_uvr61_3[0].ausgbyte1 = dsatz_uvr61_3[0].DS_UVR61_3.ausgbyte1 ;
  dsatz_winsol_uvr61_3[0].dza = dsatz_uvr61_3[0].DS_UVR61_3.dza ;
  dsatz_winsol_uvr61_3[0].ausg_analog = dsatz_uvr61_3[0].DS_UVR61_3.ausg_analog;

  int ii=0;
  for (ii=0;ii<6;ii++)
    {
      dsatz_winsol_uvr61_3[0].sensT[ii][0] = dsatz_uvr61_3[0].DS_UVR61_3.sensT[ii][0] ;
      dsatz_winsol_uvr61_3[0].sensT[ii][1] = dsatz_uvr61_3[0].DS_UVR61_3.sensT[ii][1] ;
    }
  dsatz_winsol_uvr61_3[0].wmzaehler_reg = dsatz_uvr61_3[0].DS_UVR61_3.wmzaehler_reg ;
  dsatz_winsol_uvr61_3[0].volstrom[0] = dsatz_uvr61_3[0].DS_UVR61_3.volstrom[0] ;
  dsatz_winsol_uvr61_3[0].volstrom[1] = dsatz_uvr61_3[0].DS_UVR61_3.volstrom[1] ;
  dsatz_winsol_uvr61_3[0].mlstg[0] = dsatz_uvr61_3[0].DS_UVR61_3.mlstg1[0] ;
  dsatz_winsol_uvr61_3[0].mlstg[1] = dsatz_uvr61_3[0].DS_UVR61_3.mlstg1[1] ;
  dsatz_winsol_uvr61_3[0].mlstg[2] = dsatz_uvr61_3[0].DS_UVR61_3.mlstg1[2] ;
  dsatz_winsol_uvr61_3[0].mlstg[3] = dsatz_uvr61_3[0].DS_UVR61_3.mlstg1[3] ;
  dsatz_winsol_uvr61_3[0].kwh[0] = dsatz_uvr61_3[0].DS_UVR61_3.kwh1[0] ;
  dsatz_winsol_uvr61_3[0].kwh[1] = dsatz_uvr61_3[0].DS_UVR61_3.kwh1[1] ;
  dsatz_winsol_uvr61_3[0].mwh[0] = dsatz_uvr61_3[0].DS_UVR61_3.mwh1[0] ;
  dsatz_winsol_uvr61_3[0].mwh[1] = dsatz_uvr61_3[0].DS_UVR61_3.mwh1[1] ;
  dsatz_winsol_uvr61_3[0].mwh[2] = dsatz_uvr61_3[0].DS_UVR61_3.mwh1[2] ;
  dsatz_winsol_uvr61_3[0].mwh[3] = dsatz_uvr61_3[0].DS_UVR61_3.mwh1[3] ;

  for (ii=0;ii<4;ii++)
    {
      dsatz_winsol_uvr61_3[0].unbenutzt1[ii] = 0x0;
    }
  for (ii=0;ii<25;ii++)
    {
      dsatz_winsol_uvr61_3[0].unbenutzt2[ii] = 0x0;
    }

  return 1;
}

/* Kopieren der Daten UVR1611(Modus 0xD1 - 2DL) in Winsol-Format-Struktur */
int copy_UVR2winsol_D1_1611(u_modus_D1 *dsatz_modus_d1, DS_Winsol *dsatz_winsol , int geraet)
{
  BYTES_LONG byteslong_mlstg1, byteslong_mlstg2;

  if ( geraet == 1 )
  {
    dsatz_winsol[0].tag = dsatz_modus_d1[0].DS_1611_1611.datum_zeit.tag ;
    dsatz_winsol[0].std = dsatz_modus_d1[0].DS_1611_1611.datum_zeit.std ;
    dsatz_winsol[0].min = dsatz_modus_d1[0].DS_1611_1611.datum_zeit.min ;
    dsatz_winsol[0].sek = dsatz_modus_d1[0].DS_1611_1611.datum_zeit.sek ;
    dsatz_winsol[0].ausgbyte1 = dsatz_modus_d1[0].DS_1611_1611.ausgbyte1 ;
    dsatz_winsol[0].ausgbyte2 = dsatz_modus_d1[0].DS_1611_1611.ausgbyte2 ;
    dsatz_winsol[0].dza[0] = dsatz_modus_d1[0].DS_1611_1611.dza[0] ;
    dsatz_winsol[0].dza[1] = dsatz_modus_d1[0].DS_1611_1611.dza[1] ;
    dsatz_winsol[0].dza[2] = dsatz_modus_d1[0].DS_1611_1611.dza[2] ;
    dsatz_winsol[0].dza[3] = dsatz_modus_d1[0].DS_1611_1611.dza[3] ;

    int ii=0;
    for (ii=0;ii<16;ii++)
    {
      dsatz_winsol[0].sensT[ii][0] = dsatz_modus_d1[0].DS_1611_1611.sensT[ii][0] ;
      dsatz_winsol[0].sensT[ii][1] = dsatz_modus_d1[0].DS_1611_1611.sensT[ii][1] ;
    }
    dsatz_winsol[0].wmzaehler_reg = dsatz_modus_d1[0].DS_1611_1611.wmzaehler_reg ;

    if ( dsatz_modus_d1[0].DS_1611_1611.mlstg1[3] > 0x7f ) /* negtive Wete */
    {
      byteslong_mlstg1.long_word = (10*((65536*(float)dsatz_modus_d1[0].DS_1611_1611.mlstg1[3]
      +256*(float)dsatz_modus_d1[0].DS_1611_1611.mlstg1[2]
      +(float)dsatz_modus_d1[0].DS_1611_1611.mlstg1[1])-65536)
      -((float)dsatz_modus_d1[0].DS_1611_1611.mlstg1[0]*10)/256);
      byteslong_mlstg1.long_word = byteslong_mlstg1.long_word | 0xffff0000;
    }
    else
      byteslong_mlstg1.long_word = (10*(65536*(float)dsatz_modus_d1[0].DS_1611_1611.mlstg1[3]
      +256*(float)dsatz_modus_d1[0].DS_1611_1611.mlstg1[2]
      +(float)dsatz_modus_d1[0].DS_1611_1611.mlstg1[1])
      +((float)dsatz_modus_d1[0].DS_1611_1611.mlstg1[0]*10)/256);

    dsatz_winsol[0].mlstg1[0] = byteslong_mlstg1.bytes.lowlowbyte ;
    dsatz_winsol[0].mlstg1[1] = byteslong_mlstg1.bytes.lowhighbyte ;
    dsatz_winsol[0].mlstg1[2] = byteslong_mlstg1.bytes.highlowbyte ;
    dsatz_winsol[0].mlstg1[3] = byteslong_mlstg1.bytes.highhighbyte ;
    dsatz_winsol[0].kwh1[0] = dsatz_modus_d1[0].DS_1611_1611.kwh1[0] ;
    dsatz_winsol[0].kwh1[1] = dsatz_modus_d1[0].DS_1611_1611.kwh1[1] ;
    dsatz_winsol[0].mwh1[0] = dsatz_modus_d1[0].DS_1611_1611.mwh1[0] ;
    dsatz_winsol[0].mwh1[1] = dsatz_modus_d1[0].DS_1611_1611.mwh1[1] ;

    if ( dsatz_modus_d1[0].DS_1611_1611.mlstg2[3] > 0x7f ) /* negtive Wete */
    {
      byteslong_mlstg2.long_word = (10*((65536*(float)dsatz_modus_d1[0].DS_1611_1611.mlstg2[3]
      +256*(float)dsatz_modus_d1[0].DS_1611_1611.mlstg2[2]
      +(float)dsatz_modus_d1[0].DS_1611_1611.mlstg2[1])-65536)
      -((float)dsatz_modus_d1[0].DS_1611_1611.mlstg2[0]*10)/256);
      byteslong_mlstg2.long_word = byteslong_mlstg2.long_word | 0xffff0000;
    }
    else
      byteslong_mlstg2.long_word = (10*(65536*(float)dsatz_modus_d1[0].DS_1611_1611.mlstg2[3]
      +256*(float)dsatz_modus_d1[0].DS_1611_1611.mlstg2[2]
      +(float)dsatz_modus_d1[0].DS_1611_1611.mlstg2[1])
      +((float)dsatz_modus_d1[0].DS_1611_1611.mlstg2[0]*10)/256);

    dsatz_winsol[0].mlstg2[0] = byteslong_mlstg2.bytes.lowlowbyte ;
    dsatz_winsol[0].mlstg2[1] = byteslong_mlstg2.bytes.lowhighbyte ;
    dsatz_winsol[0].mlstg2[2] = byteslong_mlstg2.bytes.highlowbyte ;
    dsatz_winsol[0].mlstg2[3] = byteslong_mlstg2.bytes.highhighbyte ;
    dsatz_winsol[0].kwh2[0] = dsatz_modus_d1[0].DS_1611_1611.kwh2[0] ;
    dsatz_winsol[0].kwh2[1] = dsatz_modus_d1[0].DS_1611_1611.kwh2[1] ;
    dsatz_winsol[0].mwh2[0] = dsatz_modus_d1[0].DS_1611_1611.mwh2[0] ;
    dsatz_winsol[0].mwh2[1] = dsatz_modus_d1[0].DS_1611_1611.mwh2[1] ;
  }
  else if (uvr_typ == UVR1611)
  {
    dsatz_winsol[0].tag = dsatz_modus_d1[0].DS_1611_1611.Z_datum_zeit.tag ;
    dsatz_winsol[0].std = dsatz_modus_d1[0].DS_1611_1611.Z_datum_zeit.std ;
    dsatz_winsol[0].min = dsatz_modus_d1[0].DS_1611_1611.Z_datum_zeit.min ;
    dsatz_winsol[0].sek = dsatz_modus_d1[0].DS_1611_1611.Z_datum_zeit.sek ;
    dsatz_winsol[0].ausgbyte1 = dsatz_modus_d1[0].DS_1611_1611.Z_ausgbyte1 ;
    dsatz_winsol[0].ausgbyte2 = dsatz_modus_d1[0].DS_1611_1611.Z_ausgbyte2 ;
    dsatz_winsol[0].dza[0] = dsatz_modus_d1[0].DS_1611_1611.Z_dza[0] ;
    dsatz_winsol[0].dza[1] = dsatz_modus_d1[0].DS_1611_1611.Z_dza[1] ;
    dsatz_winsol[0].dza[2] = dsatz_modus_d1[0].DS_1611_1611.Z_dza[2] ;
    dsatz_winsol[0].dza[3] = dsatz_modus_d1[0].DS_1611_1611.Z_dza[3] ;

    int ii=0;
    for (ii=0;ii<16;ii++)
    {
      dsatz_winsol[0].sensT[ii][0] = dsatz_modus_d1[0].DS_1611_1611.Z_sensT[ii][0] ;
      dsatz_winsol[0].sensT[ii][1] = dsatz_modus_d1[0].DS_1611_1611.Z_sensT[ii][1] ;
    }
    dsatz_winsol[0].wmzaehler_reg = dsatz_modus_d1[0].DS_1611_1611.Z_wmzaehler_reg ;

    if ( dsatz_modus_d1[0].DS_1611_1611.Z_mlstg1[3] > 0x7f ) /* negtive Wete */
    {
      byteslong_mlstg1.long_word = (10*((65536*(float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg1[3]
      +256*(float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg1[2]
      +(float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg1[1])-65536)
      -((float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg1[0]*10)/256);
      byteslong_mlstg1.long_word = byteslong_mlstg1.long_word | 0xffff0000;
    }
    else
      byteslong_mlstg1.long_word = (10*(65536*(float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg1[3]
      +256*(float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg1[2]
      +(float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg1[1])
      +((float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg1[0]*10)/256);

    dsatz_winsol[0].mlstg1[0] = byteslong_mlstg1.bytes.lowlowbyte ;
    dsatz_winsol[0].mlstg1[1] = byteslong_mlstg1.bytes.lowhighbyte ;
    dsatz_winsol[0].mlstg1[2] = byteslong_mlstg1.bytes.highlowbyte ;
    dsatz_winsol[0].mlstg1[3] = byteslong_mlstg1.bytes.highhighbyte ;
    dsatz_winsol[0].kwh1[0] = dsatz_modus_d1[0].DS_1611_1611.Z_kwh1[0] ;
    dsatz_winsol[0].kwh1[1] = dsatz_modus_d1[0].DS_1611_1611.Z_kwh1[1] ;
    dsatz_winsol[0].mwh1[0] = dsatz_modus_d1[0].DS_1611_1611.Z_mwh1[0] ;
    dsatz_winsol[0].mwh1[1] = dsatz_modus_d1[0].DS_1611_1611.Z_mwh1[1] ;

    if ( dsatz_modus_d1[0].DS_1611_1611.Z_mlstg2[3] > 0x7f ) /* negtive Wete */
    {
      byteslong_mlstg2.long_word = (10*((65536*(float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg2[3]
      +256*(float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg2[2]
      +(float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg2[1])-65536)
      -((float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg2[0]*10)/256);
      byteslong_mlstg2.long_word = byteslong_mlstg2.long_word | 0xffff0000;
    }
    else
      byteslong_mlstg2.long_word = (10*(65536*(float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg2[3]
      +256*(float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg2[2]
      +(float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg2[1])
      +((float)dsatz_modus_d1[0].DS_1611_1611.Z_mlstg2[0]*10)/256);

    dsatz_winsol[0].mlstg2[0] = byteslong_mlstg2.bytes.lowlowbyte ;
    dsatz_winsol[0].mlstg2[1] = byteslong_mlstg2.bytes.lowhighbyte ;
    dsatz_winsol[0].mlstg2[2] = byteslong_mlstg2.bytes.highlowbyte ;
    dsatz_winsol[0].mlstg2[3] = byteslong_mlstg2.bytes.highhighbyte ;
    dsatz_winsol[0].kwh2[0] = dsatz_modus_d1[0].DS_1611_1611.Z_kwh2[0] ;
    dsatz_winsol[0].kwh2[1] = dsatz_modus_d1[0].DS_1611_1611.Z_kwh2[1] ;
    dsatz_winsol[0].mwh2[0] = dsatz_modus_d1[0].DS_1611_1611.Z_mwh2[0] ;
    dsatz_winsol[0].mwh2[1] = dsatz_modus_d1[0].DS_1611_1611.Z_mwh2[1] ;
  }
  else if (uvr_typ == UVR61_3)
  {
    dsatz_winsol[0].tag = dsatz_modus_d1[0].DS_61_3_1611.Z_datum_zeit.tag ;
    dsatz_winsol[0].std = dsatz_modus_d1[0].DS_61_3_1611.Z_datum_zeit.std ;
    dsatz_winsol[0].min = dsatz_modus_d1[0].DS_61_3_1611.Z_datum_zeit.min ;
    dsatz_winsol[0].sek = dsatz_modus_d1[0].DS_61_3_1611.Z_datum_zeit.sek ;
    dsatz_winsol[0].ausgbyte1 = dsatz_modus_d1[0].DS_61_3_1611.Z_ausgbyte1 ;
    dsatz_winsol[0].ausgbyte2 = dsatz_modus_d1[0].DS_61_3_1611.Z_ausgbyte2 ;
    dsatz_winsol[0].dza[0] = dsatz_modus_d1[0].DS_61_3_1611.Z_dza[0] ;
    dsatz_winsol[0].dza[1] = dsatz_modus_d1[0].DS_61_3_1611.Z_dza[1] ;
    dsatz_winsol[0].dza[2] = dsatz_modus_d1[0].DS_61_3_1611.Z_dza[2] ;
    dsatz_winsol[0].dza[3] = dsatz_modus_d1[0].DS_61_3_1611.Z_dza[3] ;

    int ii=0;
    for (ii=0;ii<16;ii++)
    {
      dsatz_winsol[0].sensT[ii][0] = dsatz_modus_d1[0].DS_61_3_1611.Z_sensT[ii][0] ;
      dsatz_winsol[0].sensT[ii][1] = dsatz_modus_d1[0].DS_61_3_1611.Z_sensT[ii][1] ;
    }
    dsatz_winsol[0].wmzaehler_reg = dsatz_modus_d1[0].DS_61_3_1611.Z_wmzaehler_reg ;

    if ( dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg1[3] > 0x7f ) /* negtive Wete */
    {
      byteslong_mlstg1.long_word = (10*((65536*(float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg1[3]
      +256*(float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg1[2]
      +(float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg1[1])-65536)
      -((float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg1[0]*10)/256);
      byteslong_mlstg1.long_word = byteslong_mlstg1.long_word | 0xffff0000;
    }
    else
      byteslong_mlstg1.long_word = (10*(65536*(float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg1[3]
      +256*(float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg1[2]
      +(float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg1[1])
      +((float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg1[0]*10)/256);

    dsatz_winsol[0].mlstg1[0] = byteslong_mlstg1.bytes.lowlowbyte ;
    dsatz_winsol[0].mlstg1[1] = byteslong_mlstg1.bytes.lowhighbyte ;
    dsatz_winsol[0].mlstg1[2] = byteslong_mlstg1.bytes.highlowbyte ;
    dsatz_winsol[0].mlstg1[3] = byteslong_mlstg1.bytes.highhighbyte ;
    dsatz_winsol[0].kwh1[0] = dsatz_modus_d1[0].DS_61_3_1611.Z_kwh1[0] ;
    dsatz_winsol[0].kwh1[1] = dsatz_modus_d1[0].DS_61_3_1611.Z_kwh1[1] ;
    dsatz_winsol[0].mwh1[0] = dsatz_modus_d1[0].DS_61_3_1611.Z_mwh1[0] ;
    dsatz_winsol[0].mwh1[1] = dsatz_modus_d1[0].DS_61_3_1611.Z_mwh1[1] ;

    if ( dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg2[3] > 0x7f ) /* negtive Wete */
    {
      byteslong_mlstg2.long_word = (10*((65536*(float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg2[3]
      +256*(float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg2[2]
      +(float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg2[1])-65536)
      -((float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg2[0]*10)/256);
      byteslong_mlstg2.long_word = byteslong_mlstg2.long_word | 0xffff0000;
    }
    else
      byteslong_mlstg2.long_word = (10*(65536*(float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg2[3]
      +256*(float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg2[2]
      +(float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg2[1])
      +((float)dsatz_modus_d1[0].DS_61_3_1611.Z_mlstg2[0]*10)/256);

    dsatz_winsol[0].mlstg2[0] = byteslong_mlstg2.bytes.lowlowbyte ;
    dsatz_winsol[0].mlstg2[1] = byteslong_mlstg2.bytes.lowhighbyte ;
    dsatz_winsol[0].mlstg2[2] = byteslong_mlstg2.bytes.highlowbyte ;
    dsatz_winsol[0].mlstg2[3] = byteslong_mlstg2.bytes.highhighbyte ;
    dsatz_winsol[0].kwh2[0] = dsatz_modus_d1[0].DS_61_3_1611.Z_kwh2[0] ;
    dsatz_winsol[0].kwh2[1] = dsatz_modus_d1[0].DS_61_3_1611.Z_kwh2[1] ;
    dsatz_winsol[0].mwh2[0] = dsatz_modus_d1[0].DS_61_3_1611.Z_mwh2[0] ;
    dsatz_winsol[0].mwh2[1] = dsatz_modus_d1[0].DS_61_3_1611.Z_mwh2[1] ;
  }

  return 1;
}


/* Kopieren der Daten UVR61-3(Modus 0xD1 - 2DL) in Winsol-Format-Struktur */
int copy_UVR2winsol_D1_61_3(u_modus_D1 *dsatz_modus_d1, DS_Winsol_UVR61_3 *dsatz_winsol_uvr61_3 , int geraet)
{
  if ( geraet == 1 )
  {
    dsatz_winsol_uvr61_3[0].tag = dsatz_modus_d1[0].DS_61_3_61_3.datum_zeit.tag ;
    dsatz_winsol_uvr61_3[0].std = dsatz_modus_d1[0].DS_61_3_61_3.datum_zeit.std ;
    dsatz_winsol_uvr61_3[0].min = dsatz_modus_d1[0].DS_61_3_61_3.datum_zeit.min ;
    dsatz_winsol_uvr61_3[0].sek = dsatz_modus_d1[0].DS_61_3_61_3.datum_zeit.sek ;
    dsatz_winsol_uvr61_3[0].ausgbyte1 = dsatz_modus_d1[0].DS_61_3_61_3.ausgbyte1 ;
    dsatz_winsol_uvr61_3[0].dza = dsatz_modus_d1[0].DS_61_3_61_3.dza ;
    dsatz_winsol_uvr61_3[0].ausg_analog = dsatz_modus_d1[0].DS_61_3_61_3.ausg_analog;

    int ii=0;
    for (ii=0;ii<6;ii++)
    {
      dsatz_winsol_uvr61_3[0].sensT[ii][0] = dsatz_modus_d1[0].DS_61_3_61_3.sensT[ii][0] ;
      dsatz_winsol_uvr61_3[0].sensT[ii][1] = dsatz_modus_d1[0].DS_61_3_61_3.sensT[ii][1] ;
    }
    dsatz_winsol_uvr61_3[0].wmzaehler_reg = dsatz_modus_d1[0].DS_61_3_61_3.wmzaehler_reg ;
    dsatz_winsol_uvr61_3[0].volstrom[0] = dsatz_modus_d1[0].DS_61_3_61_3.volstrom[0] ;
    dsatz_winsol_uvr61_3[0].volstrom[1] = dsatz_modus_d1[0].DS_61_3_61_3.volstrom[1] ;
    dsatz_winsol_uvr61_3[0].mlstg[0] = dsatz_modus_d1[0].DS_61_3_61_3.mlstg1[0] ;
    dsatz_winsol_uvr61_3[0].mlstg[1] = dsatz_modus_d1[0].DS_61_3_61_3.mlstg1[1] ;
    dsatz_winsol_uvr61_3[0].mlstg[2] = dsatz_modus_d1[0].DS_61_3_61_3.mlstg1[2] ;
    dsatz_winsol_uvr61_3[0].mlstg[3] = dsatz_modus_d1[0].DS_61_3_61_3.mlstg1[3] ;
    dsatz_winsol_uvr61_3[0].kwh[0] = dsatz_modus_d1[0].DS_61_3_61_3.kwh1[0] ;
    dsatz_winsol_uvr61_3[0].kwh[1] = dsatz_modus_d1[0].DS_61_3_61_3.kwh1[1] ;
    dsatz_winsol_uvr61_3[0].mwh[0] = dsatz_modus_d1[0].DS_61_3_61_3.mwh1[0] ;
    dsatz_winsol_uvr61_3[0].mwh[1] = dsatz_modus_d1[0].DS_61_3_61_3.mwh1[1] ;
    dsatz_winsol_uvr61_3[0].mwh[2] = dsatz_modus_d1[0].DS_61_3_61_3.mwh1[2] ;
    dsatz_winsol_uvr61_3[0].mwh[3] = dsatz_modus_d1[0].DS_61_3_61_3.mwh1[3] ;

    for (ii=0;ii<4;ii++)
    {
      dsatz_winsol_uvr61_3[0].unbenutzt1[ii] = 0x0;
    }
    for (ii=0;ii<25;ii++)
    {
      dsatz_winsol_uvr61_3[0].unbenutzt2[ii] = 0x0;
    }
  }
  else if (uvr_typ == UVR61_3)
  {
    dsatz_winsol_uvr61_3[0].tag = dsatz_modus_d1[0].DS_61_3_61_3.Z_datum_zeit.tag ;
    dsatz_winsol_uvr61_3[0].std = dsatz_modus_d1[0].DS_61_3_61_3.Z_datum_zeit.std ;
    dsatz_winsol_uvr61_3[0].min = dsatz_modus_d1[0].DS_61_3_61_3.Z_datum_zeit.min ;
    dsatz_winsol_uvr61_3[0].sek = dsatz_modus_d1[0].DS_61_3_61_3.Z_datum_zeit.sek ;
    dsatz_winsol_uvr61_3[0].ausgbyte1 = dsatz_modus_d1[0].DS_61_3_61_3.Z_ausgbyte1 ;
    dsatz_winsol_uvr61_3[0].dza = dsatz_modus_d1[0].DS_61_3_61_3.Z_dza ;
    dsatz_winsol_uvr61_3[0].ausg_analog = dsatz_modus_d1[0].DS_61_3_61_3.Z_ausg_analog;

    int ii=0;
    for (ii=0;ii<6;ii++)
    {
      dsatz_winsol_uvr61_3[0].sensT[ii][0] = dsatz_modus_d1[0].DS_61_3_61_3.Z_sensT[ii][0] ;
      dsatz_winsol_uvr61_3[0].sensT[ii][1] = dsatz_modus_d1[0].DS_61_3_61_3.Z_sensT[ii][1] ;
    }
    dsatz_winsol_uvr61_3[0].wmzaehler_reg = dsatz_modus_d1[0].DS_61_3_61_3.Z_wmzaehler_reg ;
    dsatz_winsol_uvr61_3[0].volstrom[0] = dsatz_modus_d1[0].DS_61_3_61_3.Z_volstrom[0] ;
    dsatz_winsol_uvr61_3[0].volstrom[1] = dsatz_modus_d1[0].DS_61_3_61_3.Z_volstrom[1] ;
    dsatz_winsol_uvr61_3[0].mlstg[0] = dsatz_modus_d1[0].DS_61_3_61_3.Z_mlstg1[0] ;
    dsatz_winsol_uvr61_3[0].mlstg[1] = dsatz_modus_d1[0].DS_61_3_61_3.Z_mlstg1[1] ;
    dsatz_winsol_uvr61_3[0].mlstg[2] = dsatz_modus_d1[0].DS_61_3_61_3.Z_mlstg1[2] ;
    dsatz_winsol_uvr61_3[0].mlstg[3] = dsatz_modus_d1[0].DS_61_3_61_3.Z_mlstg1[3] ;
    dsatz_winsol_uvr61_3[0].kwh[0] = dsatz_modus_d1[0].DS_61_3_61_3.Z_kwh1[0] ;
    dsatz_winsol_uvr61_3[0].kwh[1] = dsatz_modus_d1[0].DS_61_3_61_3.Z_kwh1[1] ;
    dsatz_winsol_uvr61_3[0].mwh[0] = dsatz_modus_d1[0].DS_61_3_61_3.Z_mwh1[0] ;
    dsatz_winsol_uvr61_3[0].mwh[1] = dsatz_modus_d1[0].DS_61_3_61_3.Z_mwh1[1] ;
    dsatz_winsol_uvr61_3[0].mwh[2] = dsatz_modus_d1[0].DS_61_3_61_3.Z_mwh1[2] ;
    dsatz_winsol_uvr61_3[0].mwh[3] = dsatz_modus_d1[0].DS_61_3_61_3.Z_mwh1[3] ;

    for (ii=0;ii<4;ii++)
    {
      dsatz_winsol_uvr61_3[0].unbenutzt1[ii] = 0x0;
    }
    for (ii=0;ii<25;ii++)
    {
      dsatz_winsol_uvr61_3[0].unbenutzt2[ii] = 0x0;
    }
  }
  else if (uvr_typ == UVR1611)
  {
    dsatz_winsol_uvr61_3[0].tag = dsatz_modus_d1[0].DS_1611_61_3.Z_datum_zeit.tag ;
    dsatz_winsol_uvr61_3[0].std = dsatz_modus_d1[0].DS_1611_61_3.Z_datum_zeit.std ;
    dsatz_winsol_uvr61_3[0].min = dsatz_modus_d1[0].DS_1611_61_3.Z_datum_zeit.min ;
    dsatz_winsol_uvr61_3[0].sek = dsatz_modus_d1[0].DS_1611_61_3.Z_datum_zeit.sek ;
    dsatz_winsol_uvr61_3[0].ausgbyte1 = dsatz_modus_d1[0].DS_1611_61_3.Z_ausgbyte1 ;
    dsatz_winsol_uvr61_3[0].dza = dsatz_modus_d1[0].DS_1611_61_3.Z_dza ;
    dsatz_winsol_uvr61_3[0].ausg_analog = dsatz_modus_d1[0].DS_1611_61_3.Z_ausg_analog;

    int ii=0;
    for (ii=0;ii<6;ii++)
    {
      dsatz_winsol_uvr61_3[0].sensT[ii][0] = dsatz_modus_d1[0].DS_1611_61_3.Z_sensT[ii][0] ;
      dsatz_winsol_uvr61_3[0].sensT[ii][1] = dsatz_modus_d1[0].DS_1611_61_3.Z_sensT[ii][1] ;
    }
    dsatz_winsol_uvr61_3[0].wmzaehler_reg = dsatz_modus_d1[0].DS_1611_61_3.Z_wmzaehler_reg ;
    dsatz_winsol_uvr61_3[0].volstrom[0] = dsatz_modus_d1[0].DS_1611_61_3.Z_volstrom[0] ;
    dsatz_winsol_uvr61_3[0].volstrom[1] = dsatz_modus_d1[0].DS_1611_61_3.Z_volstrom[1] ;
    dsatz_winsol_uvr61_3[0].mlstg[0] = dsatz_modus_d1[0].DS_1611_61_3.Z_mlstg1[0] ;
    dsatz_winsol_uvr61_3[0].mlstg[1] = dsatz_modus_d1[0].DS_1611_61_3.Z_mlstg1[1] ;
    dsatz_winsol_uvr61_3[0].mlstg[2] = dsatz_modus_d1[0].DS_1611_61_3.Z_mlstg1[2] ;
    dsatz_winsol_uvr61_3[0].mlstg[3] = dsatz_modus_d1[0].DS_1611_61_3.Z_mlstg1[3] ;
    dsatz_winsol_uvr61_3[0].kwh[0] = dsatz_modus_d1[0].DS_1611_61_3.Z_kwh1[0] ;
    dsatz_winsol_uvr61_3[0].kwh[1] = dsatz_modus_d1[0].DS_1611_61_3.Z_kwh1[1] ;
    dsatz_winsol_uvr61_3[0].mwh[0] = dsatz_modus_d1[0].DS_1611_61_3.Z_mwh1[0] ;
    dsatz_winsol_uvr61_3[0].mwh[1] = dsatz_modus_d1[0].DS_1611_61_3.Z_mwh1[1] ;
    dsatz_winsol_uvr61_3[0].mwh[2] = dsatz_modus_d1[0].DS_1611_61_3.Z_mwh1[2] ;
    dsatz_winsol_uvr61_3[0].mwh[3] = dsatz_modus_d1[0].DS_1611_61_3.Z_mwh1[3] ;

    for (ii=0;ii<4;ii++)
    {
      dsatz_winsol_uvr61_3[0].unbenutzt1[ii] = 0x0;
    }
    for (ii=0;ii<25;ii++)
    {
      dsatz_winsol_uvr61_3[0].unbenutzt2[ii] = 0x0;
    }
  }

  return 1;
}

/* Ausgabe der Daten auf Bildschirm  */
void print_dsatz_uvr1611_content(const u_DS_UVR1611_UVR61_3 *dsatz_uvr1611)
{
  printf(" Datensatz start\n");
  printf("Zeitstempel: %X %X %X\n",dsatz_uvr1611[0].DS_UVR1611.zeitstempel[1],dsatz_uvr1611[0].DS_UVR1611.zeitstempel[2],dsatz_uvr1611[0].DS_UVR1611.zeitstempel[3]);

  printf("Zeit/Datum: %02d:%02d:%02d / %02d.%02d.200%d \n",
   dsatz_uvr1611[0].DS_UVR1611.datum_zeit.std,
   dsatz_uvr1611[0].DS_UVR1611.datum_zeit.min,
   dsatz_uvr1611[0].DS_UVR1611.datum_zeit.sek,
   dsatz_uvr1611[0].DS_UVR1611.datum_zeit.tag,
   dsatz_uvr1611[0].DS_UVR1611.datum_zeit.monat,
   dsatz_uvr1611[0].DS_UVR1611.datum_zeit.jahr);

  int ii=0;
  for(ii=0;ii<12;ii++)
    {
      int k=0;
      printf("Temperaturen ");
      for(k=0;k<4;k++)
  printf(" t%d: %x%X ",ii+k+1,
         dsatz_uvr1611[0].DS_UVR1611.sensT[ii+k][0],
         dsatz_uvr1611[0].DS_UVR1611.sensT[ii+k][1]);

      printf("\n Temperaturen ");
      for(k=0;k<4;k++)
  printf(" t%d: %g ",ii+k+1,
         berechnetemp(dsatz_uvr1611[0].DS_UVR1611.sensT[ii][0],
          dsatz_uvr1611[0].DS_UVR1611.sensT[ii][1]));
      printf("\n");
    }
  printf("Ausgangsbytes (1) %X, (2)%X\n",dsatz_uvr1611[0].DS_UVR1611.ausgbyte1,dsatz_uvr1611[0].DS_UVR1611.ausgbyte2);
  printf("Drehzahlstufen (1) %X, (2) %X, (6) %X, (7) %X\n",dsatz_uvr1611[0].DS_UVR1611.dza[0],dsatz_uvr1611[0].DS_UVR1611.dza[1],dsatz_uvr1611[0].DS_UVR1611.dza[2],dsatz_uvr1611[0].DS_UVR1611.dza[3]);
  printf("Waermemengenzaehler-bit: %X\n",dsatz_uvr1611[0].DS_UVR1611.wmzaehler_reg);
  printf("(1) Momentleistung %X%x%X%x, Kwh %X%x, MWh %X%x\n",dsatz_uvr1611[0].DS_UVR1611.mlstg1[1],dsatz_uvr1611[0].DS_UVR1611.mlstg1[2],dsatz_uvr1611[0].DS_UVR1611.mlstg1[3],dsatz_uvr1611[0].DS_UVR1611.mlstg1[4],dsatz_uvr1611[0].DS_UVR1611.kwh1[1],dsatz_uvr1611[0].DS_UVR1611.kwh1[2],dsatz_uvr1611[0].DS_UVR1611.mwh1[1],dsatz_uvr1611[0].DS_UVR1611.mwh1[2]);
  printf("(2) Momentleistung %X%x%X%x, Kwh %X%x, MWh %X%x\n",dsatz_uvr1611[0].DS_UVR1611.mlstg2[1],dsatz_uvr1611[0].DS_UVR1611.mlstg2[2],dsatz_uvr1611[0].DS_UVR1611.mlstg2[3],dsatz_uvr1611[0].DS_UVR1611.mlstg2[4],dsatz_uvr1611[0].DS_UVR1611.kwh2[1],dsatz_uvr1611[0].DS_UVR1611.kwh2[2],dsatz_uvr1611[0].DS_UVR1611.mwh2[1],dsatz_uvr1611[0].DS_UVR1611.mwh2[2]);

  printf("Zeit/Datum: %02d:%02d:%02d / %02d.%02d.200%d \n",
   dsatz_uvr1611[0].DS_UVR1611.datum_zeit.std,
   dsatz_uvr1611[0].DS_UVR1611.datum_zeit.min,
   dsatz_uvr1611[0].DS_UVR1611.datum_zeit.sek,
   dsatz_uvr1611[0].DS_UVR1611.datum_zeit.tag,
   dsatz_uvr1611[0].DS_UVR1611.datum_zeit.monat,
   dsatz_uvr1611[0].DS_UVR1611.datum_zeit.jahr);

  printf("Zeitstempel: %X %X %X\n",dsatz_uvr1611[0].DS_UVR1611.zeitstempel[1],dsatz_uvr1611[0].DS_UVR1611.zeitstempel[2],dsatz_uvr1611[0].DS_UVR1611.zeitstempel[3]);
  printf(" Datensatz ende\n");
}

/* Daten vom DL lesen - 1DL-Modus */
int datenlesen_A8(int anz_datensaetze)
{
  unsigned modTeiler;
  int i, merk_i, fehlerhafte_ds, result, lowbyte, middlebyte, merkmiddlebyte, tmp_erg = 0;
  int Bytes_for_0xA8 = 65, monatswechsel = 0, in_bytes=0;
  u_DS_UVR1611_UVR61_3 u_dsatz_uvr[1];
  DS_Winsol dsatz_winsol[1];
  DS_Winsol *puffer_dswinsol = &dsatz_winsol[0];
  DS_Winsol_UVR61_3 dsatz_winsol_uvr61_3[1];
  DS_Winsol_UVR61_3 *puffer_dswinsol_uvr61_3 = &dsatz_winsol_uvr61_3[0];
  UCHAR pruefsumme = 0, merk_monat = 0;
  UCHAR sendbuf[6];       /*  sendebuffer fuer die Request-Commandos*/
  UCHAR empfbuf[6];

  modTeiler = 0x100;
  i = 0; /* Gesamtdurchlaufzaehler mit 0 initialisiert */
  merk_i = 0; /* Bei falscher Pruefziffer den Datensatz bis zu fuenfmal wiederholt lesen */
  fehlerhafte_ds = 0; /* Anzahl fehlerhaft gelesener Datensaetze mit 0 initialisiert */
  lowbyte = 0;
  middlebyte = 0;
  merkmiddlebyte = middlebyte;

  sendbuf[0]=VERSIONSABFRAGE;   /* Senden der Versionsabfrage */
  if (usb_zugriff)
  {
    close_usb();
    init_usb();
    fd_set rfds;
    struct timeval tv;
    int retval=0;
    int retry=0;
    int retry_interval=2; 

    write_erg=write(fd,sendbuf,1);
    if (write_erg == 1)    /* Lesen der Antwort*/
    {
      do
      {
        in_bytes=0;
        FD_ZERO(&rfds);  /* muss jedes Mal gesetzt werden */
        FD_SET(fd, &rfds);
        tv.tv_sec = retry_interval; /* Wait up to five seconds. */
        tv.tv_usec = 0;
        retval = select(fd+1, &rfds, NULL, NULL, &tv);
        zeitstempel();
        if (retval == -1)
          perror("select(fd)");
        else if (retval)
        {
#ifdef DEBUG
          fprintf(stderr,"Data is available now. %d.%d\n",(int)tv.tv_sec,(int)tv.tv_usec);
#endif
          if (FD_ISSET(fd,&rfds))
          {
            ioctl(fd, FIONREAD, &in_bytes);
#ifdef DEBUG
		     fprintf(stderr,"Bytes im Puffer: %d\n",in_bytes);
#endif
            if (in_bytes == 1)
            {
              result=read(fd,empfbuf,1);
              retry=4;
            }
          }
        }
      } while( retry < 3 && in_bytes != 0);
    }
  }
  
  if (ip_zugriff)
  {
    if (!ip_first)
    {
      sock = socket(PF_INET, SOCK_STREAM, 0);
      if (sock == -1)
      {
        perror("socket failed()");
        do_cleanup();
        return 2;
      }
      if (connect(sock, (const struct sockaddr *)&SERVER_sockaddr_in, sizeof(SERVER_sockaddr_in)) == -1)
      {
        perror("connect failed()");
        do_cleanup();
        return 3;
      }
    }  /* if (!ip_first) */
    write_erg=send(sock,sendbuf,1,0);
    if (write_erg == 1)    /* Lesen der Antwort */
      result  = recv(sock,empfbuf,1,0);
  }

  /* fuellen des Sendebuffer - 6 Byte */
  sendbuf[0] = DATENBEREICHLESEN;
  // Startadressen von Kopfsatzlesen:
  sendbuf[1] = *start_adresse;     /* Beginn des Datenbereiches (low vor high) */
  sendbuf[2] = *(start_adresse+1);
  sendbuf[3] = *(start_adresse+2);
  sendbuf[4] = 0x01;  /* Anzahl der zu lesenden Rahmen */

  switch (sendbuf[1])  // vorbelegen lowbyte bei Startadr. > 00 00 00
  {
    case 0x00: lowbyte = 0; break;
    case 0x40: lowbyte = 1; break;
    case 0x80: lowbyte = 2; break;
    case 0xc0: lowbyte = 3; break;
  }

 #if DEBUG
fprintf(stderr," Startadresse: %x %x %x\n",sendbuf[1],sendbuf[2],sendbuf[3]);
#endif
  for(;i<anz_datensaetze;i++)
  {
    sendbuf[5] = (sendbuf[0] + sendbuf[1] + sendbuf[2] + sendbuf[3] + sendbuf[4]) % modTeiler;  /* Pruefziffer */

    if (usb_zugriff)
    {
      fd_set rfds;
      struct timeval tv;
      int retval=0;
      int retry=0;
      int retry_interval=2;
      int durchlauf=0;
      write_erg=write(fd,sendbuf,6);
      if (write_erg == 6)    /* Lesen der Antwort*/
      {
        do
        {
          in_bytes=0;
          FD_ZERO(&rfds);  /* muss jedes Mal gesetzt werden */
          FD_SET(fd, &rfds);
          tv.tv_sec = retry_interval; /* Wait up to five seconds. */
          tv.tv_usec = 0;
          retval = select(fd+1, &rfds, NULL, NULL, &tv);
          zeitstempel();
          if (retval == -1)
            perror("select(fd)");
          else if (retval)
          {
#ifdef DEBUG
          fprintf(stderr,"Data is available now. %d.%d\n",(int)tv.tv_sec,(int)tv.tv_usec);
#endif
            if (FD_ISSET(fd,&rfds))
            {
              ioctl(fd, FIONREAD, &in_bytes);
#ifdef DEBUG
		     fprintf(stderr,"Bytes im Puffer: %d\n",in_bytes);
#endif
              if (in_bytes == 65)
              {
                result=read(fd, u_dsatz_uvr,Bytes_for_0xA8);
                retry=4;
#ifdef DEBUG
                fprintf(stderr,"%s - Daten beim %d. Zugriff gelesen.\n",sZeit,durchlauf);
#endif
              }
              else
                durchlauf++;
            }
          }
        } while( retry < 3 && in_bytes != 0);
      }
    }
    
    if (ip_zugriff)
    {
      if (!ip_first)
      {
        sock = socket(PF_INET, SOCK_STREAM, 0);
        if (sock == -1)
        {
          perror("socket failed()");
          do_cleanup();
          return 2;
        }
        if (connect(sock, (const struct sockaddr *)&SERVER_sockaddr_in, sizeof(SERVER_sockaddr_in)) == -1)
        {
          perror("connect failed()");
          do_cleanup();
          return 3;
        }
       }  /* if (!ip_first) */

       write_erg=send(sock,sendbuf,6,0);
       if (write_erg == 6)    /* Lesen der Antwort */
          result  = recv(sock, u_dsatz_uvr,Bytes_for_0xA8,0);
    } /* if (ip_zugriff) */

//**************************************************************************************** !!!!!!!!
    if (uvr_typ == UVR1611)
      pruefsumme = berechnepruefziffer_uvr1611(u_dsatz_uvr);
    if (uvr_typ == UVR61_3)
      pruefsumme = berechnepruefziffer_uvr61_3(u_dsatz_uvr);
#if DEBUG > 3
  if (uvr_typ == UVR1611)
      fprintf(stderr, "%d. Datensatz - Pruefsumme gelesen: %x  berechnet: %x \n",i+1,u_dsatz_uvr[0].DS_UVR1611.pruefsum,pruefsumme);
  if (uvr_typ == UVR61_3)
      fprintf(stderr, "%d. Datensatz - Pruefsumme gelesen: %x  berechnet: %x \n",i+1,u_dsatz_uvr[0].DS_UVR61_3.pruefsum,pruefsumme);
#endif

    if (u_dsatz_uvr[0].DS_UVR1611.pruefsum == pruefsumme || u_dsatz_uvr[0].DS_UVR61_3.pruefsum == pruefsumme)
    {  /*Aenderung: 02.09.06 - Hochzaehlen der Startadresse erst dann, wenn korrekt gelesen wurde (eventuell endlosschleife?) */
#if DEBUG > 4
    print_dsatz_uvr1611_content(u_dsatz_uvr);
#endif
      if ( i == 0 ) /* erster Datenssatz wurde gelesen - Logfile oeffnen / erstellen */
      {
        if (uvr_typ == UVR1611)
        {
          tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_UVR1611.datum_zeit.monat,u_dsatz_uvr[0].DS_UVR1611.datum_zeit.jahr));
          merk_monat = u_dsatz_uvr[0].DS_UVR1611.datum_zeit.monat;
        }
        if (uvr_typ == UVR61_3)
        {
          tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.monat,u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.jahr));
          merk_monat = u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.monat;
        }
        if ( tmp_erg == 0 )
        {
          printf("Der Logfile-Name konnte nicht erzeugt werden!");
          exit(-1);
        }
        else
        {
          if ( open_logfile(LogFileName[1], 1) == -1 )
          {
            printf("Das LogFile kann nicht geoeffnet werden!\n");
            exit(-1);
          }
        }
      }
      /* Hat der Monat gewechselt? Wenn ja, neues LogFile erstellen. */
      if ( uvr_typ == UVR1611 && merk_monat != u_dsatz_uvr[0].DS_UVR1611.datum_zeit.monat )
        monatswechsel = 1;
      if ( uvr_typ == UVR61_3 && merk_monat != u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.monat )
        monatswechsel = 1;

      if ( monatswechsel == 1 )
      {
        printf("Monatswechsel!\n");
        if ( close_logfile() == -1)
        {
          printf("Fehler beim Monatswechsel: Cannot close logfile!");
          exit(-1);
        }
        else
        {
          if (uvr_typ == UVR1611)
            tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_UVR1611.datum_zeit.monat,u_dsatz_uvr[0].DS_UVR1611.datum_zeit.jahr));
          if (uvr_typ == UVR61_3)
            tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.monat,u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.jahr));
          if ( tmp_erg == 0 )
          {
            printf("Fehler beim Monatswechsel: Der Logfile-Name konnte nicht erzeugt werden!");
            exit(-1);
          }
          else
          {
            if ( open_logfile(LogFileName[1], 1) == -1 )
            {
              printf("Fehler beim Monatswechsel: Das LogFile kann nicht geoeffnet werden!\n");
              exit(-1);
            }
          }
        }
      } /* Ende: if ( merk_monat != dsatz_uvr1611[0].datum_zeit.monat ) */

      if (uvr_typ == UVR1611)
        merk_monat = u_dsatz_uvr[0].DS_UVR1611.datum_zeit.monat;
// CAN_BC----------------------------------------------------------------------------------------------------------------
      if (uvr_typ == CAN_BC)
        merk_monat = u_dsatz_uvr[0].DS_UVR1611.datum_zeit.monat;

      if (uvr_typ == UVR61_3)
        merk_monat = u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.monat;

      if (uvr_typ == UVR1611)
        copy_UVR2winsol_1611( &u_dsatz_uvr[0], &dsatz_winsol[0] );
// CAN_BC----------------------------------------------------------------------------------------------------------------
      if (uvr_typ == CAN_BC)
        copy_UVR2winsol_1611( &u_dsatz_uvr[0], &dsatz_winsol[0] );

      if (uvr_typ == UVR61_3)
        copy_UVR2winsol_61_3( &u_dsatz_uvr[0], &dsatz_winsol_uvr61_3[0] );

      if ( csv==1 && fp_csvfile != NULL )
      {
        if (uvr_typ == UVR1611)
           writeWINSOLlogfile2CSV(fp_logfile, &dsatz_winsol[0],
           u_dsatz_uvr[0].DS_UVR1611.datum_zeit.jahr,
           u_dsatz_uvr[0].DS_UVR1611.datum_zeit.monat );
        if (uvr_typ == UVR61_3)
          writeWINSOLlogfile2CSV(fp_logfile, &dsatz_winsol[0],
           u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.jahr,
           u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.monat );
      }

      /* schreiben der gelesenen Rohdaten in das LogFile */
      if (uvr_typ == UVR1611)
        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile);
// CAN_BC-------------------------------------------------------------------------------------------------------------------
      if (uvr_typ == CAN_BC)
	  {
        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile);
#if DEBUG > 3
        printf("uvr_Typ CAN_BC\n");
#endif
		}
      if (uvr_typ == UVR61_3)
        tmp_erg = fwrite(puffer_dswinsol_uvr61_3,59,1,fp_logfile);

      if (usb_zugriff)
      {
        if ( ((i%20) == 0) && (i > 0) )
          printf("%d Datensaetze geschrieben.\n",i);
      }
      else if ( ((i%100) == 0) && (i > 0) )
        printf("%d Datensaetze geschrieben.\n",i);

      if ( *end_adresse == sendbuf[1] && *(end_adresse+1) == sendbuf[2] && *(end_adresse+2) == sendbuf[3] )
            break;

      /* Hochzaehlen der Startadressen                                      */
      if (lowbyte <= 2)
        lowbyte++;
      else
      {
        lowbyte = 0;
        middlebyte++;
      }

      switch (lowbyte)
      {
        case 0: sendbuf[1] = 0x00; break;
        case 1: sendbuf[1] = 0x40; break;
        case 2: sendbuf[1] = 0x80; break;
        case 3: sendbuf[1] = 0xc0; break;
      }

      if (middlebyte > merkmiddlebyte)   /* das mittlere Byte muss erhoeht werden */
      {
        if ( sendbuf[2] != 0xFE )
        {
          sendbuf[2] = sendbuf[2] + 0x02;
          merkmiddlebyte = middlebyte;
        }
        else /* das highbyte muss erhoeht werden */
        {
          sendbuf[2] = 0x00;
          sendbuf[3] = sendbuf[3] + 0x01;
          merkmiddlebyte = middlebyte;
        }
      }

          if (sendbuf[3] > 0x0F ) // "Speicherueberlauf" im BL-Net
          {
                sendbuf[1] = 0x00;
                sendbuf[2] = 0x00;
                sendbuf[3] = 0x00;
          }

      monatswechsel = 0;
    } /* Ende: if (dsatz_uvr1611[0].pruefsum == pruefsumme) */
    else
    {
      if (merk_i < 5)
      {
        i--; /* falsche Pruefziffer - also nochmals lesen */
#ifdef DEBUG
        fprintf(stderr, " falsche Pruefsumme - Versuch#%d\n",merk_i);
#endif
        merk_i++; /* hochzaehlen bis 5 */
      }
      else
      {
        merk_i = 0;
        fehlerhafte_ds++;
        printf ( " fehlerhafter1 Datensatz - insgesamt:%d\n",fehlerhafte_ds);
      }
    }
  }
  return i + 1 - fehlerhafte_ds;
}

/* Daten vom DL lesen - 2DL-Modus */
int datenlesen_D1(int anz_datensaetze)
{
  unsigned modTeiler;
  int i, merk_i, fehlerhafte_ds, result=0, lowbyte, middlebyte, merkmiddlebyte, tmp_erg = 0;
  int Bytes_for_0xD1 = 127, monatswechsel = 0, in_bytes=0;
  u_modus_D1 u_dsatz_uvr[1];

  DS_Winsol dsatz_winsol[1];
  DS_Winsol *puffer_dswinsol = &dsatz_winsol[0];
  DS_Winsol dsatz_winsol_2[1];
  DS_Winsol *puffer_dswinsol_2 = &dsatz_winsol_2[0];
  DS_Winsol_UVR61_3 dsatz_winsol_uvr61_3[1];
  DS_Winsol_UVR61_3 *puffer_dswinsol_uvr61_3 = &dsatz_winsol_uvr61_3[0];
  DS_Winsol_UVR61_3 dsatz_winsol_uvr61_3_2[1];
  DS_Winsol_UVR61_3 *puffer_dswinsol_uvr61_3_2 = &dsatz_winsol_uvr61_3_2[0];
  UCHAR pruefsumme = 0, merk_monat = 0;
  UCHAR sendbuf[6];       /*  sendebuffer fuer die Request-Commandos*/
  UCHAR empfbuf[6];

  modTeiler = 0x100;
  i = 0; /* Gesamtdurchlaufzaehler mit 0 initialisiert */
  merk_i = 0; /* Bei falscher Pruefziffer den Datensatz bis zu fuenfmal wiederholt lesen */
  fehlerhafte_ds = 0; /* Anzahl fehlerhaft gelesener Datensaetze mit 0 initialisiert */
  lowbyte = 0;
  middlebyte = 0;
  merkmiddlebyte = middlebyte;

  sendbuf[0]=VERSIONSABFRAGE;   /* Senden der Versionsabfrage */
  if (usb_zugriff)
  {
    close_usb();
    init_usb();
    fd_set rfds;
    struct timeval tv;
    int retval=0;
    int retry=0;
    int retry_interval=2; 
    
    write_erg=write(fd,sendbuf,1);
    if (write_erg == 1)    /* Lesen der Antwort*/
    {
      do
      {
        in_bytes=0;
        FD_ZERO(&rfds);  /* muss jedes Mal gesetzt werden */
        FD_SET(fd, &rfds);
        tv.tv_sec = retry_interval; /* Wait up to five seconds. */
        tv.tv_usec = 0;
        retval = select(fd+1, &rfds, NULL, NULL, &tv);
        zeitstempel();
        if (retval == -1)
          perror("select(fd)");
        else if (retval)
        {
#ifdef DEBUG
          fprintf(stderr,"Data is available now. %d.%d\n",(int)tv.tv_sec,(int)tv.tv_usec);
#endif
          if (FD_ISSET(fd,&rfds))
          {
            ioctl(fd, FIONREAD, &in_bytes);
#ifdef DEBUG
		     fprintf(stderr,"Bytes im Puffer: %d\n",in_bytes);
#endif
            if (in_bytes == 1)
            {
              result=read(fd,empfbuf,1);
              retry=4;
            }
          }
        }
      } while( retry < 3 && in_bytes != 0);
    }
  }
  if (ip_zugriff)
  {
    if (!ip_first)
    {
      sock = socket(PF_INET, SOCK_STREAM, 0);
      if (sock == -1)
      {
        perror("socket failed()");
        do_cleanup();
        return 2;
      }
      if (connect(sock, (const struct sockaddr *)&SERVER_sockaddr_in, sizeof(SERVER_sockaddr_in)) == -1)
      {
        perror("connect failed()");
        do_cleanup();
        return 3;
      }
    }  /* if (!ip_first) */
    write_erg=send(sock,sendbuf,1,0);
    if (write_erg == 1)    /* Lesen der Antwort */
      result  = recv(sock,empfbuf,1,0);
  }

  /* fuellen des Sendebuffer - 6 Byte */
  sendbuf[0] = DATENBEREICHLESEN;
  // Startadressen von Kopfsatzlesen:
  sendbuf[1] = *start_adresse;     /* Beginn des Datenbereiches (low vor high) */
  sendbuf[2] = *(start_adresse+1);
  sendbuf[3] = *(start_adresse+2);
  sendbuf[4] = 0x01;  /* Anzahl der zu lesenden Rahmen */

  switch (sendbuf[1])  // vorbelegen lowbyte bei Startadr. > 00 00 00
  {
    case 0x00: lowbyte = 0; break;
    case 0x80: lowbyte = 1; break;
  }

  for(;i<anz_datensaetze;i++)
  {
    sendbuf[5] = (sendbuf[0] + sendbuf[1] + sendbuf[2] + sendbuf[3] + sendbuf[4]) % modTeiler;  /* Pruefziffer */

    if (usb_zugriff)
    {
      fd_set rfds;
      struct timeval tv;
      int retval=0;
      int retry=0;
      int retry_interval=2; 
      write_erg=write(fd,sendbuf,6);
      if (write_erg == 6)    /* Lesen der Antwort*/
      {
         do
        {
          in_bytes=0;
          FD_ZERO(&rfds);  /* muss jedes Mal gesetzt werden */
          FD_SET(fd, &rfds);
          tv.tv_sec = retry_interval; /* Wait up to five seconds. */
          tv.tv_usec = 0;
          retval = select(fd+1, &rfds, NULL, NULL, &tv);
          zeitstempel();
          if (retval == -1)
            perror("select(fd)");
          else if (retval)
          {
#ifdef DEBUG
          fprintf(stderr,"Data is available now. %d.%d\n",(int)tv.tv_sec,(int)tv.tv_usec);
#endif
            if (FD_ISSET(fd,&rfds))
            {
              ioctl(fd, FIONREAD, &in_bytes);
#ifdef DEBUG
		     fprintf(stderr,"Bytes im Puffer: %d\n",in_bytes);
#endif
              if (in_bytes == Bytes_for_0xD1) /* 127 */
              {
                result=read(fd, u_dsatz_uvr,Bytes_for_0xD1);
                retry=4;
              }
            }
          }
        } while( retry < 3 && in_bytes != 0);
      }
    }
    if (ip_zugriff)
    {
      if (!ip_first)
      {
        sock = socket(PF_INET, SOCK_STREAM, 0);
        if (sock == -1)
        {
          perror("socket failed()");
          do_cleanup();
          return 2;
        }
        if (connect(sock, (const struct sockaddr *)&SERVER_sockaddr_in, sizeof(SERVER_sockaddr_in)) == -1)
        {
          perror("connect failed()");
          do_cleanup();
          return 3;
        }
       }  /* if (!ip_first) */

       write_erg=send(sock,sendbuf,6,0);
       if (write_erg == 6)    /* Lesen der Antwort */
          result  = recv(sock, u_dsatz_uvr,Bytes_for_0xD1,0);
    } /* if (ip_zugriff) */

//**************************************************************************************** !!!!!!!!
      pruefsumme = berechnepruefziffer_modus_D1( &u_dsatz_uvr[0], result );
#if DEBUG > 3
      printf("Pruefsumme berechnet: %x in Byte %d erhalten %x\n",pruefsumme,result,u_dsatz_uvr[0].DS_alles.all_bytes[result-1]);
#endif

    if (u_dsatz_uvr[0].DS_alles.all_bytes[result-1] == pruefsumme )
    {  /*Aenderung: 02.09.06 - Hochzaehlen der Startadresse erst dann, wenn korrekt gelesen wurde (eventuell endlosschleife?) */
#if DEBUG > 4
    print_dsatz_uvr1611_content(u_dsatz_uvr);
      int zz;
      for (zz=0;zz<result-1;zz++)
        fprintf(stderr, "%2x ",u_dsatz_uvr[0].DS_alles.all_bytes[zz]);
      fprintf(stderr, "\nuvr_typ(1): 0x%x uvr_typ(2): 0x%x \n",uvr_typ,uvr_typ2);
      fprintf(stderr, "%2x \n",u_dsatz_uvr[0].DS_alles.all_bytes[59]);
#endif
      if ( i == 0 ) /* erster Datenssatz wurde gelesen - Logfile oeffnen / erstellen */
      {
        if (uvr_typ == UVR1611)
        {
          tmp_erg = erzeugeLogfileName(u_dsatz_uvr[0].DS_1611_1611.datum_zeit.monat,u_dsatz_uvr[0].DS_1611_1611.datum_zeit.jahr);
          merk_monat = u_dsatz_uvr[0].DS_1611_1611.datum_zeit.monat;
        }
        if (uvr_typ == UVR61_3)
        {
          tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_61_3_61_3.datum_zeit.monat,u_dsatz_uvr[0].DS_61_3_61_3.datum_zeit.jahr));
          merk_monat = u_dsatz_uvr[0].DS_61_3_61_3.datum_zeit.monat;
        }

        if ( tmp_erg == 0 ) /* Das erste  */
        {
          printf("Der Logfile-Name (1) konnte nicht erzeugt werden!");
          exit(-1);
        }
        /* ********* 2. Geraet ********* */
        if (uvr_typ2 == UVR1611 && uvr_typ == UVR1611)
        {
          tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_1611_1611.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_1611_1611.Z_datum_zeit.jahr));
        }
        else if (uvr_typ2 == UVR1611 && uvr_typ == UVR61_3)
        {
          tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_61_3_1611.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_61_3_1611.Z_datum_zeit.jahr));
        }
        if (uvr_typ2 == UVR61_3 && uvr_typ == UVR61_3)
        {
          tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_61_3_61_3.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_61_3_61_3.Z_datum_zeit.jahr));
        }
        else if (uvr_typ2 == UVR61_3 && uvr_typ == UVR1611)
        {
          tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_1611_61_3.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_1611_61_3.Z_datum_zeit.jahr));
        }
        if ( tmp_erg == 0 )
        {
          printf("Der Logfile-Name (2) konnte nicht erzeugt werden!");
          exit(-1);
        }
        else
        {
          if ( open_logfile(LogFileName[1], 1) == -1 )
          {
            printf("Das LogFile kann nicht geoeffnet werden!\n");
            exit(-1);
          }
          if ( open_logfile(LogFileName[2], 2) == -1 )
          {
            printf("Das LogFile kann nicht geoeffnet werden!\n");
            exit(-1);
          }
        }
      }
      /* Hat der Monat gewechselt? Wenn ja, neues LogFile erstellen. */
      if ( uvr_typ == UVR1611 && merk_monat != u_dsatz_uvr[0].DS_1611_1611.datum_zeit.monat )
        monatswechsel = 1;
      if ( uvr_typ == UVR61_3 && merk_monat != u_dsatz_uvr[0].DS_61_3_61_3.datum_zeit.monat )
        monatswechsel = 1;

      if ( monatswechsel == 1 )
      {
        printf("Monatswechsel!\n");
        if ( close_logfile() == -1)
        {
          printf("Fehler beim Monatswechsel: Cannot close logfile!");
          exit(-1);
        }
        else
        {
          if (uvr_typ == UVR1611)
            tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_1611_1611.datum_zeit.monat,u_dsatz_uvr[0].DS_1611_1611.datum_zeit.jahr));
          if (uvr_typ == UVR61_3)
            tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_61_3_61_3.datum_zeit.monat,u_dsatz_uvr[0].DS_61_3_61_3.datum_zeit.jahr));

          /* ********* 2. Geraet ********* */
          if (uvr_typ2 == UVR1611 && uvr_typ == UVR1611)
            tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_1611_1611.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_1611_1611.Z_datum_zeit.jahr));
          else if (uvr_typ2 == UVR1611 && uvr_typ == UVR61_3)
            tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_61_3_1611.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_61_3_1611.Z_datum_zeit.jahr));

          if (uvr_typ2 == UVR61_3 && uvr_typ == UVR61_3)
            tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_61_3_61_3.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_61_3_61_3.Z_datum_zeit.jahr));
          else if (uvr_typ2 == UVR61_3 && uvr_typ == UVR1611)
            tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_1611_61_3.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_1611_61_3.Z_datum_zeit.jahr));

          if ( tmp_erg == 0 )
          {
            printf("Fehler beim Monatswechsel: Der Logfile-Name konnte nicht erzeugt werden!");
            exit(-1);
          }
          else
          {
            if ( open_logfile(LogFileName[1], 1) == -1 )
            {
              printf("Fehler beim Monatswechsel: Das LogFile kann nicht geoeffnet werden!\n");
              exit(-1);
            }
            if ( open_logfile(LogFileName[2], 2) == -1 )
            {
              printf("Das LogFile kann nicht geoeffnet werden!\n");
              exit(-1);
            }
          }
        }
      } /* Ende: if ( merk_monat != dsatz_uvr1611[0].datum_zeit.monat ) */

      if (uvr_typ == UVR1611)
        merk_monat = u_dsatz_uvr[0].DS_1611_1611.datum_zeit.monat;
      if (uvr_typ == UVR61_3)
        merk_monat = u_dsatz_uvr[0].DS_61_3_61_3.datum_zeit.monat;

      /* Daten in die Winsol-Struktur kopieren */
      if (uvr_typ == UVR1611)
        copy_UVR2winsol_D1_1611( &u_dsatz_uvr[0], &dsatz_winsol[0], 1 );
      if (uvr_typ == UVR61_3)
        copy_UVR2winsol_D1_61_3( &u_dsatz_uvr[0], &dsatz_winsol_uvr61_3[0], 1);
      /* schreiben der gelesenen Rohdaten in das LogFile */
      if (uvr_typ == UVR1611)
        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile);
      if (uvr_typ == UVR61_3)
        tmp_erg = fwrite(puffer_dswinsol_uvr61_3,59,1,fp_logfile);

      /* ********* 2. Geraet ********* */
      /* Daten in die Winsol-Struktur kopieren */
      if (uvr_typ2 == UVR1611)
        copy_UVR2winsol_D1_1611( &u_dsatz_uvr[0], &dsatz_winsol_2[0], 2 );
      if (uvr_typ2 == UVR61_3)
        copy_UVR2winsol_D1_61_3( &u_dsatz_uvr[0], &dsatz_winsol_uvr61_3_2[0], 2 );
      /* schreiben der gelesenen Rohdaten in das LogFile */
      if (uvr_typ2 == UVR1611)
        tmp_erg = fwrite(puffer_dswinsol_2,59,1,fp_logfile_2);
      if (uvr_typ2 == UVR61_3)
        tmp_erg = fwrite(puffer_dswinsol_uvr61_3_2 ,59,1,fp_logfile_2);

      if ( ((i%100) == 0) && (i > 0) )
        printf("%d Datensaetze geschrieben.\n",i);

      if ( *end_adresse == sendbuf[1] && *(end_adresse+1) == sendbuf[2] && *(end_adresse+2) == sendbuf[3] )
            break;

      /* Hochzaehlen der Startadressen                                      */
      if (lowbyte == 0)
        lowbyte++;
      else
      {
        lowbyte = 0;
        middlebyte++;
      }

      switch (lowbyte)
      {
        case 0: sendbuf[1] = 0x00; break;
        case 1: sendbuf[1] = 0x80; break;
      }

      if (middlebyte > merkmiddlebyte)   /* das mittlere Byte muss erhoeht werden */
      {
        if ( sendbuf[2] != 0xFE )
        {
          sendbuf[2] = sendbuf[2] + 0x02;
          merkmiddlebyte = middlebyte;
        }
        else /* das highbyte muss erhoeht werden */
        {
          sendbuf[2] = 0x00;
          sendbuf[3] = sendbuf[3] + 0x01;
          merkmiddlebyte = middlebyte;
        }
      }

          if (sendbuf[3] > 0x0F ) // "Speicherueberlauf" im BL-Net
          {
                sendbuf[1] = 0x00;
                sendbuf[2] = 0x00;
                sendbuf[3] = 0x00;
          }

      monatswechsel = 0;
    } /* Ende: if (dsatz_uvr1611[0].pruefsum == pruefsumme) */
    else
    {
      if (merk_i < 5)
      {
        i--; /* falsche Pruefziffer - also nochmals lesen */
#ifdef DEBUG
        fprintf(stderr, " falsche Pruefsumme - Versuch#%d\n",merk_i);
#endif
        merk_i++; /* hochzaehlen bis 5 */
      }
      else
      {
        merk_i = 0;
        fehlerhafte_ds++;
        printf ( " fehlerhafter2 Datensatz - insgesamt:%d\n",fehlerhafte_ds);
      }
    }
  }

  return i + 1 - fehlerhafte_ds;
}

/* Daten vom DL lesen - CAN-Modus */
int datenlesen_DC(int anz_datensaetze)
{
  unsigned modTeiler;
  int i=0, j=0, y=0, merk_i=0, fehlerhafte_ds=0, result, lowbyte, middlebyte, merkmiddlebyte, tmp_erg = 0;
  int Bytes_for_0xDC = 524, monatswechsel = 0, anzahl_can_rahmen = 0, marker = 0, in_bytes=0;
  int pruefsum_check = 0;
  int Speicherueberlauf = 0; /* = 1 wenn Ringspeicher komplett voll und wird ueberschrieben */
  u_DS_CAN u_dsatz_can[1];
  DS_Winsol dsatz_winsol[8];  /* 8 Datensaetze moeglich */
  DS_Winsol *puffer_dswinsol = &dsatz_winsol[0];
  DS_CANBC dsatz_canbc[8];
  DS_CANBC *puffer_dscanbc = &dsatz_canbc[0];

  UCHAR pruefsumme = 0, merk_monat = 0;
  UCHAR sendbuf[6];       /*  sendebuffer fuer die Request-Commandos*/
  UCHAR empfbuf[18];
  UCHAR tmp_buf[525];

#if DEBUG > 3
/*  ##### Debug 3-CAN-Rahmen ########  */
  FILE *fp_logfile_debug=NULL;
  char DebugFile[] = "debug.log";
  FILE *fp_logfile_debug2=NULL;
  char DebugFile2[] = "debug2.log";
  u_DS_CAN *puffer_u_dsatz_can = &u_dsatz_can[0];
/*  ##### Debug 3-CAN-Rahmen ########  */
#endif

  modTeiler = 0x100;
  i = 0; /* Gesamtdurchlaufzaehler mit 0 initialisiert */
  merk_i = 0; /* Bei falscher Pruefziffer den Datensatz bis zu fuenfmal wiederholt lesen */
  fehlerhafte_ds = 0; /* Anzahl fehlerhaft gelesener Datensaetze mit 0 initialisiert */
  lowbyte = 0;
  middlebyte = 0;
  merkmiddlebyte = middlebyte;

#if DEBUG > 3
/*  ##### Debug 3-CAN-Rahmen ########  */
  if ((fp_logfile_debug=fopen(DebugFile,"w")) == NULL) /* dann Neuerstellung der Logdatei */
  {
     printf("Debug-Datei %s konnte nicht erstellt werden\n",DebugFile);
  }

if ((fp_logfile_debug2=fopen(DebugFile2,"w")) == NULL) /* dann Neuerstellung der Logdatei */
  {
     printf("Debug2-Datei %s konnte nicht erstellt werden\n",DebugFile);
  }
/*  ##### Debug 3-CAN-Rahmen ########  */
#endif

  for(i=1;i<525;i++)
  {
    u_dsatz_can[0].all_bytes[i] = 0xFF;
  }

  sendbuf[0]=VERSIONSABFRAGE;   /* Senden der Versionsabfrage */
  if (usb_zugriff)
  {
    close_usb();
    init_usb();
    fd_set rfds;
    struct timeval tv;
    int retval=0;
    int retry=0;
    int retry_interval=2; 
    
    write_erg=write(fd,sendbuf,1);
    if (write_erg == 1)    /* Lesen der Antwort*/
      result=read(fd,empfbuf,1);
    sendbuf[0]=KONFIGCAN;
    write_erg=write(fd,sendbuf,1);
    if (write_erg == 1)    /* Lesen der Antwort*/
    {
      do
      {
        in_bytes=0;
        FD_ZERO(&rfds);  /* muss jedes Mal gesetzt werden */
        FD_SET(fd, &rfds);
        tv.tv_sec = retry_interval; /* Wait up to five seconds. */
        tv.tv_usec = 0;
        retval = select(fd+1, &rfds, NULL, NULL, &tv);
        zeitstempel();
        if (retval == -1)
          perror("select(fd)");
        else if (retval)
        {
#ifdef DEBUG
          fprintf(stderr,"Data is available now. %d.%d\n",(int)tv.tv_sec,(int)tv.tv_usec);
#endif
          if (FD_ISSET(fd,&rfds))
          {
            ioctl(fd, FIONREAD, &in_bytes);
#ifdef DEBUG
		     fprintf(stderr,"Bytes im Puffer: %d\n",in_bytes);
#endif
            if (in_bytes == 18)
            {
              result=read(fd,empfbuf,18);   /* ?? */
              retry=4;
            }
          }
        }
      } while( retry < 3 && in_bytes != 0);
    }
  }
  if (ip_zugriff)
  {
    if (!ip_first)
    {
      sock = socket(PF_INET, SOCK_STREAM, 0);
      if (sock == -1)
      {
        perror("socket failed()");
        do_cleanup();
        return 2;
      }
      if (connect(sock, (const struct sockaddr *)&SERVER_sockaddr_in, sizeof(SERVER_sockaddr_in)) == -1)
      {
        perror("connect failed()");
        do_cleanup();
        return 3;
      }
    }  /* if (!ip_first) */
    write_erg=send(sock,sendbuf,1,0);
    if (write_erg == 1)    /* Lesen der Antwort */
      result  = recv(sock,empfbuf,1,0);

    sendbuf[0]=KONFIGCAN;
    write_erg=send(sock,sendbuf,1,0);
    if (write_erg == 1)    /* Lesen der Antwort */
      result  = recv(sock,empfbuf,18,0);
  }

  anzahl_can_rahmen = empfbuf[0];
#if DEBUG > 3
  fprintf(stderr,"Anzahl CAN-Datenrahmen: %d. \n",anzahl_can_rahmen);

  /*  ##### Debug 3-CAN-Rahmen ########  */
  fprintf(fp_logfile_debug2,"Anzahl CAN-Datenrahmen: %d. \n",anzahl_can_rahmen);
#endif

  if ( *start_adresse > 0 || *(start_adresse+1) > 0 || *(start_adresse+2) > 0 )
        Speicherueberlauf = 1;  /* Der Ringspeicher im BL-Net ist voll */

  /* fuellen des Sendebuffer - 6 Byte */
  sendbuf[0] = DATENBEREICHLESEN;
  // Startadressen von Kopfsatzlesen:
  sendbuf[1] = *start_adresse;      /* Beginn des Datenbereiches (low vor high) */
  sendbuf[2] = *(start_adresse+1);
  sendbuf[3] = *(start_adresse+2);
  sendbuf[4] = 0x01;  /* Anzahl der zu lesenden Rahmen */

  switch (sendbuf[1])  // vorbelegen lowbyte bei Startadr. > 00 00 00
  {
    case 0x00: lowbyte = 0; break;
    case 0x40: switch(anzahl_can_rahmen)
               {
                  case 1: lowbyte = 1; break;
                  case 3: lowbyte = 3; break;
                  case 5: lowbyte = 1; break;
                  case 7: lowbyte = 3; break;
               }
               break;
    case 0x80: switch(anzahl_can_rahmen)
               {
                  case 1: lowbyte = 2; break;
                  case 2: lowbyte = 3; break;
                  case 3: lowbyte = 2; break;
                  case 5: lowbyte = 2; break;
                  case 6: lowbyte = 3; break;
                  case 7: lowbyte = 2; break;
               }
               break;
    case 0xc0: switch(anzahl_can_rahmen)
               {
                  case 1: lowbyte = 3; break;
                  case 3: lowbyte = 1; break;
                  case 5: lowbyte = 3; break;
                  case 7: lowbyte = 1; break;
               }
               break;
  }

#if DEBUG > 3
/*  ##### Debug 3-CAN-Rahmen ########  */
  fprintf(fp_logfile_debug2,"Anzahl Datensaetze: %d. \n",anz_datensaetze);
#endif

  for(i=0;i<anz_datensaetze;i++)
  {
    sendbuf[5] = (sendbuf[0] + sendbuf[1] + sendbuf[2] + sendbuf[3] + sendbuf[4]) % modTeiler;  /* Pruefziffer */

/* DEBUG */
// fprintf(stderr," CAN-Logging-Test: %04d. Startadresse: %x %x %x - Endadresse: %x %x %x\n",i+1,sendbuf[1],sendbuf[2],sendbuf[3],*end_adresse,*(end_adresse+1),*(end_adresse+2));

#if DEBUG > 3
/*  ##### Debug CAN-Rahmen ########  */
  fprintf(fp_logfile_debug2," CAN-Logging-Debug: %04d. Startadresse: %x %x %x - Endadresse: %x %x %x\n",i+1,sendbuf[1],sendbuf[2],sendbuf[3],*end_adresse,*(end_adresse+1),*(end_adresse+2));
#endif

    if (usb_zugriff)
    {
      fd_set rfds;
      struct timeval tv;
      int retval=0;
      int retry=0;
      int retry_interval=2; 
      write_erg=write(fd,sendbuf,6);
      if (write_erg == 6)    /* Lesen der Antwort*/
      {
         do
        {
          in_bytes=0;
          FD_ZERO(&rfds);  /* muss jedes Mal gesetzt werden */
          FD_SET(fd, &rfds);
          tv.tv_sec = retry_interval; /* Wait up to five seconds. */
          tv.tv_usec = 0;
          retval = select(fd+1, &rfds, NULL, NULL, &tv);
          zeitstempel();
          if (retval == -1)
            perror("select(fd)");
          else if (retval)
          {
#ifdef DEBUG
          fprintf(stderr,"Data is available now. %d.%d\n",(int)tv.tv_sec,(int)tv.tv_usec);
#endif
            if (FD_ISSET(fd,&rfds))
            {
              ioctl(fd, FIONREAD, &in_bytes);
#ifdef DEBUG
		     fprintf(stderr,"Bytes im Puffer: %d\n",in_bytes);
#endif
              if (in_bytes == Bytes_for_0xDC) /* 524 */
              {
                result=read(fd, u_dsatz_can,Bytes_for_0xDC);
                retry=4;
              }
            }
          }
        } while( retry < 3 && in_bytes != 0);
      }
    }
    if (ip_zugriff)
    {
      if (!ip_first)
      {
        sock = socket(PF_INET, SOCK_STREAM, 0);
        if (sock == -1)
        {
          perror("socket failed()");
          do_cleanup();
          return 2;
        }
        if (connect(sock, (const struct sockaddr *)&SERVER_sockaddr_in, sizeof(SERVER_sockaddr_in)) == -1)
        {
          perror("connect failed()");
          do_cleanup();
          return 3;
        }
      }  /* if (!ip_first) */

      result = 0;
      marker = 0;
      write_erg=send(sock,sendbuf,6,0);
      if (write_erg == 6)    /* Lesen der Antwort */
      {
         do
         {
            result = recv(sock, tmp_buf,Bytes_for_0xDC,0);
            if (result > 0)
            {
              for (j=0;j<result;j++)
                 u_dsatz_can[0].all_bytes[marker+j] = tmp_buf[j];
              marker = marker + result;
            }
         } while ( marker < ((anzahl_can_rahmen * 61) + 3) );
      }
    } /* if (ip_zugriff) */

    if (uvr_typ == UVR1611)
      pruefsumme = berechnepruefziffer_uvr1611_CAN(u_dsatz_can, anzahl_can_rahmen);

  if (uvr_typ == UVR1611)
  {
    switch(anzahl_can_rahmen)
        {
          case 1: if (u_dsatz_can[0].DS_CAN_1.pruefsum == pruefsumme )
                    pruefsum_check = 1;
                break;
          case 2: if (u_dsatz_can[0].DS_CAN_2.pruefsum == pruefsumme )
                    pruefsum_check = 1;
                break;
          case 3: if (u_dsatz_can[0].DS_CAN_3.pruefsum == pruefsumme )
                    pruefsum_check = 1;
                break;
          case 4: if (u_dsatz_can[0].DS_CAN_4.pruefsum == pruefsumme )
                    pruefsum_check = 1;
                break;
          case 5: if (u_dsatz_can[0].DS_CAN_5.pruefsum == pruefsumme )
                    pruefsum_check = 1;
                break;
          case 6: if (u_dsatz_can[0].DS_CAN_6.pruefsum == pruefsumme )
                    pruefsum_check = 1;
                break;
          case 7: if (u_dsatz_can[0].DS_CAN_7.pruefsum == pruefsumme )
                    pruefsum_check = 1;
                break;
          case 8: if (u_dsatz_can[0].DS_CAN_8.pruefsum == pruefsumme )
                    pruefsum_check = 1;
                break;
        }
  }

    if ( pruefsum_check == 1 )
    {  /*Aenderung: 02.09.06 - Hochzaehlen der Startadresse erst dann, wenn korrekt gelesen wurde (eventuell endlosschleife?) */
      if ( i == 0 ) /* erster Datenssatz wurde gelesen - Logfile oeffnen / erstellen */
      {
        if (uvr_typ == UVR1611)
        {
          tmp_erg = ( erzeugeLogfileName_CAN(u_dsatz_can[0].DS_CAN_1.DS_CAN[0].datum_zeit.monat,u_dsatz_can[0].DS_CAN_1.DS_CAN[0].datum_zeit.jahr,anzahl_can_rahmen) );
          merk_monat = u_dsatz_can[0].DS_CAN_1.DS_CAN[0].datum_zeit.monat;
        }
        if ( tmp_erg == 0 )
        {
          printf("Der Logfile-Name konnte nicht erzeugt werden!");
          exit(-1);
        }
        else
        {

// CAN_BC-------------------------------------------------------------------------------------------------------------------
                     printf("Rahmen %d\n",kopf_DC[0].all_bytes[5]);
                     int c=0;
                     int cc=0;
//                     int can_typ[8];
                     for (c=0;c<kopf_DC[0].all_bytes[5];c++)
                     {
                     cc=c+6;
                     can_typ[c]=kopf_DC[0].all_bytes[cc];
                     fprintf(stderr, " Typ%02x: %02X \n", c, can_typ[c]);
                     }
// CAN_BC-------------------------------------------------------------------------------------------------------------------
                     int x;
                     for(x=1;x<=anzahl_can_rahmen;x++)
                        {
                                 if ( open_logfile_CAN(LogFileName[x], x, can_typ) == -1 )
                                {
                                  printf("Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName[x]);
                                  exit(-1);
                                }
                        }
        }

      }
      /* Hat der Monat gewechselt? Wenn ja, neues LogFile erstellen. */
      if ( merk_monat != u_dsatz_can[0].DS_CAN_1.DS_CAN[0].datum_zeit.monat )
        monatswechsel = 1;

      if ( monatswechsel == 1 )
      {
            switch(anzahl_can_rahmen)
                {
                  case 1: fclose(fp_logfile); break;
                  case 2: fclose(fp_logfile); fclose(fp_logfile_2); break;
                  case 3: fclose(fp_logfile); fclose(fp_logfile_2); fclose(fp_logfile_3); break;
                  case 4: fclose(fp_logfile); fclose(fp_logfile_2); fclose(fp_logfile_3); fclose(fp_logfile_4); break;
                  case 5: fclose(fp_logfile); fclose(fp_logfile_2); fclose(fp_logfile_3); fclose(fp_logfile_4);
                          fclose(fp_logfile_5); break;
                  case 6: fclose(fp_logfile); fclose(fp_logfile_2); fclose(fp_logfile_3); fclose(fp_logfile_4);
                          fclose(fp_logfile_5); fclose(fp_logfile_6); break;
                  case 7: fclose(fp_logfile); fclose(fp_logfile_2); fclose(fp_logfile_3); fclose(fp_logfile_4);
                          fclose(fp_logfile_5); fclose(fp_logfile_6); fclose(fp_logfile_7); break;
                  case 8: fclose(fp_logfile); fclose(fp_logfile_2); fclose(fp_logfile_3); fclose(fp_logfile_4);
                          fclose(fp_logfile_5); fclose(fp_logfile_6); fclose(fp_logfile_7); fclose(fp_logfile_8); break;
                }

        printf("Monatswechsel!\n");

                if (uvr_typ == UVR1611)
            tmp_erg = (erzeugeLogfileName_CAN(u_dsatz_can[0].DS_CAN_1.DS_CAN[0].datum_zeit.monat,u_dsatz_can[0].DS_CAN_1.DS_CAN[0].datum_zeit.jahr,anzahl_can_rahmen));
        if ( tmp_erg == 0 )
        {
           printf("Fehler beim Monatswechsel: Der Logfile-Name konnte nicht erzeugt werden!");
           exit(-1);
        }
        else
        {
                        int x;
                        for(x=1;x<=anzahl_can_rahmen;x++)
                        {
                                 if ( open_logfile_CAN(LogFileName[x], x, can_typ) == -1 )
                                {
                                  printf("Fehler beim Monatswechsel: Das LogFile %s kann nicht geoeffnet werden!\n",LogFileName[x]);
                                  exit(-1);
                                }
                        }
        }
      } /* Ende: if ( merk_monat != dsatz_uvr1611[0].datum_zeit.monat ) */

      /* uvr_typ == UVR1611 */
      merk_monat = u_dsatz_can[0].DS_CAN_1.DS_CAN[0].datum_zeit.monat;

      /* uvr_typ == UVR1611 */
          /* gelesene Datensaetze dem Winsol-Format zuordnen und in Logdateien schreiben */
          switch(anzahl_can_rahmen)
          {
            case 1: copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_1.DS_CAN[0], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile);
                        break;
            case 2: copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_2.DS_CAN[0], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile);
                        copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_2.DS_CAN[1], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_2);
                        break;
            case 3: copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_3.DS_CAN[0], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile);
                    copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_3.DS_CAN[1], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_2);
                    copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_3.DS_CAN[2], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_3);
#if DEBUG > 3
/*  ##### Debug 3-CAN-Rahmen ########  */
        tmp_erg = fwrite(puffer_u_dsatz_can,marker,1,fp_logfile_debug);
#endif
                        break;
            case 4:     if(can_typ[0]==UVR1611){copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_4.DS_CAN[0], &dsatz_winsol[0] );  
				           tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile); }
                        if(can_typ[0]==CAN_BC){copy_UVR2winsol_1611_CANBC( &u_dsatz_can[0].DS_CAN_BC_4.DS_CANBC[0], &dsatz_canbc[0] ); 
                           tmp_erg = fwrite(puffer_dscanbc,59,1,fp_logfile);  }
                        if(can_typ[1]==UVR1611){copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_4.DS_CAN[1], &dsatz_winsol[0] ); 
							tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_2); }
                        if(can_typ[1]==CAN_BC){copy_UVR2winsol_1611_CANBC( &u_dsatz_can[0].DS_CAN_BC_4.DS_CANBC[1], &dsatz_canbc[0] ); 
                            tmp_erg = fwrite(puffer_dscanbc,59,1,fp_logfile_2);  }
                        if(can_typ[2]==UVR1611){copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_4.DS_CAN[2], &dsatz_winsol[0] );
							tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_3); }
                        if(can_typ[2]==CAN_BC){copy_UVR2winsol_1611_CANBC( &u_dsatz_can[0].DS_CAN_BC_4.DS_CANBC[2], &dsatz_canbc[0] ); 
                            tmp_erg = fwrite(puffer_dscanbc,59,1,fp_logfile_3); }
//                       fprintf(stderr, " Typ2: %02X \n",can_typ[3]);
                        if(can_typ[3]==UVR1611){copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_4.DS_CAN[3], &dsatz_winsol[0] );
							tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_4); }
                        if(can_typ[3]==CAN_BC){copy_UVR2winsol_1611_CANBC( &u_dsatz_can[0].DS_CAN_BC_4.DS_CANBC[3], &dsatz_canbc[0] ); 
//                        copy_UVR2winsol_1611_CANBC( &u_dsatz_can[0].DS_CAN_4.DS_CAN[3], &dsatz_winsol[0] );
                            tmp_erg = fwrite(puffer_dscanbc,59,1,fp_logfile_4); }
                        break;
            case 5: copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_5.DS_CAN[0], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile);
                        copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_5.DS_CAN[1], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_2);
                                copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_5.DS_CAN[2], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_3);
                                copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_5.DS_CAN[3], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_4);
                                copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_5.DS_CAN[4], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_5);
#if DEBUG > 3
        tmp_erg = fwrite(puffer_u_dsatz_can,marker,1,fp_logfile_debug);
#endif
                        break;
            case 6: copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_6.DS_CAN[0], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile);
                        copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_6.DS_CAN[1], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_2);
                                copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_6.DS_CAN[2], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_3);
                                copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_6.DS_CAN[3], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_4);
                                copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_6.DS_CAN[4], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_5);
                                copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_6.DS_CAN[5], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_6);
                        break;
            case 7: copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_7.DS_CAN[0], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile);
                        copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_7.DS_CAN[1], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_2);
                                copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_7.DS_CAN[2], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_3);
                                copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_7.DS_CAN[3], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_4);
                                copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_7.DS_CAN[4], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_5);
                                copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_7.DS_CAN[5], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_6);
                                copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_7.DS_CAN[6], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_7);
                        break;
            case 8: copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_8.DS_CAN[0], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile);
                        copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_8.DS_CAN[1], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_2);
                                copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_8.DS_CAN[2], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_3);
                                copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_8.DS_CAN[3], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_4);
                                copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_8.DS_CAN[4], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_5);
                                copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_8.DS_CAN[5], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_6);
                                copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_8.DS_CAN[6], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_7);
                                copy_UVR2winsol_1611_CAN( &u_dsatz_can[0].DS_CAN_8.DS_CAN[7], &dsatz_winsol[0] );
                        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile_8);
                        break;
          }

      if ( ((i%100) == 0) && (i > 0) )
        printf("%d Datensaetze geschrieben.\n",i);

      if ( *end_adresse == sendbuf[1] && *(end_adresse+1) == sendbuf[2] && *(end_adresse+2) == sendbuf[3] )
            break;

      /* Hochzaehlen der Startadressen                                      */
      if (lowbyte <= 2)
        lowbyte++;
      else
      {
        lowbyte = 0;
      }

          switch (anzahl_can_rahmen)
          {
            case 1: switch (lowbyte)
                {
                   case 0: sendbuf[1] = 0x00; middlebyte++; break;
                   case 1: sendbuf[1] = 0x40; break;
                   case 2: sendbuf[1] = 0x80; break;
                   case 3: sendbuf[1] = 0xc0; break;
                }
                                if (middlebyte > merkmiddlebyte)   /* das mittlere Byte muss erhoeht werden */
                                {
                                        sendbuf[2] = sendbuf[2] + 0x02;
                                        merkmiddlebyte = middlebyte;
                                }
                if ( sendbuf[2] >= 0xFE ) /* das highbyte muss erhoeht werden */
                                {
                                        sendbuf[2] = 0x00;
                                        sendbuf[3] = sendbuf[3] + 0x01;
                                        merkmiddlebyte = middlebyte;
                                }
                                break;
                case 2: if ( sendbuf[2] == 0xFE && sendbuf[1] == 0x80 ) /* das highbyte muss erhoeht werden */
                                {
                                        switch (lowbyte)
                                        {
                                                case 0: sendbuf[1] = 0x00; middlebyte++; break;
                                                case 1: sendbuf[1] = 0x80; lowbyte = 3; break;
                                        }
                                        sendbuf[2] = 0x00;
                                        sendbuf[3] = sendbuf[3] + 0x01;
                                        merkmiddlebyte = middlebyte;
                                }
                                else
                                {
                                        switch (lowbyte)
                                        {
                                                case 0: sendbuf[1] = 0x00; middlebyte++; break;
                                                case 1: sendbuf[1] = 0x80; lowbyte = 3; break;
                                        }
                                        if (middlebyte > merkmiddlebyte)   /* das mittlere Byte muss erhoeht werden */
                                        {
                                                sendbuf[2] = sendbuf[2] + 0x02;
                                                merkmiddlebyte = middlebyte;
                                        }
                                }
                break;
            case 3: switch (lowbyte)
                {
                   case 0: sendbuf[1] = 0x00; break;
                   case 1: sendbuf[1] = 0xc0; break;
                   case 2: sendbuf[1] = 0x80; break;
                   case 3: sendbuf[1] = 0x40; break;
                }
                if (( sendbuf[2] == 0xFE ) && ( sendbuf[1] != 0xc0 )) /* das highbyte muss erhoeht werden */
                {
                    sendbuf[2] = 0x00;
                    sendbuf[3] = sendbuf[3] + 0x01;
                }
                                else
                                {
                                        if ( sendbuf[1] != 0xc0 )
                                        {
                                                sendbuf[2] = sendbuf[2] + 0x02;
                                        }
                                }
                                break;
                case 4: sendbuf[1] = 0x00;
                sendbuf[2] = sendbuf[2] + 0x02;
                if ( sendbuf[2] >= 0xFE ) /* das highbyte muss erhoeht werden */
                {
                    sendbuf[2] = 0x00;
                    sendbuf[3] = sendbuf[3] + 0x01;
                }
                                break;
            case 5: switch (lowbyte)
                {
                   case 0: sendbuf[1] = 0x00; break;
                   case 1: sendbuf[1] = 0x40; break;
                   case 2: sendbuf[1] = 0x80; break;
                   case 3: sendbuf[1] = 0xc0; break;
                }
                            if ( y == 3 )
                            {
                              sendbuf[2] = sendbuf[2] + 0x04;
                              y++;
                            }
                            else
                            {
                              sendbuf[2] = sendbuf[2] + 0x02;
                                  y++;
                            }
                if ( sendbuf[2] >= 0xFE ) /* das highbyte muss erhoeht werden */
                {
                   sendbuf[2] = 0x00;
                   sendbuf[3] = sendbuf[3] + 0x01;
                }
                                break;
                case 6: switch (lowbyte)
                {
                   case 0: sendbuf[1] = 0x00;
                           sendbuf[2] = sendbuf[2] + 0x04;
                                           break;
                   case 1: sendbuf[1] = 0x80;
                           sendbuf[2] = sendbuf[2] + 0x02;
                                                   lowbyte = 3;
                                           break;
                }
                if ( sendbuf[2] >= 0xFE ) /* das highbyte muss erhoeht werden */
                {
                    sendbuf[2] = 0x00;
                    sendbuf[3] = sendbuf[3] + 0x01;
                }
                                break;
            case 7: switch (lowbyte)
                {
                   case 0: sendbuf[1] = 0x00; break;
                   case 1: sendbuf[1] = 0xc0; break;
                   case 2: sendbuf[1] = 0x80; break;
                   case 3: sendbuf[1] = 0x40; break;
                }
                        if ( y == 0 )
                                {
                                    sendbuf[2] = sendbuf[2] + 0x02;
                                    y++;
                                }
                                else
                                {
                                    sendbuf[2] = sendbuf[2] + 0x04;
                                    y++;
                                }
                if ( sendbuf[2] >= 0xFE ) /* das highbyte muss erhoeht werden */
                {
                    sendbuf[2] = 0x00;
                    sendbuf[3] = sendbuf[3] + 0x01;
                }
                                break;
                case 8: sendbuf[1] = 0x00;
                sendbuf[2] = sendbuf[2] + 0x04;
                if ( sendbuf[2] >= 0xFE ) /* das highbyte muss erhoeht werden */
                {
                    sendbuf[2] = 0x00;
                    sendbuf[3] = sendbuf[3] + 0x01;
                }
                                break;
          }

      if ( y == 4 )
            y = 0;

          if (sendbuf[3] > 0x0F ) // "Speicherueberlauf" im BL-Net
          {
                sendbuf[1] = 0x00;
                sendbuf[2] = 0x00;
                sendbuf[3] = 0x00;
                Speicherueberlauf = 0;
          }

          if ( Speicherueberlauf == 0 )
          {
                if ( *(end_adresse+2) == sendbuf[3] || *(end_adresse+2) < sendbuf[3] )
                {
                        if ( *(end_adresse+1) == sendbuf[2] )
                        {
#if DEBUG > 3
 /*  ##### Debug 3-CAN-Rahmen ########  */
  fprintf(fp_logfile_debug2," Mittel-Byte Abbruch: %04d. Startadresse: %x %x %x - Endadresse: %x %x %x\n",i+1,sendbuf[1],sendbuf[2],sendbuf[3],*end_adresse,*(end_adresse+1),*(end_adresse+2));
#endif
                                if ( *end_adresse == sendbuf[1] || *end_adresse < sendbuf[1] )
                                {
#if DEBUG > 3
 /*  ##### Debug 3-CAN-Rahmen ########  */
  fprintf(fp_logfile_debug2," Abbruch erreicht: %04d. Startadresse: %x %x %x - Endadresse: %x %x %x\n",i+1,sendbuf[1],sendbuf[2],sendbuf[3],*end_adresse,*(end_adresse+1),*(end_adresse+2));
#endif
                                        break;
                                }
                        }
                        else if ( *(end_adresse+1) < sendbuf[2] )
                        {
#if DEBUG > 3
/*  ##### Debug 3-CAN-Rahmen ########  */
  fprintf(fp_logfile_debug2," Abbruch MittelByte-EA < MittelByte-SA : %04d. Startadresse: %x %x %x - Endadresse: %x %x %x\n",i+1,sendbuf[1],sendbuf[2],sendbuf[3],*end_adresse,*(end_adresse+1),*(end_adresse+2));
#endif
                                break;
                        }
                }
          }

      monatswechsel = 0;
    } /* Ende: if (dsatz_uvr1611[0].pruefsum == pruefsumme) */
    else
    {
      if (merk_i < 5)
      {
        i--; /* falsche Pruefziffer - also nochmals lesen */
#ifdef DEBUG
        fprintf(stderr, " falsche Pruefsumme - Versuch#%d\n",merk_i);
#endif
        merk_i++; /* hochzaehlen bis 5 */
      }
      else
      {
        merk_i = 0;
        fehlerhafte_ds++;
        printf ( " fehlerhafter3 Datensatz - insgesamt:%d\n",fehlerhafte_ds);
      }
    }
  }
  return i + 1 - fehlerhafte_ds;
//  return anz_datensaetze - fehlerhafte_ds;
}

/* Berechnung der Pruefsumme des Kopfsatz Modus 0xD1 */
int berechneKopfpruefziffer_D1(KopfsatzD1 derKopf[] )
{
  return  ((derKopf[0].kennung + derKopf[0].version
     + derKopf[0].zeitstempel[0]
     + derKopf[0].zeitstempel[1]
     + derKopf[0].zeitstempel[2]
     + derKopf[0].satzlaengeGeraet1
     + derKopf[0].satzlaengeGeraet2
     + derKopf[0].startadresse[0]
     + derKopf[0].startadresse[1]
     + derKopf[0].startadresse[2]
     + derKopf[0].endadresse[0]
     + derKopf[0].endadresse[1]
     + derKopf[0].endadresse[2]) % 0x100);
}

/* Berechnung der Pruefsumme des Kopfsatz Modus 0xDC */
int berechneKopfpruefziffer_DC(KOPFSATZ_DC derKopf[] )
{
  int retval=0, allebytes=0, i=0 ;

  allebytes = 12 + derKopf[0].all_bytes[5];

  for ( i=0; i<allebytes; i++)
        retval += derKopf[0].all_bytes[i];
  return retval % 0x100;
  }

/* Berechnung der Pruefsumme des Kopfsatz Modus 0xA8 */
int berechneKopfpruefziffer_A8(KopfsatzA8 derKopf[] )
{
  return  ((derKopf[0].kennung + derKopf[0].version
     + derKopf[0].zeitstempel[0]
     + derKopf[0].zeitstempel[1]
     + derKopf[0].zeitstempel[2]
     + derKopf[0].satzlaengeGeraet1
     + derKopf[0].startadresse[0]
     + derKopf[0].startadresse[1]
     + derKopf[0].startadresse[2]
     + derKopf[0].endadresse[0]
     + derKopf[0].endadresse[1]
     + derKopf[0].endadresse[2]) % 0x100);
}

/* Berechnung der Pruefsumme fuer UVR1611 */
int berechnepruefziffer_uvr1611(u_DS_UVR1611_UVR61_3 ds_uvr1611[])
{
  unsigned modTeiler;
  modTeiler = 0x100;    /* modTeiler = 256; */

  int retval=0;

  int k=0;
  for (k=0;k<16;k++)
    retval+= (ds_uvr1611[0].DS_UVR1611.sensT[k][0]+ds_uvr1611[0].DS_UVR1611.sensT[k][1]);

  retval += ds_uvr1611[0].DS_UVR1611.ausgbyte1+ds_uvr1611[0].DS_UVR1611.ausgbyte2+
    ds_uvr1611[0].DS_UVR1611.dza[0]+ds_uvr1611[0].DS_UVR1611.dza[1]+ds_uvr1611[0].DS_UVR1611.dza[2]+ds_uvr1611[0].DS_UVR1611.dza[3]+
    ds_uvr1611[0].DS_UVR1611.wmzaehler_reg+
    ds_uvr1611[0].DS_UVR1611.mlstg1[0]+ds_uvr1611[0].DS_UVR1611.mlstg1[1]+
    ds_uvr1611[0].DS_UVR1611.mlstg1[2]+ds_uvr1611[0].DS_UVR1611.mlstg1[3]+
    ds_uvr1611[0].DS_UVR1611.kwh1[0]+ds_uvr1611[0].DS_UVR1611.kwh1[1]+
    ds_uvr1611[0].DS_UVR1611.mwh1[0]+ds_uvr1611[0].DS_UVR1611.mwh1[1]+
    ds_uvr1611[0].DS_UVR1611.mlstg2[0]+ds_uvr1611[0].DS_UVR1611.mlstg2[1]+
    ds_uvr1611[0].DS_UVR1611.mlstg2[2]+ds_uvr1611[0].DS_UVR1611.mlstg2[3]+
    ds_uvr1611[0].DS_UVR1611.kwh2[0]+ds_uvr1611[0].DS_UVR1611.kwh2[1]+
    ds_uvr1611[0].DS_UVR1611.mwh2[0]+ds_uvr1611[0].DS_UVR1611.mwh2[1]+
    ds_uvr1611[0].DS_UVR1611.datum_zeit.sek+ds_uvr1611[0].DS_UVR1611.datum_zeit.min+
    ds_uvr1611[0].DS_UVR1611.datum_zeit.std+
    ds_uvr1611[0].DS_UVR1611.datum_zeit.tag+ds_uvr1611[0].DS_UVR1611.datum_zeit.monat+
    ds_uvr1611[0].DS_UVR1611.datum_zeit.jahr+
    ds_uvr1611[0].DS_UVR1611.zeitstempel[0]+ds_uvr1611[0].DS_UVR1611.zeitstempel[1]+
    ds_uvr1611[0].DS_UVR1611.zeitstempel[2];

  return retval % modTeiler;
}

/* Berechnung der Pruefsumme fuer UVR1611 CAN-Logging */
int berechnepruefziffer_uvr1611_CAN(u_DS_CAN dsatz_can[], int anzahl_can_rahmen)
{
  unsigned modTeiler = 0x100;    /* modTeiler = 256; */
  int retval=0, i=0, allebytes = 0;

  allebytes = anzahl_can_rahmen*61 + 3;  // ohne Byte Pruefsumme
  for ( i=0; i<allebytes; i++)
        retval += dsatz_can[0].all_bytes[i];
  return retval % modTeiler;
}

/* Berechnung der Pruefsumme fuer UVR61-3 */
int berechnepruefziffer_uvr61_3(u_DS_UVR1611_UVR61_3 ds_uvr61_3[])
{
  unsigned modTeiler;
  modTeiler = 0x100;    /* modTeiler = 256; */

  int retval=0;

  int k=0;
  for (k=0;k<6;k++)
    retval+= (ds_uvr61_3[0].DS_UVR61_3.sensT[k][0]+ds_uvr61_3[0].DS_UVR61_3.sensT[k][1]);

    retval+= ds_uvr61_3[0].DS_UVR61_3.ausgbyte1+
    ds_uvr61_3[0].DS_UVR61_3.dza+ds_uvr61_3[0].DS_UVR61_3.ausg_analog+
    ds_uvr61_3[0].DS_UVR61_3.wmzaehler_reg+
    ds_uvr61_3[0].DS_UVR61_3.volstrom[0]+ds_uvr61_3[0].DS_UVR61_3.volstrom[1]+
    ds_uvr61_3[0].DS_UVR61_3.mlstg1[0]+ds_uvr61_3[0].DS_UVR61_3.mlstg1[1]+
    ds_uvr61_3[0].DS_UVR61_3.kwh1[0]+ds_uvr61_3[0].DS_UVR61_3.kwh1[1]+
    ds_uvr61_3[0].DS_UVR61_3.mwh1[0]+ds_uvr61_3[0].DS_UVR61_3.mwh1[1]+
    ds_uvr61_3[0].DS_UVR61_3.mwh1[2]+ds_uvr61_3[0].DS_UVR61_3.mwh1[3]+
    ds_uvr61_3[0].DS_UVR61_3.datum_zeit.sek+ds_uvr61_3[0].DS_UVR61_3.datum_zeit.min+
    ds_uvr61_3[0].DS_UVR61_3.datum_zeit.std+
    ds_uvr61_3[0].DS_UVR61_3.datum_zeit.tag+ds_uvr61_3[0].DS_UVR61_3.datum_zeit.monat+
    ds_uvr61_3[0].DS_UVR61_3.datum_zeit.jahr+
    ds_uvr61_3[0].DS_UVR61_3.zeitstempel[0]+ds_uvr61_3[0].DS_UVR61_3.zeitstempel[1]+
    ds_uvr61_3[0].DS_UVR61_3.zeitstempel[2];

  return retval % modTeiler;
}

/* Berechnung der Pruefsumme fuer Modus D1 (2DL) */
int berechnepruefziffer_modus_D1(u_modus_D1 *ds_modus_D1, int anzahl)
{
  unsigned modTeiler;
  modTeiler = 0x100;    /* modTeiler = 256; */

  int retval=0;

  int k=0;
  for (k=0;k<anzahl-1;k++)
    retval+= ds_modus_D1[0].DS_alles.all_bytes[k];

  return retval % modTeiler;
}

/* Ermittlung Anzahl der Datensaetze Modus 0xD1 */
int anzahldatensaetze_D1(KopfsatzD1 kopf[])
{
  /* UCHAR byte1, byte2, byte3; */
  int byte1, byte2, byte3;

  /* Byte 1 - lowest */
  switch (kopf[0].endadresse[0])
    {
    case 0x0  : byte1 = 1; break;
    case 0x80 : byte1 = 2; break;
    default: printf("Falschen Wert im Low-Byte Endadresse gelesen!\n");
      return -1;
    }

  /* Byte 2 - mitte */
  byte2 = ((kopf[0].endadresse[1] / 0x02) * 0x02);

  /* Byte 3 - highest */
  byte3 = (kopf[0].endadresse[2] * 0x100)*0x02;

  if ( *end_adresse < *start_adresse || *(end_adresse+1) < *(start_adresse+1) || *(end_adresse+2) < *(start_adresse+2) )
        return 4096; // max. Anzahl Datenrahmen bei UVR1611 bzw. UVR61-3, Speicherueberlauf
  else
        return byte1 + byte2 + byte3;
}

/* Ermittlung Anzahl der Datensaetze Modus 0xDC (CAN-Logging) */
int anzahldatensaetze_DC(KOPFSATZ_DC kopf[])
{
  /* UCHAR byte1, byte2, byte3; */
  int byte1, byte2, byte3, return_byte, return_byte_max;

  switch(kopf[0].all_bytes[5])
  {
    case 1:
          if (kopf[0].DC_Rahmen1.endadresse[0] == kopf[0].DC_Rahmen1.startadresse[0] &&
          kopf[0].DC_Rahmen1.endadresse[1] == kopf[0].DC_Rahmen1.startadresse[1] &&
          kopf[0].DC_Rahmen1.endadresse[2] == kopf[0].DC_Rahmen1.startadresse[2] &&
          kopf[0].DC_Rahmen1.endadresse[0] == 0xFF && kopf[0].DC_Rahmen1.endadresse[1] == 0xFF && kopf[0].DC_Rahmen1.endadresse[2] == 0xFF )
          {
             printf("Keine geloggten Daten verfuegbar!\n");
                 return -2;
          }
      /* Byte 1 - lowest */
      switch (kopf[0].DC_Rahmen1.endadresse[0])
      {
        case 0x0  : byte1 = 1; break;
        case 0x40 : byte1 = 2; break;
        case 0x80 : byte1 = 3; break;
        case 0xc0 : byte1 = 4; break;
        default: printf("Falschen Wert im Low-Byte Endadresse gelesen!\n");
          return -1;
      }
      /* Byte 2 - mitte */
      byte2 = ((kopf[0].DC_Rahmen1.endadresse[1] / 0x02) * 0x04);
      /* Byte 3 - highest */
      byte3 = (kopf[0].DC_Rahmen1.endadresse[2] * 0x200);
          return_byte = byte1 + byte2 + byte3;
          return_byte_max = 8192;
      break;
    case 2:
          if (kopf[0].DC_Rahmen2.endadresse[0] == kopf[0].DC_Rahmen2.startadresse[0] &&
          kopf[0].DC_Rahmen2.endadresse[1] == kopf[0].DC_Rahmen2.startadresse[1] &&
          kopf[0].DC_Rahmen2.endadresse[2] == kopf[0].DC_Rahmen2.startadresse[2]  &&
          kopf[0].DC_Rahmen2.endadresse[0] == 0xFF && kopf[0].DC_Rahmen2.endadresse[1] == 0xFF && kopf[0].DC_Rahmen2.endadresse[2] == 0xFF )
          {
             printf("Keine geloggten Daten verfuegbar!\n");
                 return -2;
          }
      /* Byte 1 - lowest */
      switch (kopf[0].DC_Rahmen2.endadresse[0])
      {
        case 0x0  : byte1 = 1; break;
        case 0x40 : byte1 = 2; break;
        case 0x80 : byte1 = 3; break;
        case 0xc0 : byte1 = 4; break;
        default: printf("Falschen Wert im Low-Byte Endadresse gelesen!\n");
          return -1;
      }
          return_byte = ((kopf[0].DC_Rahmen2.endadresse[2] * 0x200) + (kopf[0].DC_Rahmen2.endadresse[1] /2 )* 4 + (byte1 -1)) / 2 ; // + 1;
          return_byte_max = 4096;
      break;
    case 3:
          if (kopf[0].DC_Rahmen3.endadresse[0] == kopf[0].DC_Rahmen3.startadresse[0] &&
          kopf[0].DC_Rahmen3.endadresse[1] == kopf[0].DC_Rahmen3.startadresse[1] &&
          kopf[0].DC_Rahmen3.endadresse[2] == kopf[0].DC_Rahmen3.startadresse[2]  &&
          kopf[0].DC_Rahmen3.endadresse[0] == 0xFF && kopf[0].DC_Rahmen3.endadresse[1] == 0xFF && kopf[0].DC_Rahmen3.endadresse[2] == 0xFF )
          {
             printf("Keine geloggten Daten verfuegbar!\n");
                 return -2;
          }
      /* Byte 1 - lowest */
      switch (kopf[0].DC_Rahmen3.endadresse[0])
      {
        case 0x0  : byte1 = 1; break;
        case 0x40 : byte1 = 2; break;
        case 0x80 : byte1 = 3; break;
        case 0xc0 : byte1 = 4; break;
        default: printf("Falschen Wert im Low-Byte Endadresse gelesen!\n");
          return -1;
      }
          return_byte = ((kopf[0].DC_Rahmen3.endadresse[2] * 0x200) + (kopf[0].DC_Rahmen3.endadresse[1] /2 )* 4 + (byte1 -1)) / 3 ; // + 1;
          return_byte_max = 2730;
      break;
    case 4:
          if (kopf[0].DC_Rahmen4.endadresse[0] == kopf[0].DC_Rahmen4.startadresse[0] &&
          kopf[0].DC_Rahmen4.endadresse[1] == kopf[0].DC_Rahmen4.startadresse[1] &&
          kopf[0].DC_Rahmen4.endadresse[2] == kopf[0].DC_Rahmen4.startadresse[2]  &&
          kopf[0].DC_Rahmen4.endadresse[0] == 0xFF && kopf[0].DC_Rahmen4.endadresse[1] == 0xFF && kopf[0].DC_Rahmen4.endadresse[2] == 0xFF )
          {
             printf("Keine geloggten Daten verfuegbar!\n");
                 return -2;
          }
      /* Byte 1 - lowest */
      switch (kopf[0].DC_Rahmen4.endadresse[0])
      {
        case 0x0  : byte1 = 1; break;
        case 0x40 : byte1 = 2; break;
        case 0x80 : byte1 = 3; break;
        case 0xc0 : byte1 = 4; break;
        default: printf("Falschen Wert im Low-Byte Endadresse gelesen!\n");
          return -1;
      }
          return_byte = ((kopf[0].DC_Rahmen4.endadresse[2] * 0x200) + (kopf[0].DC_Rahmen4.endadresse[1] /2 )* 4 + (byte1 -1)) / 4 ; // + 1;
          return_byte_max = 2048;
      break;
    case 5:
          if (kopf[0].DC_Rahmen5.endadresse[0] == kopf[0].DC_Rahmen5.startadresse[0] &&
          kopf[0].DC_Rahmen5.endadresse[1] == kopf[0].DC_Rahmen5.startadresse[1] &&
          kopf[0].DC_Rahmen5.endadresse[2] == kopf[0].DC_Rahmen5.startadresse[2]  &&
          kopf[0].DC_Rahmen5.endadresse[0] == 0xFF && kopf[0].DC_Rahmen5.endadresse[1] == 0xFF && kopf[0].DC_Rahmen5.endadresse[2] == 0xFF )
          {
             printf("Keine geloggten Daten verfuegbar!\n");
                 return -2;
          }
      /* Byte 1 - lowest */
      switch (kopf[0].DC_Rahmen5.endadresse[0])
      {
        case 0x0  : byte1 = 1; break;
        case 0x40 : byte1 = 2; break;
        case 0x80 : byte1 = 3; break;
        case 0xc0 : byte1 = 4; break;
        default: printf("Falschen Wert im Low-Byte Endadresse gelesen!\n");
          return -1;
      }
          return_byte = ((kopf[0].DC_Rahmen5.endadresse[2] * 0x200) + (kopf[0].DC_Rahmen5.endadresse[1] /2 )* 4 + (byte1 -1)) / 5 ; // + 1;
          return_byte_max = 1638;
      break;
    case 6:
          if (kopf[0].DC_Rahmen6.endadresse[0] == kopf[0].DC_Rahmen6.startadresse[0] &&
          kopf[0].DC_Rahmen6.endadresse[1] == kopf[0].DC_Rahmen6.startadresse[1] &&
          kopf[0].DC_Rahmen6.endadresse[2] == kopf[0].DC_Rahmen6.startadresse[2]  &&
          kopf[0].DC_Rahmen6.endadresse[0] == 0xFF && kopf[0].DC_Rahmen6.endadresse[1] == 0xFF && kopf[0].DC_Rahmen6.endadresse[2] == 0xFF )
          {
             printf("Keine geloggten Daten verfuegbar!\n");
                 return -2;
          }
      /* Byte 1 - lowest */
      switch (kopf[0].DC_Rahmen6.endadresse[0])
      {
        case 0x0  : byte1 = 1; break;
        case 0x40 : byte1 = 2; break;
        case 0x80 : byte1 = 3; break;
        case 0xc0 : byte1 = 4; break;
        default: printf("Falschen Wert im Low-Byte Endadresse gelesen!\n");
          return -1;
      }
          return_byte = ((kopf[0].DC_Rahmen6.endadresse[2] * 0x200) + (kopf[0].DC_Rahmen6.endadresse[1] /2 )* 4 + (byte1 -1)) / 6 ; // + 1;
          return_byte_max = 1365;
      break;
    case 7:
          if (kopf[0].DC_Rahmen7.endadresse[0] == kopf[0].DC_Rahmen7.startadresse[0] &&
          kopf[0].DC_Rahmen7.endadresse[1] == kopf[0].DC_Rahmen7.startadresse[1] &&
          kopf[0].DC_Rahmen7.endadresse[2] == kopf[0].DC_Rahmen7.startadresse[2]  &&
          kopf[0].DC_Rahmen7.endadresse[0] == 0xFF && kopf[0].DC_Rahmen7.endadresse[1] == 0xFF && kopf[0].DC_Rahmen7.endadresse[2] == 0xFF )
          {
             printf("Keine geloggten Daten verfuegbar!\n");
                 return -2;
          }
      /* Byte 1 - lowest */
      switch (kopf[0].DC_Rahmen7.endadresse[0])
      {
        case 0x0  : byte1 = 1; break;
        case 0x40 : byte1 = 2; break;
        case 0x80 : byte1 = 3; break;
        case 0xc0 : byte1 = 4; break;
        default: printf("Falschen Wert im Low-Byte Endadresse gelesen!\n");
          return -1;
      }
          return_byte = ((kopf[0].DC_Rahmen7.endadresse[2] * 0x200) + (kopf[0].DC_Rahmen7.endadresse[1] /2 )* 4 + (byte1 -1)) / 7 ; //+ 1;
          return_byte_max = 1170;
      break;
    case 8:
          if (kopf[0].DC_Rahmen8.endadresse[0] == kopf[0].DC_Rahmen8.startadresse[0] &&
          kopf[0].DC_Rahmen8.endadresse[1] == kopf[0].DC_Rahmen8.startadresse[1] &&
          kopf[0].DC_Rahmen8.endadresse[2] == kopf[0].DC_Rahmen8.startadresse[2]  &&
          kopf[0].DC_Rahmen8.endadresse[0] == 0xFF && kopf[0].DC_Rahmen8.endadresse[1] == 0xFF && kopf[0].DC_Rahmen8.endadresse[2] == 0xFF )
          {
             printf("Keine geloggten Daten verfuegbar!\n");
                 return -2;
          }
      /* Byte 1 - lowest */
      switch (kopf[0].DC_Rahmen8.endadresse[0])
      {
        case 0x0  : byte1 = 1; break;
        case 0x40 : byte1 = 2; break;
        case 0x80 : byte1 = 3; break;
        case 0xc0 : byte1 = 4; break;
        default: printf("Falschen Wert im Low-Byte Endadresse gelesen!\n");
          return -1;
      }
          return_byte = ((kopf[0].DC_Rahmen8.endadresse[2] * 0x200) + (kopf[0].DC_Rahmen8.endadresse[1] /2 )* 4 + (byte1 -1)) / 8 ; // + 1;
          return_byte_max = 1024;
          break;
  }

  if ( *end_adresse < *start_adresse || *(end_adresse+1) < *(start_adresse+1) || *(end_adresse+2) < *(start_adresse+2) )
        return return_byte_max; // max. Anzahl Datenrahmen bei UVR1611 bzw. UVR61-3, Speicherueberlauf
  else
    return return_byte;
}

/* Ermittlung Anzahl der Datensaetze Modus 0xA8 */
int anzahldatensaetze_A8(KopfsatzA8 kopf[])
{
  /* UCHAR byte1, byte2, byte3; */
  int byte1, byte2, byte3;

  /* Byte 1 - lowest */
  switch (kopf[0].endadresse[0])
    {
    case 0x0  : byte1 = 1; break;
    case 0x40 : byte1 = 2; break;
    case 0x80 : byte1 = 3; break;
    case 0xc0 : byte1 = 4; break;
    default: printf("Falschen Wert im Low-Byte Endadresse gelesen!\n");
      return -1;
    }

  /* Byte 2 - mitte */
  byte2 = ((kopf[0].endadresse[1] / 0x02) * 0x04);

  /* Byte 3 - highest */
  byte3 = (kopf[0].endadresse[2] * 0x100)*0x02;

  if ( *end_adresse < *start_adresse || *(end_adresse+1) < *(start_adresse+1) || *(end_adresse+2) < *(start_adresse+2) )
        return 8192; // max. Anzahl Datenrahmen bei UVR1611 bzw. UVR61-3, Speicherueberlauf
  else
        return byte1 + byte2 + byte3;
}

/* Datenpuffer im D-LOGG zuruecksetzen -USB */
int reset_datenpuffer_usb(int do_reset )
{
  int result = 0, in_bytes=0;;
  UCHAR sendbuf[1];       /*  sendebuffer fuer die Request-Commandos*/
  UCHAR empfbuf[256];

  close_usb();
  sendbuf[0]=ENDELESEN;   /* Senden "Ende lesen" */
  init_usb();
  
  fd_set rfds;
  struct timeval tv;
  int retval=0;
  int retry=0;
  int retry_interval=2; 

  write_erg=write(fd,sendbuf,1);
  if (write_erg == 1)    /* Lesen der Antwort*/
  {
    do
    {
	    in_bytes=0;
      FD_ZERO(&rfds);  /* muss jedes Mal gesetzt werden */
      FD_SET(fd, &rfds);
      tv.tv_sec = retry_interval; /* Wait up to five seconds. */
      tv.tv_usec = 0;
      retval = select(fd+1, &rfds, NULL, NULL, &tv);
      zeitstempel();
      if (retval == -1)
        perror("select(fd)");
      else if (retval)
      {
#ifdef DEBUG
          fprintf(stderr,"Data is available now. %d.%d\n",(int)tv.tv_sec,(int)tv.tv_usec);
#endif
        if (FD_ISSET(fd,&rfds))
        {
          ioctl(fd, FIONREAD, &in_bytes);
#ifdef DEBUG
		     fprintf(stderr,"Bytes im Puffer: %d\n",in_bytes);
#endif
          if (in_bytes == 1)
            result=read(fd,empfbuf,1);
        }
      }
    } while( retry < 3 && in_bytes != 0);
  }  /* Hier fertig mit "Ende lesen" */

  zeitstempel();

  if ( (empfbuf[0] == ENDELESEN) && (do_reset == 1) )
  {
    sendbuf[0]=RESETDATAFLASH;   /* Senden Buffer zuruecksetzen */
    write_erg=write(fd,sendbuf,1);
    if (write_erg == 1)    /* Lesen der Antwort*/
    {
      do
      {
        in_bytes=0;
        FD_ZERO(&rfds);  /* muss jedes Mal gesetzt werden */
        FD_SET(fd, &rfds);
        tv.tv_sec = retry_interval; /* Wait up to five seconds. */
        tv.tv_usec = 0;
        retval = select(fd+1, &rfds, NULL, NULL, &tv);
        zeitstempel();
        if (retval == -1)
          perror("select(fd)");
        else if (retval)
        {
#ifdef DEBUG
          fprintf(stderr,"Data is available now. %d.%d\n",(int)tv.tv_sec,(int)tv.tv_usec);
#endif
          if (FD_ISSET(fd,&rfds))
          {
            ioctl(fd, FIONREAD, &in_bytes);
#ifdef DEBUG
		     fprintf(stderr,"Bytes im Puffer: %d\n",in_bytes);
#endif
            if (in_bytes == 1)
            {
              result=read(fd,empfbuf,1);
              /* printf("Vom DL erhaltene Reset-Bestaetigung: %x\n",empfbuf[0]); */
              fprintf(fp_varlogfile,"%s - %s -- Vom DL erhaltene Reset-Bestaetigung: %x.\n",sDatum, sZeit,empfbuf[0]);
            }
          }
        }
      } while( retry < 3 && in_bytes != 0);
    }
    else
      printf("Reset konnte nicht gesendet werden. Ergebnis = %d\n",result);
  }
  else
  {
    /* printf("Kein Data-Reset! \n"); */ /* reset-variable=%d \n",reset); */
    fprintf(fp_varlogfile,"%s - %s -- Kein Data-Reset! \n",sDatum, sZeit);
  }
  return 1;
}

/* Datenpuffer im D-LOGG zuruecksetzen -IP */
int reset_datenpuffer_ip(int do_reset )
{
  int result = 0;
  UCHAR sendbuf[1];       /*  sendebuffer fuer die Request-Commandos*/
  UCHAR empfbuf[256];

  sendbuf[0]=ENDELESEN;   /* Senden "Ende lesen" */
  if (!ip_first)
  {
    sock = socket(PF_INET, SOCK_STREAM, 0);
      if (sock == -1)
      {
        perror("socket failed()");
        do_cleanup();
        return 2;
      }
      if (connect(sock, (const struct sockaddr *)&SERVER_sockaddr_in, sizeof(SERVER_sockaddr_in)) == -1)
      {
        perror("connect failed()");
        do_cleanup();
        return 3;
      }
     }  /* if (!ip_first) */
     write_erg=send(sock,sendbuf,1,0);
    if (write_erg == 1)    /* Lesen der Antwort */
      result  = recv(sock,empfbuf,1,0);
   /* Hier fertig mit "Ende lesen" */

  zeitstempel();

  if ( (empfbuf[0] == ENDELESEN) && (do_reset == 1) )
  {
    sendbuf[0]=RESETDATAFLASH;   /* Senden Buffer zuruecksetzen */
    write_erg=send(sock,sendbuf,1,0);
    if (write_erg == 1)    /* Lesen der Antwort*/
    {
      result  = recv(sock,empfbuf,1,0);
      fprintf(fp_varlogfile,"%s - %s -- Vom DL erhaltene Reset-Bestaetigung: %x.\n",sDatum, sZeit,empfbuf[0]);
    }
    else
      printf("Reset konnte nicht gesendet werden. Ergebnis = %d\n",result);
  }
  else
  {
    fprintf(fp_varlogfile,"%s - %s -- Kein Data-Reset! \n",sDatum, sZeit);
  }
  return 1;
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

/* Berechne die Temperatur / Volumenstrom / Strahlung */
float berechnetemp(UCHAR lowbyte, UCHAR highbyte)
{
  UCHAR temp_highbyte;
  int z;
  short wert;
int sensor=0; //!!!!!!!!!!!!!
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

/* Abfrage per IP */
int ip_handling(int sock)
{
  unsigned char empfbuf[256];
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

  return 0;
}
