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
 * Version 0.1		18.04.2006 erste Testversion                             *
 * Version 0.5a		05.10.2006 Protokoll-Log in /var/dl-lesenx.log speichern *
 * Version 0.6		27.01.2006 C. Dolainsky                                  *
 * 					28.01.2006 Anpassung an geaenderte dl-lesen.h.           *
 * 					18.03.2007 IP                                            *
 * Version 0.7		01.04.2007                                               *
 * Version 0.7.7	26.12.2007 UVR61-3                                       *
 * Version 0.8		13.01.2008 2DL-Modus                                     *
 *****************************************************************************/

#include <sys/types.h>
#include <sys/stat.h>
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

#include "../dl-lesen.h"

//#define DEBUG 4

#define BAUDRATE B115200
#define UVR61_3 0x5A
#define UVR1611 0x76

extern char *optarg;
extern int optind, opterr, optopt;

int do_cleanup(void);
int check_arg(int arg_c, char *arg_v[]);
int check_arg_getopt(int arg_c, char *arg_v[]);
int erzeugeLogfileName(UCHAR ds_monat, UCHAR ds_jahr);
int open_logfile(char LogFile[], int geraet);
int close_logfile(void);
int get_modulmodus(void);
int kopfsatzlesen(void);
void testfunktion(void);
int copy_UVR2winsol_1611(u_DS_UVR1611_UVR61_3 *dsatz_uvr1611, DS_Winsol *dsatz_winsol );
int copy_UVR2winsol_61_3(u_DS_UVR1611_UVR61_3 *dsatz_uvr61_3, DS_Winsol_UVR61_3 *dsatz_winsol_uvr61_3 );
int copy_UVR2winsol_D1_1611(u_modus_D1 *dsatz_modus_d1, DS_Winsol *dsatz_winsol , int geraet);
int copy_UVR2winsol_D1_61_3(u_modus_D1 *dsatz_modus_d1, DS_Winsol_UVR61_3 *dsatz_winsol_uvr61_3, int geraet);
int datenlesen_A8(int anz_datensaetze);
int datenlesen_D1(int anz_datensaetze);
int berechneKopfpruefziffer_D1(KopfsatzD1 derKopf[] );
int berechneKopfpruefziffer_A8(KopfsatzA8 derKopf[] );
int berechnepruefziffer_uvr1611(u_DS_UVR1611_UVR61_3 ds_uvr1611[]);
int berechnepruefziffer_uvr61_3(u_DS_UVR1611_UVR61_3 ds_uvr61_3[]);
int berechnepruefziffer_modus_D1(u_modus_D1 ds_modus_D1[], int anzahl);
int anzahldatensaetze_D1(KopfsatzD1 kopf[]);
int anzahldatensaetze_A8(KopfsatzA8 kopf[]);
int reset_datenpuffer_usb(int do_reset );
int reset_datenpuffer_ip(int do_reset );
void berechne_werte(int anz_regler);
void ausgaengeanz(void);
void drehzahlstufenanz(void);
void waermemengenanz(void);
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
int fd_serialport,  write_erg; /* anz_datensaetze; */
struct sockaddr_in SERVER_sockaddr_in;

int csv_header_done=-1;

FILE *fp_logfile=NULL, *fp_logfile_2=NULL, *fp_varlogfile=NULL ; /* pointer IMMER initialisieren und vor benutzung pruefen */
FILE *fp_csvfile=NULL ;

char dlport[13]; /* Uebergebener Parameter USB-Port */
char LogFileName[12], LogFileName_2[14];
char varLogFile[22];
char sDatum[11], sZeit[11];
u_DS_UVR1611_UVR61_3 structDLdatensatz;

struct termios oldtio; /* will be used to save old port settings */

int ip_zugriff, ip_first;
int usb_zugriff;
UCHAR uvr_modus, uvr_typ, uvr_typ2;  /* uvr_typ2 -> 2. Geraet bei 2DL */
int sock;


int main(int argc, char *argv[])
{
  struct termios newtio; /* will be used for new port settings */

  int i, anz_ds, erg_check_arg, i_varLogFile, erg=0;
  char *pvarLogFile;

  ip_zugriff = 0;
  usb_zugriff = 0;
  ip_first = 1;

  erg_check_arg = check_arg_getopt(argc, argv);

#if  DEBUG>1
  printf("Ergebnis vom Argumente-Check %d\n",erg_check_arg);
  printf("angegebener Port: %s Variablen: reset = %d csv = %d \n",dlport,reset,csv);
#endif

  if ( erg_check_arg != 1 )
    exit(-1);

  /* LogFile zum Speichern von Ausgaben aus diesem Programm hier */
  pvarLogFile = &varLogFile[0];
  sprintf(pvarLogFile,"./dl-lesenx.log");

  if ((fp_varlogfile=fopen(varLogFile,"a")) == NULL)
    {
      printf("Log-Datei %s konnte nicht erstellt werden\n",varLogFile);
      i_varLogFile = -1;
      fp_varlogfile=NULL;
    }
  else
    i_varLogFile = 1;


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
    /* PF_INET instead of AF_INET - because of Protocol-family instead of address family !? */
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
      fprintf(stderr, "%s: Fehler im Initialisieren der IP-Kommunikation\n", argv[0]);
      do_cleanup();
      return 4;
    }
    //  close(sock); /* IP-Socket schliessen */

  } /* Ende IP-Zugriff */
  else  if (usb_zugriff && !ip_zugriff)
  {
    /* first open the serial port for reading and writing */
    fd_serialport = open(dlport, O_RDWR | O_NOCTTY );
    if (fd_serialport < 0)
    {
      zeitstempel();
      if (fp_varlogfile)
        fprintf(fp_varlogfile,"%s - %s -- kann nicht auf port %s zugreifen \n",sDatum, sZeit,dlport);
        perror(dlport);
        do_cleanup();
        return (-1);
    }

    /* save current port settings */
    tcgetattr(fd_serialport,&oldtio);
    /* initialize the port settings structure to all zeros */
    bzero( &newtio, sizeof(newtio));
    /* then set the baud rate, handshaking and a few other settings */
    newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;
    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    newtio.c_cc[VMIN]     = 1;   /* blocking read until first char received */

    tcflush(fd_serialport, TCIFLUSH);
    tcsetattr(fd_serialport,TCSANOW,&newtio);
  }
  else
  {
    fprintf(stderr, " da lief was falsch ....\n %s \n", argv[0]);
    do_cleanup();
  }

  uvr_modus = get_modulmodus(); /* Welcher Modus 0xA8 (1DL) oder 0xD1 (2DL) */

  /* ************************************************************************   */
  /* Lesen des Kopfsatzes zur Ermittlung der Anzahl der zu lesenden Datensaetze  */
  i=1;
  do                        /* max. 5 durchlgaenge */
  {
#ifdef DEBUG
      printf("\n Kopfsatzlesen - Versuch%d\n",i);
#endif
      anz_ds = kopfsatzlesen();
      i++;
  }
  while((anz_ds == -1) && (i < 6));

  if ( anz_ds == -1 )
  {
    printf(" Kopfsatzlesen fehlgeschlagen\n");
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
  if ( uvr_modus == 0xA8 )
    erg = datenlesen_A8(anz_ds);
  else if ( uvr_modus == 0xD1 )
    erg = datenlesen_D1(anz_ds);

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
  //tcsetattr(fd_serialport,TCSANOW,&oldtio);

  int retval=0;
  if ( close_logfile() == -1)
  {
    printf("Cannot close logfile!");
    retval=-1;
  }

  if ( do_cleanup() < 0 )
    retval=-1;

  return (retval);
}

/* Aufraeumen und alles Schliessen */
int do_cleanup(void)
{
  int retval=0;

  if ( fd_serialport > 0 )
    {
      // reset and close
      tcflush(fd_serialport, TCIFLUSH);
      tcsetattr(fd_serialport,TCSANOW,&oldtio);
    }

  if (ip_zugriff)
    close(sock); /* IP-Socket schliessen */

  if (csv==1 && fp_csvfile )
    {
      if ( fclose(fp_csvfile) != 0 )
  {
    printf("Cannot close csvfile %s!",varLogFile);
    retval=-1;
  }
      else
  fp_csvfile=NULL;
    }

  if (fp_varlogfile)
    {
      if ( fclose(fp_varlogfile) != 0 )
  {
    printf("Cannot close %s!",varLogFile);
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

  fprintf(stderr,"\ndl-lesenx (-p USB-Port | -i IP:Port)  [--csv] [--res] [-h] [-v]\n");
  fprintf(stderr,"    -p USB-Port -> Angabe des USB-Portes,\n");
  fprintf(stderr,"                   an dem der D-LOGG angeschlossen ist.\n");
  fprintf(stderr,"    -i IP:Port  -> Angabe der IP-Adresse und des Ports,\n");
  fprintf(stderr,"                   an dem der BL-Net angeschlossen ist.\n");
  fprintf(stderr,"          --csv  -> im CSV-Format speichern (wird noch nicht unterstuetzt)\n");
  fprintf(stderr,"                   Standard: ist WINSOL-Format\n");
  fprintf(stderr,"          --res  -> nach dem Lesen Ruecksetzen des DL\n");
  fprintf(stderr,"                   Standard: KEIN Ruecksetzen des DL nach dem Lesen\n");
  fprintf(stderr,"          -h    -> diesen Hilfetext\n");
  fprintf(stderr,"          -v    -> Versionsangabe\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"Beispiel: dl-lesenx -p /dev/ttyUSB0 -res\n");
  fprintf(stderr,"          Liest die Daten vom USB-Port 0 und setzt den D-LOGG zurueck.\n");
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
        printf("    Version 0.8 vom 13.01.2008 \n");
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
        if ( strlen(optarg) < 22)
        {
          SERVER_sockaddr_in.sin_addr.s_addr = inet_addr(strtok(optarg,trennzeichen));
          SERVER_sockaddr_in.sin_port = htons((unsigned short int) atol(strtok(NULL,trennzeichen)));
          SERVER_sockaddr_in.sin_family = AF_INET;
          fprintf(stderr,"\n Adresse:port gesetzt: %s:%d\n", inet_ntoa(SERVER_sockaddr_in.sin_addr),
          ntohs(SERVER_sockaddr_in.sin_port));
          i_is_set=1;
          ip_zugriff = 1;
        }
        else
        {
          fprintf(stderr," IP-Adresse zu lang: %s\n",optarg);
          print_usage();
          return -1;
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
  char csv_endung[] = ".csv", winsol_endung[] = ".log";
  char *pLogFileName=NULL, *pLogFileName_2=NULL;
  pLogFileName = LogFileName;
  pLogFileName_2 = LogFileName_2;

  if (csv ==  1) /* LogDatei im CSV-Format schreiben */
    {
      erg=sprintf(pLogFileName,"2%03d%02d%s",ds_jahr,ds_monat,csv_endung);
      if (uvr_modus == 0xD1)
        erg=sprintf(pLogFileName_2,"2%03d%02d_2%s",ds_jahr,ds_monat,csv_endung);
    }
  else  /* LogDatei im Winsol-Format schreiben */
    {
      erg=sprintf(pLogFileName,"Y2%03d%02d%s",ds_jahr,ds_monat,winsol_endung);
      if (uvr_modus == 0xD1)
        erg=sprintf(pLogFileName_2,"Y2%03d%02d_2%s",ds_jahr,ds_monat,winsol_endung);
    }

  return erg;
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

/* Logdatei schliessen */
int close_logfile(void)
{
  int i = -1;

  i=fclose(fp_logfile);
  if (uvr_modus == 0xD1)
    i=fclose(fp_logfile_2);
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
         printf(" Drehz.-Stufe highbyte: %X  \n", temp_highbyte);
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

    fprintf(fp_WSLOGcsvfile," %.1f; %.1f;",momentLstg1,kwh1);
  }
  else
    fprintf(fp_WSLOGcsvfile," --; --;");

  fprintf(fp_WSLOGcsvfile,"\n");
}

/* Modulmoduskennung abfragen */
int get_modulmodus(void)
{
  int result;
  int sendbuf[1];       /*  sendebuffer fuer die Request-Commandos*/
  int empfbuf[1];

  sendbuf[0]=VERSIONSABFRAGE;    /* Senden der Kopfsatz-abfrage */

/* ab hier unterscheiden nach USB und IP */
  if (usb_zugriff)
  {
    write_erg=write(fd_serialport,sendbuf,1);
    if (write_erg == 1)    /* Lesen der Antwort*/
      result=read(fd_serialport,empfbuf,1);
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


/* Kopfsatz vom DL lesen und Rueckgabe Anzahl Datensaetze */
int kopfsatzlesen(void)
{
  int result, anz_ds, pruefz, merk_pruefz, durchlauf;
  int sendbuf[1];       /*  sendebuffer fuer die Request-Commandos*/
  UCHAR print_endaddr;
  KopfsatzD1 kopf_D1[1];
  KopfsatzA8 kopf_A8[1];
  durchlauf=0;

  do
    {
      sendbuf[0]=KOPFSATZLESEN;    /* Senden der Kopfsatz-abfrage */

  if (usb_zugriff)
  {
    write_erg=write(fd_serialport,sendbuf,1);
    if (write_erg == 1)    /* Lesen der Antwort*/
    {
      if ( uvr_modus == 0xD1 )
      {
        result=read(fd_serialport,kopf_D1,14);
      }
      else
      {
        result=read(fd_serialport,kopf_A8,13);
      }
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
      if ( uvr_modus == 0xD1 )
      {
        result = recv(sock,kopf_D1,14,0);
      }
      else
      {
        result = recv(sock,kopf_A8,13,0);
      }
    }
  }

  if ( uvr_modus == 0xD1 )
  {
    pruefz = berechneKopfpruefziffer_D1( kopf_D1 );
    merk_pruefz = kopf_D1[0].pruefsum;
  }
  else
  {
    pruefz = berechneKopfpruefziffer_A8( kopf_A8 );
    merk_pruefz = kopf_A8[0].pruefsum;
  }

   durchlauf++;

#ifdef DEBUG
  if ( uvr_modus == 0xD1 )
    printf("  Durchlauf #%d  berechnete pruefziffer:%d kopfsatz.pruefsumme:%d\n",durchlauf,pruefz%0x100,kopf_D1[0].pruefsum);
  else
    printf("  Durchlauf #%d  berechnete pruefziffer:%d kopfsatz.pruefsumme:%d\n",durchlauf,pruefz%0x100,kopf_A8[0].pruefsum);
#endif
  }
  while (( (pruefz != merk_pruefz )  && (durchlauf < 10)));


  if (pruefz != merk_pruefz )
    {
#ifdef DEBUG
      printf(" Durchlauf #%i - falsche Pruefziffer im Kopfsatz\n",durchlauf);
#endif
      return -1;
    }
#ifdef DEBUG
  else
    printf("Anzahl Durchlaeufe Pruefziffer Kopfsatz: %i\n",durchlauf);
#endif

  if ( uvr_modus == 0xD1 )
  {
    anz_ds = anzahldatensaetze_D1(kopf_D1);
    print_endaddr = kopf_D1[0].endadresse[0];
  }
  else
  {
    anz_ds = anzahldatensaetze_A8(kopf_A8);
    print_endaddr = kopf_A8[0].endadresse[0];
  }

  if ( anz_ds == -1 )
    {
      zeitstempel();
      fprintf(fp_varlogfile,"%s - %s -- Falschen Wert im Low-Byte Endadresse gelesen! Wert: %x\n",sDatum, sZeit,print_endaddr);
    }
  else
    printf(" Anzahl Datensaetze aus Kopfsatz: %i\n",anz_ds);

#if  DEBUG>5
  printf("  Errechnete Pruefsumme: %x\n", berechneKopfpruefziffer(kopf) ); printf(" empfangene Pruefsumme: %x\n",kopf[0].pruefsum);
  printf("empfangen von DL/BL:\n Kennung: %X\n",kopf[0].kennung);
  printf(" Version: %X\n",kopf[0].version);
  printf(" Zeitstempel (hex): %x %x %x\n",kopf[0].zeitstempel[0],kopf[0].zeitstempel[1],kopf[0].zeitstempel[2]);
  printf(" Zeitstempel (int): %d %d %d\n",kopf[0].zeitstempel[0],kopf[0].zeitstempel[1],kopf[0].zeitstempel[2]);

  long secs = (long)(kopf[0].zeitstempel[0]) * 32768 + (long)(kopf[0].zeitstempel[1]) * 256 + (long)(kopf[0].zeitstempel[2]);
  long secsinv = (long)(kopf[0].zeitstempel[0]) + (long)(kopf[0].zeitstempel[1]) * 256 + (long)(kopf[0].zeitstempel[2]) * 32768;

  printf(" Zeitstempel : %ld  %ld\n", secs,secsinv);

  printf(" Satzlaenge: %x\n",kopf[0].satzlaenge);
  printf(" Startadresse: %x %x %x\n",kopf[0].startadresse[0],kopf[0].startadresse[1],kopf[0].startadresse[2]);
  printf(" Endadresse: %x %x %x\n",kopf[0].endadresse[0],kopf[0].endadresse[1],kopf[0].endadresse[2]);
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

  return anz_ds;
}

/* Funktion zum Testen */
void testfunktion(void)
{
  int result;
  int sendbuf[1];       /*  sendebuffer fuer die Request-Commandos*/
  UCHAR empfbuf[256];

  sendbuf[0]=VERSIONSABFRAGE;   /* Senden der Versionsabfrage */
  write_erg=write(fd_serialport,sendbuf,1);
  if (write_erg == 1)    /* Lesen der Antwort*/
    result=read(fd_serialport,empfbuf,1);
  printf("Vom DL erhalten Version: %x\n",empfbuf[0]);

  sendbuf[0]=FWABFRAGE;    /* Senden der Firmware-Versionsabfrage */
  write_erg=write(fd_serialport,sendbuf,1);
  if (write_erg == 1)    /* Lesen der Antwort*/
    result=read(fd_serialport,empfbuf,1);
  printf("Vom DL erhalten Version FW: %x\n",empfbuf[0]);

  sendbuf[0]=MODEABFRAGE;    /* Senden der Modus-abfrage */
  write_erg=write(fd_serialport,sendbuf,1);
  if (write_erg == 1)    /* Lesen der Antwort*/
    result=read(fd_serialport,empfbuf,1);
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
  int Bytes_for_0xA8 = 65, monatswechsel = 0;
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
    write_erg=write(fd_serialport,sendbuf,1);
    if (write_erg == 1)    /* Lesen der Antwort*/
      result=read(fd_serialport,empfbuf,1);
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
  sendbuf[1] = 0x00;  /* Beginn des Datenbereiches (low vor high) */
  sendbuf[2] = 0x00;  /* Beginn des Datenbereiches (low vor high) */
  sendbuf[3] = 0x00;  /* Beginn des Datenbereiches (low vor high) */
  sendbuf[4] = 0x01;  /* Anzahl der zu lesenden Rahmen */

  for(;i<anz_datensaetze;i++)
  {
    sendbuf[5] = (sendbuf[0] + sendbuf[1] + sendbuf[2] + sendbuf[3] + sendbuf[4]) % modTeiler;  /* Pruefziffer */

    if (usb_zugriff)
    {
      write_erg=write(fd_serialport,sendbuf,6);
      if (write_erg == 6)    /* Lesen der Antwort*/
        // result=read(fd_serialport, dsatz_uvr1611,65);
        result=read(fd_serialport, u_dsatz_uvr,Bytes_for_0xA8);
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

        // result  = recv(sock, dsatz_uvr1611,65,0);
    } /* if (ip_zugriff) */

//**************************************************************************************** !!!!!!!!
    if (uvr_typ == UVR1611)
      pruefsumme = berechnepruefziffer_uvr1611(u_dsatz_uvr);
    if (uvr_typ == UVR61_3)
      pruefsumme = berechnepruefziffer_uvr61_3(u_dsatz_uvr);
#if DEBUG > 3
  if (uvr_typ == UVR1611)
      printf("%d. Datensatz - Pruefsumme gelesen: %x  berechnet: %x \n",i+1,u_dsatz_uvr[0].DS_UVR1611.pruefsum,pruefsumme);
  if (uvr_typ == UVR61_3)
      printf("%d. Datensatz - Pruefsumme gelesen: %x  berechnet: %x \n",i+1,u_dsatz_uvr[0].DS_UVR61_3.pruefsum,pruefsumme);
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
          if ( open_logfile(LogFileName, 1) == -1 )
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

//  printf("Monat: %2d Variable monatswechsel: %1d\n", u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.monat,monatswechsel);
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
            if ( open_logfile(LogFileName, 1) == -1 )
            {
              printf("Fehler beim Monatswechsel: Das LogFile kann nicht geoeffnet werden!\n");
              exit(-1);
            }
          }
        }
      } /* Ende: if ( merk_monat != dsatz_uvr1611[0].datum_zeit.monat ) */

      if (uvr_typ == UVR1611)
        merk_monat = u_dsatz_uvr[0].DS_UVR1611.datum_zeit.monat;
      if (uvr_typ == UVR61_3)
        merk_monat = u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.monat;

      if (uvr_typ == UVR1611)
        copy_UVR2winsol_1611( &u_dsatz_uvr[0], &dsatz_winsol[0] );
      if (uvr_typ == UVR61_3)
        copy_UVR2winsol_61_3( &u_dsatz_uvr[0], &dsatz_winsol_uvr61_3[0] );

      if ( csv==1 && fp_csvfile != NULL )
      {
        if (uvr_typ == UVR1611)
//          writeWINSOLlogfile2CSV(fp_csvfile, &dsatz_winsol[0],
           writeWINSOLlogfile2CSV(fp_logfile, &dsatz_winsol[0],
           u_dsatz_uvr[0].DS_UVR1611.datum_zeit.jahr,
           u_dsatz_uvr[0].DS_UVR1611.datum_zeit.monat );
        if (uvr_typ == UVR61_3)
//          writeWINSOLlogfile2CSV(fp_csvfile, &dsatz_winsol[0],
          writeWINSOLlogfile2CSV(fp_logfile, &dsatz_winsol[0],
           u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.jahr,
           u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.monat );
      }

      /* puffer_dswinsol = &dsatz_winsol[0];*/
      /* schreiben der gelesenen Rohdaten in das LogFile */
      if (uvr_typ == UVR1611)
        tmp_erg = fwrite(puffer_dswinsol,59,1,fp_logfile);
      if (uvr_typ == UVR61_3)
        tmp_erg = fwrite(puffer_dswinsol_uvr61_3,59,1,fp_logfile);

      if ( ((i%100) == 0) && (i > 0) )
        printf("%d Datensaetze geschrieben.\n",i);

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
      monatswechsel = 0;
    } /* Ende: if (dsatz_uvr1611[0].pruefsum == pruefsumme) */
    else
    {
      if (merk_i < 5)
      {
        i--; /* falsche Pruefziffer - also nochmals lesen */
#ifdef DEBUG
        printf ( " falsche Pruefsumme - Versuch#%d\n",merk_i);
#endif
        merk_i++; /* hochzaehlen bis 5 */
      }
      else
      {
        merk_i = 0;
        fehlerhafte_ds++;
        printf ( " fehlerhafter Datensatz - insgesamt:%d\n",fehlerhafte_ds);
      }
    }
  }

  return i - fehlerhafte_ds;
}

/* Daten vom DL lesen - 2DL-Modus */
int datenlesen_D1(int anz_datensaetze)
{
  unsigned modTeiler;
  int i, merk_i, fehlerhafte_ds, result=0, lowbyte, middlebyte, merkmiddlebyte, tmp_erg = 0;
  int Bytes_for_0xD1 = 127, monatswechsel = 0;
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
    write_erg=write(fd_serialport,sendbuf,1);
    if (write_erg == 1)    /* Lesen der Antwort*/
      result=read(fd_serialport,empfbuf,1);
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
  sendbuf[1] = 0x00;  /* Beginn des Datenbereiches (low vor high) */
  sendbuf[2] = 0x00;  /* Beginn des Datenbereiches (low vor high) */
  sendbuf[3] = 0x00;  /* Beginn des Datenbereiches (low vor high) */
  sendbuf[4] = 0x01;  /* Anzahl der zu lesenden Rahmen */

  for(;i<anz_datensaetze;i++)
  {
    sendbuf[5] = (sendbuf[0] + sendbuf[1] + sendbuf[2] + sendbuf[3] + sendbuf[4]) % modTeiler;  /* Pruefziffer */

    if (usb_zugriff)
    {
      write_erg=write(fd_serialport,sendbuf,6);
      if (write_erg == 6)    /* Lesen der Antwort*/
        // result=read(fd_serialport, dsatz_uvr1611,65);
        result=read(fd_serialport, u_dsatz_uvr,Bytes_for_0xD1);
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
        // result  = recv(sock, dsatz_uvr1611,65,0);
    } /* if (ip_zugriff) */

//**************************************************************************************** !!!!!!!!
      pruefsumme = berechnepruefziffer_modus_D1( &u_dsatz_uvr[0], result );
//      printf("Pruefsumme berechnet: %x in Byte %d erhalten %x\n",pruefsumme,result,u_dsatz_uvr[0].DS_alles.all_bytes[result-1]);
#if DEBUG > 3
  if (uvr_typ == UVR1611)
      printf("%d. Datensatz - Pruefsumme gelesen: %x  berechnet: %x \n",i+1,u_dsatz_uvr[0].DS_UVR1611.pruefsum,pruefsumme);
  if (uvr_typ == UVR61_3)
      printf("%d. Datensatz - Pruefsumme gelesen: %x  berechnet: %x \n",i+1,u_dsatz_uvr[0].DS_UVR61_3.pruefsum,pruefsumme);
#endif

    if (u_dsatz_uvr[0].DS_alles.all_bytes[result-1] == pruefsumme )
    {  /*Aenderung: 02.09.06 - Hochzaehlen der Startadresse erst dann, wenn korrekt gelesen wurde (eventuell endlosschleife?) */
#if DEBUG > 4
    print_dsatz_uvr1611_content(u_dsatz_uvr);
      int zz;
      for (zz=0;zz<result-1;zz++)
        printf("%2x ",u_dsatz_uvr[0].DS_alles.all_bytes[zz]);
      printf("\nuvr_typ(1): 0x%x uvr_typ(2): 0x%x \n",uvr_typ,uvr_typ2);
      printf("%2x \n",u_dsatz_uvr[0].DS_alles.all_bytes[59]);
#endif
      if ( i == 0 ) /* erster Datenssatz wurde gelesen - Logfile oeffnen / erstellen */
      {
        if (uvr_typ == UVR1611)
        {
          tmp_erg = erzeugeLogfileName(u_dsatz_uvr[0].DS_1611_1611.datum_zeit.monat,u_dsatz_uvr[0].DS_1611_1611.datum_zeit.jahr);
          merk_monat = u_dsatz_uvr[0].DS_1611_1611.datum_zeit.monat;
//          printf("merk_monat1: %d\n",merk_monat);
        }
        if (uvr_typ == UVR61_3)
        {
          tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_61_3_61_3.datum_zeit.monat,u_dsatz_uvr[0].DS_61_3_61_3.datum_zeit.jahr));
          merk_monat = u_dsatz_uvr[0].DS_61_3_61_3.datum_zeit.monat;
//          printf("merk_monat2: %d\n",merk_monat);
        }
//        printf("uvr_typ(1): 0x%x Gelesener Monat: %d, Jahr %d tmp_erg: %d\n",uvr_typ,merk_monat,u_dsatz_uvr[0].DS_1611_1611.datum_zeit.jahr,tmp_erg );

        if ( tmp_erg == 0 ) /* Das erste  */
        {
          printf("Der Logfile-Name (1) konnte nicht erzeugt werden!");
          exit(-1);
        }
        /* ********* 2. Geraet ********* */
        if (uvr_typ2 == UVR1611 && uvr_typ == UVR1611)
        {
          tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_1611_1611.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_1611_1611.Z_datum_zeit.jahr));
//        printf("Gelesener 2. Monat: %d Jahr %d\n",u_dsatz_uvr[0].DS_1611_1611.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_1611_1611.Z_datum_zeit.jahr);
        }
        else if (uvr_typ2 == UVR1611 && uvr_typ == UVR61_3)
        {
          tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_61_3_1611.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_61_3_1611.Z_datum_zeit.jahr));
//        printf("Gelesener 2. Monat: %d Jahr %d\n",u_dsatz_uvr[0].DS_61_3_1611.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_61_3_1611.Z_datum_zeit.jahr);
        }
        if (uvr_typ2 == UVR61_3 && uvr_typ == UVR61_3)
        {
          tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_61_3_61_3.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_61_3_61_3.Z_datum_zeit.jahr));
//        printf("Gelesener 2. Monat: %d Jahr %d\n",u_dsatz_uvr[0].DS_61_3_61_3.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_61_3_61_3.Z_datum_zeit.jahr);
        }
        else if (uvr_typ2 == UVR61_3 && uvr_typ == UVR1611)
        {
          tmp_erg = (erzeugeLogfileName(u_dsatz_uvr[0].DS_1611_61_3.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_1611_61_3.Z_datum_zeit.jahr));
//        printf("Gelesener 2. Monat: %d Jahr %d\n",u_dsatz_uvr[0].DS_1611_61_3.Z_datum_zeit.monat,u_dsatz_uvr[0].DS_1611_61_3.Z_datum_zeit.jahr);
        }
        if ( tmp_erg == 0 )
        {
          printf("Der Logfile-Name (2) konnte nicht erzeugt werden!");
          exit(-1);
        }
        else
        {
          if ( open_logfile(LogFileName, 1) == -1 )
          {
            printf("Das LogFile kann nicht geoeffnet werden!\n");
            exit(-1);
          }
          if ( open_logfile(LogFileName_2, 2) == -1 )
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

//  printf("Monat: %2d Variable monatswechsel: %1d\n", u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.monat,monatswechsel);
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
            if ( open_logfile(LogFileName, 1) == -1 )
            {
              printf("Fehler beim Monatswechsel: Das LogFile kann nicht geoeffnet werden!\n");
              exit(-1);
            }
            if ( open_logfile(LogFileName_2, 2) == -1 )
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
//printf("Nach Schreiben der Daten 1. Geraet\n");

      /* ********* 2. Geraet ********* */
      /* Daten in die Winsol-Struktur kopieren */
      if (uvr_typ2 == UVR1611)
        copy_UVR2winsol_D1_1611( &u_dsatz_uvr[0], &dsatz_winsol_2[0], 2 );
      if (uvr_typ2 == UVR61_3)
        copy_UVR2winsol_D1_61_3( &u_dsatz_uvr[0], &dsatz_winsol_uvr61_3_2[0], 2 );
//printf("nach copy_ 2. Geraet.\n");
      /* schreiben der gelesenen Rohdaten in das LogFile */
      if (uvr_typ2 == UVR1611)
        tmp_erg = fwrite(puffer_dswinsol_2,59,1,fp_logfile_2);
      if (uvr_typ2 == UVR61_3)
        tmp_erg = fwrite(puffer_dswinsol_uvr61_3_2 ,59,1,fp_logfile_2);
//printf("Nach Schreiben der Daten 2. Geraet\n");

/*      if ( csv==1 && fp_csvfile != NULL )
      {
        if (uvr_typ == UVR1611)
//          writeWINSOLlogfile2CSV(fp_csvfile, &dsatz_winsol[0],
           writeWINSOLlogfile2CSV(fp_logfile, &dsatz_winsol[0],
           u_dsatz_uvr[0].DS_UVR1611.datum_zeit.jahr,
           u_dsatz_uvr[0].DS_UVR1611.datum_zeit.monat );
        if (uvr_typ == UVR61_3)
//          writeWINSOLlogfile2CSV(fp_csvfile, &dsatz_winsol[0],
          writeWINSOLlogfile2CSV(fp_logfile, &dsatz_winsol[0],
           u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.jahr,
           u_dsatz_uvr[0].DS_UVR61_3.datum_zeit.monat );
      }
*/

      if ( ((i%100) == 0) && (i > 0) )
        printf("%d Datensaetze geschrieben.\n",i);

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
      monatswechsel = 0;
    } /* Ende: if (dsatz_uvr1611[0].pruefsum == pruefsumme) */
    else
    {
      if (merk_i < 5)
      {
        i--; /* falsche Pruefziffer - also nochmals lesen */
#ifdef DEBUG
        printf ( " falsche Pruefsumme - Versuch#%d\n",merk_i);
#endif
        merk_i++; /* hochzaehlen bis 5 */
      }
      else
      {
        merk_i = 0;
        fehlerhafte_ds++;
        printf ( " fehlerhafter Datensatz - insgesamt:%d\n",fehlerhafte_ds);
      }
    }
  }

  return i - fehlerhafte_ds;
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

/* Ermittlung Anzahl der Datensaetze Modus 0xA8 */
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

  return byte1 + byte2 + byte3;
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

  return byte1 + byte2 + byte3;
}

/* Datenpuffer im D-LOGG zuruecksetzen -USB */
int reset_datenpuffer_usb(int do_reset )
{
  int result = 0, sendbuf[1];       /*  sendebuffer fuer die Request-Commandos*/
  UCHAR empfbuf[256];

  sendbuf[0]=ENDELESEN;   /* Senden "Ende lesen" */
   write_erg=write(fd_serialport,sendbuf,1);
   if (write_erg == 1)    /* Lesen der Antwort*/
     result=read(fd_serialport,empfbuf,1);
   /* Hier fertig mit "Ende lesen" */

  zeitstempel();

  if ( (empfbuf[0] == ENDELESEN) && (do_reset == 1) )
  {
    sendbuf[0]=RESETDATAFLASH;   /* Senden Buffer zuruecksetzen */
    write_erg=write(fd_serialport,sendbuf,1);
    if (write_erg == 1)    /* Lesen der Antwort*/
    {
      result=read(fd_serialport,empfbuf,1);
      /* printf("Vom DL erhaltene Reset-Bestaetigung: %x\n",empfbuf[0]); */
      fprintf(fp_varlogfile,"%s - %s -- Vom DL erhaltene Reset-Bestaetigung: %x.\n",sDatum, sZeit,empfbuf[0]);
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
  int result = 0, sendbuf[1];       /*  sendebuffer fuer die Request-Commandos*/
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
      /* printf("Vom DL erhaltene Reset-Bestaetigung: %x\n",empfbuf[0]); */
      fprintf(fp_varlogfile,"%s - %s -- Vom DL erhaltene Reset-Bestaetigung: %x.\n",sDatum, sZeit,empfbuf[0]);
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

/* Aufruf der Funktionen zur Berechnung der Werte */
void berechne_werte(int anz_regler)
{
  int i, j, anz_bytes_1 = 0, anz_bytes_2 = 0;

  /* anz_regler = 1 : nur ein Regler vorhanden oder der erste Regler bei 0xD1 */
  if (anz_regler == 2)
  {
    i = 0;
//!!!!!!!!!!!!!    switch(akt_daten[0])
    {
//!!!!!!!!!!!!!      case UVR1611: anz_bytes_1 = 56; break; /* UVR1611 */
//!!!!!!!!!!!!!      case UVR61_3: anz_bytes_1 = 27; break; /* UVR61-3 */
    }
//!!!!!!!!!!!!!    switch(akt_daten[anz_bytes_1])
    {
//!!!!!!!!!!!!!      case UVR1611: anz_bytes_2 = 56; break; /* UVR1611 */
//!!!!!!!!!!!!!      case UVR61_3: anz_bytes_2 = 27; break; /* UVR61-3 */
    }

#ifdef DEBUG
    for (i=0;i<(anz_bytes_1+anz_bytes_2);i++) // Testausgabe 2DL
    {
        fprintf(stdout,"%2x; ", akt_daten[i]);
    }
    fprintf(stdout,"\n");
#endif

    i = 0;
    for (j=anz_bytes_1;j<(anz_bytes_1+anz_bytes_2) ;j++)
    {
//!!!!!!!!!!!!!      akt_daten[i] = akt_daten[j];
      i++;
    }
//!!!!!!!!!!!!!    akt_daten[i]='\0'; /* mit /000 abschliessen!! */
  }

//!!!!!!!!!!!!!  temperaturanz();
  ausgaengeanz();
  drehzahlstufenanz();
  waermemengenanz();

}

/* Bearbeitung der Temperatur-Sensoren */
//!!!!!!!!!!!!!void temperaturanz(void)
//!!!!!!!!!!!!!{
//!!!!!!!!!!!!!  int i, j, anzSensoren;
//!!!!!!!!!!!!!  anzSensoren = 16;

//!!!!!!!!!!!!!  switch(uvr_typ)
//!!!!!!!!!!!!!  {
//!!!!!!!!!!!!!    case UVR1611: anzSensoren = 16; break; /* UVR1611 */
//!!!!!!!!!!!!!    case UVR61_3: anzSensoren = 6; break; /* UVR61-3 */
//!!!!!!!!!!!!!  }

  /* vor berechnetemp() die oberen 4 Bit des HighByte auswerten!!!! */
  /* Wichtig fuer Minus-Temp. */
//!!!!!!!!!!!!!  j=1;
//!!!!!!!!!!!!!  for(i=1;i<=anzSensoren;i++)
//!!!!!!!!!!!!!    {
//!!!!!!!!!!!!!      SENS_Art[i] = eingangsparameter(akt_daten[j+1]);
//!!!!!!!!!!!!!      switch(SENS_Art[i])
//!!!!!!!!!!!!!  {
//!!!!!!!!!!!!!  case 0: SENS[i] = 0; break;
//!!!!!!!!!!!!!  case 1: SENS[i] = 0; break; // digit. Pegel (AUS)
//!!!!!!!!!!!!!  case 2: SENS[i] = berechnetemp(akt_daten[j],akt_daten[j+1],SENS_Art[i]); break; // Temp.
//!!!!!!!!!!!!!  case 3: SENS[i] = berechnevol(akt_daten[j],akt_daten[j+1]); break;
//!!!!!!!!!!!!!  case 6: SENS[i] = berechnetemp(akt_daten[j],akt_daten[j+1],SENS_Art[i]); break; // Strahlung
//!!!!!!!!!!!!!  case 7: SENS[i] = berechnetemp(akt_daten[j],akt_daten[j+1],SENS_Art[i]); break; // Raumtemp.
//!!!!!!!!!!!!!  case 9: SENS[i] = 1; break; // digit. Pegel (EIN)
//!!!!!!!!!!!!!  case 10: SENS[i] = berechnetemp(akt_daten[j],akt_daten[j+1],SENS_Art[i]); break; // Minus-Temperaturen
//!!!!!!!!!!!!!  case 15: SENS[i] = berechnetemp(akt_daten[j],akt_daten[j+1],SENS_Art[i]); break; // Minus-Raumtemp.
//!!!!!!!!!!!!!  }
//!!!!!!!!!!!!!      j=j+2;
//!!!!!!!!!!!!!    }
//!!!!!!!!!!!!!}

/* Bearbeitung der Ausgaenge */
void ausgaengeanz(void)
{
  //  int ausgaenge[13];
  // Ausgnge 2byte: low vor high
  // Bitbelegung:
  // AAAA AAAA
  // xxxA AAAA
  //  x ... dont care
  // A ... Ausgang (von low nach high zu nummerieren)
  int z;

  if (uvr_typ == UVR1611) /* UVR1611 */
  {
    /* Ausgaenge belegt? */
    for(z=1;z<9;z++)
    {
//!!!!!!!!!!!!!        if (tstbit( akt_daten[33], z-1 ) == 1)
//!!!!!!!!!!!!!          AUSG[z] = 1;
//!!!!!!!!!!!!!        else
//!!!!!!!!!!!!!          AUSG[z] = 0;
    }
    for(z=1;z<6;z++)
    {
//!!!!!!!!!!!!!      if (tstbit( akt_daten[34], z-1 ) == 1)
//!!!!!!!!!!!!!        AUSG[z+8] = 1;
//!!!!!!!!!!!!!      else
//!!!!!!!!!!!!!        AUSG[z+8] = 0;
     }
  }
  else if (uvr_typ == UVR61_3) /* UVR61-3 */
  {
    for(z=1;z<4;z++)
    {
//!!!!!!!!!!!!!      if (tstbit( akt_daten[13], z-1 ) == 1)
//!!!!!!!!!!!!!        AUSG[z] = 1;
//!!!!!!!!!!!!!      else
//!!!!!!!!!!!!!        AUSG[z] = 0;
     }
  }

}

/* Bearbeitung Drehzahlstufen */
void drehzahlstufenanz(void)
{
//!!!!!!!!!!!!!  if (uvr_typ == UVR1611) /* UVR1611 */
  {
//!!!!!!!!!!!!!    if ( ((akt_daten[35] & UVR1611) == 0 ) && (akt_daten[35] != 0 ) ) /* ist das hochwertigste Bit gesetzt ? */
    {
//!!!!!!!!!!!!!      DZR[1] = 1;
//!!!!!!!!!!!!!      DZStufe[1] = akt_daten[35] & 0x1F; /* die drei hochwertigsten Bits loeschen */
    }
//!!!!!!!!!!!!!    else
    {
//!!!!!!!!!!!!!      DZR[1] = 0;
//!!!!!!!!!!!!!      DZStufe[1] = 0;
    }
//!!!!!!!!!!!!!    if ( ((akt_daten[36] & UVR1611) == 0 )  && (akt_daten[36] != 0 ) )/* ist das hochwertigste Bit gesetzt ? */
    {
//!!!!!!!!!!!!!      DZR[2] = 1;
//!!!!!!!!!!!!!      DZStufe[2] = akt_daten[36] & 0x1F; /* die drei hochwertigsten Bits loeschen */
    }
//!!!!!!!!!!!!!    else
    {
//!!!!!!!!!!!!!      DZR[2] = 0;
//!!!!!!!!!!!!!      DZStufe[2] = 0;
    }
//!!!!!!!!!!!!!    if ( ((akt_daten[37] & UVR1611) == 0 )  && (akt_daten[37] != 0 ) )/* ist das hochwertigste Bit gesetzt ? */
    {
//!!!!!!!!!!!!!      DZR[6] = 1;
//!!!!!!!!!!!!!      DZStufe[6] = akt_daten[37] & 0x1F; /* die drei hochwertigsten Bits loeschen */
    }
//!!!!!!!!!!!!!    else
    {
//!!!!!!!!!!!!!      DZR[6] = 0;
//!!!!!!!!!!!!!      DZStufe[6] = 0;
    }
//!!!!!!!!!!!!!    if (akt_daten[38] != UVR1611 ) /* angepasst: bei UVR1611 ist die Pumpe aus! */
    {
//!!!!!!!!!!!!!      DZR[7] = 1;
//!!!!!!!!!!!!!      DZStufe[7] = akt_daten[38] & 0x1F; /* die drei hochwertigsten Bits loeschen */
    }
//!!!!!!!!!!!!!    else
    {
//!!!!!!!!!!!!!      DZR[7] = 0;
//!!!!!!!!!!!!!      DZStufe[7] = 0;
    }
  }
//!!!!!!!!!!!!!    else if (uvr_typ == UVR61_3) /* UVR61-3 */
  {
//!!!!!!!!!!!!!    if ( ((akt_daten[14] & UVR61_3) == 0 ) && (akt_daten[14] != 0 ) ) /* ist das hochwertigste Bit gesetzt ? */
    {
//!!!!!!!!!!!!!      DZR[1] = 1;
//!!!!!!!!!!!!!      DZStufe[1] = akt_daten[14] & 0x1F; /* die drei hochwertigsten Bits loeschen */
    }
//!!!!!!!!!!!!!    else
    {
//!!!!!!!!!!!!!      DZR[1] = 0;
//!!!!!!!!!!!!!      DZStufe[1] = 0;
    }
  }
}

/* Bearbeitung Waermemengen-Register und -Zaehler */
void waermemengenanz(void)
{
  //  float momentLstg1, kwh1, mwh1, momentLstg2, kwh2, mwh2;
//!!!!!!!!!!!!!  WMReg[1] = 0;
//!!!!!!!!!!!!!  WMReg[2] = 0;
//!!!!!!!!!!!!!  int tmp_wert;

  if (uvr_typ == UVR1611) /* UVR1611 */
  {
//!!!!!!!!!!!!!    switch(akt_daten[39])
//!!!!!!!!!!!!!    {
//!!!!!!!!!!!!!    case 1: WMReg[1] = 1; break; /* Waermemengenzaehler1 */
//!!!!!!!!!!!!!    case 2: WMReg[2] = 1; break; /* Waermemengenzaehler2 */
//!!!!!!!!!!!!!    case 3: WMReg[1] = 1;        /* Waermemengenzaehler1 */
//!!!!!!!!!!!!!      WMReg[2] = 1;  break; /* Waermemengenzaehler2 */
//!!!!!!!!!!!!!    }

//!!!!!!!!!!!!!    if (WMReg[1] == 1)
    {
//!!!!!!!!!!!!!      if ( akt_daten[43] > 0x7f ) /* negtive Wete */
      {
//!!!!!!!!!!!!!        tmp_wert = (10*((65536*(float)akt_daten[43]+256*(float)akt_daten[42]+(float)akt_daten[41])-65536)-((float)akt_daten[40]*10)/256);
//!!!!!!!!!!!!!        tmp_wert = tmp_wert | 0xffff0000;
//!!!!!!!!!!!!!        Mlstg[1] = tmp_wert / 100;
      }
//!!!!!!!!!!!!!      else
//!!!!!!!!!!!!!        Mlstg[1] = (10*(65536*(float)akt_daten[43]+256*(float)akt_daten[42]+(float)akt_daten[41])+((float)akt_daten[40]*10)/256)/100;
//!!!!!!!!!!!!!      W_kwh[1] = ( (float)akt_daten[45]*256 + (float)akt_daten[44] )/10;
//!!!!!!!!!!!!!      W_Mwh[1] = (akt_daten[47]*0x100 + akt_daten[46]);
    }
//!!!!!!!!!!!!!    if (WMReg[2] == 1)
    {
//!!!!!!!!!!!!!      if ( akt_daten[51] > 0x7f )  /* negtive Wete */
      {
//!!!!!!!!!!!!!        tmp_wert = (10*((65536*(float)akt_daten[51]+256*(float)akt_daten[50]+(float)akt_daten[49])-65536)-((float)akt_daten[48]*10)/256);
//!!!!!!!!!!!!!        tmp_wert = tmp_wert | 0xffff0000;
//!!!!!!!!!!!!!        Mlstg[2] = tmp_wert / 100;
      }
//!!!!!!!!!!!!!      else
//!!!!!!!!!!!!!        Mlstg[2] = (10*(65536*(float)akt_daten[51]+256*(float)akt_daten[50]+(float)akt_daten[49])+((float)akt_daten[48]*10)/256)/100;
//!!!!!!!!!!!!!      W_kwh[2] = ((float)akt_daten[53]*256 + (float)akt_daten[52])/10;
//!!!!!!!!!!!!!      W_Mwh[2] = (akt_daten[55]*0x100 + akt_daten[54]);
    }
  }

  if (uvr_typ == UVR61_3) /* UVR61-3 */
  {
//!!!!!!!!!!!!!    if (akt_daten[16] == 1) /* ( tstbit(akt_daten[16],0) == 1)  akt_daten[17] und akt_daten[18] sind Volumenstrom */
//!!!!!!!!!!!!!      WMReg[1] = 1;
//!!!!!!!!!!!!!    if (WMReg[1] == 1)
    {
//!!!!!!!!!!!!!      Mlstg[1] = (256*(float)akt_daten[20]+(float)akt_daten[19])/10;
//!!!!!!!!!!!!!      W_kwh[1] = ( (float)akt_daten[22]*256 + (float)akt_daten[21] )/10;
//!!!!!!!!!!!!!      W_Mwh[1] = akt_daten[26]*0x1000000 + akt_daten[25]*0x10000 + akt_daten[24]*0x100 + akt_daten[23];
    }
  }
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
  int sendbuf[1];

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
