/*****************************************************************************
 * Speicherkriterium im UVR1611 einstellen                                   *
 * (c) 2006,2007 H. Roemer                                                   *
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
 * Version 0.1		03.09.2006 erste Version                                 *
 * Version 0.2		06.09.2006                                               *
 * 						Versionsabfrage vor eigentlichem Senden des neuen    *
 *                      Wertes eingefuegt.                                   *
 *                      Das Senden der beiden Byte zum Einstellen des neuen  *
 *                      Wertes duerfen nicht gleichzeitig in einem           *
 *                      write-Befehl geschrieben werden, sondern einzeln     *
 *                      nacheinander, also zuerst Kennung SETUP und          *
 *                      anschliessend den eigentlichen neuen Wert.           *
 * Version 0.3		18.02.2007                                               *
 * 						Argumentenuebergabe angepasst an Christoph's Version *
 * 						Argumentenbereiche ueberarbeitet,                    *
 * 						IP-Kommunikation zugefuegt                           *
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
#include <ctype.h>
#include <getopt.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BAUDRATE B115200
#define SETUP 0x96
#define VERSIONSABFRAGE 0x81

#define BUFFER_SIZE 1024

extern char *optarg;
extern int optind, opterr, optopt;

int check_arg_getopt(int arg_c, char *arg_v[]);

int fd,  write_erg;
struct sockaddr_in SERVER_sockaddr_in; /* fuer ip-Zurgriff */
char dlport[13]; /* Uebergebener Paramter USB-Port */

int ip_zugriff;
int usb_zugriff;
int uvr_modus;
int sock;
unsigned char setup_wert;


int main(int argc, char *argv[])
{
	ip_zugriff = 0;
  	usb_zugriff = 0;

	struct termios oldtio; /* will be used to save old port settings */
	struct termios newtio; /* will be used for new port settings */
	unsigned char empfbuf[2], punkt, sendbuf[2];
	int erg_check_arg, result;
	int send_bytes;       /*  sendebuffer fuer die Request-Commandos*/

	punkt = (unsigned char)0x2e;
	erg_check_arg = check_arg_getopt(argc, argv);

  	if( erg_check_arg != 1 )
    	exit(-1);

	if (usb_zugriff)
	{
    	/* first open the serial port for reading and writing */
    	fd = open(dlport, O_RDWR | O_NOCTTY );
    	if (fd < 0)
    	{
        	perror(dlport);
        	exit(-1);
    	}
    	/* save current port settings */
    	tcgetattr(fd,&oldtio);
    	/* initialize the port settings structure to all zeros */
    	//bzero(&newtio, sizeof(newtio));
	memset( &newtio, 0, sizeof(newtio) );
    	/* then set the baud rate, handshaking and a few other settings */
    	newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
    	newtio.c_iflag = IGNPAR;
    	newtio.c_oflag = 0;
    	/* set input mode (non-canonical, no echo,...) */
    	newtio.c_lflag = 0;
    	newtio.c_cc[VTIME]    = 0;   /* inter-character timer unused */
    	newtio.c_cc[VMIN]     = 1;   /* blocking read until first char received */

	    tcflush(fd, TCIFLUSH);
	    tcsetattr(fd,TCSANOW,&newtio);

		sendbuf[0]=VERSIONSABFRAGE; 	/* Senden der Versionsabfrage */
		write_erg=write(fd,sendbuf,1);
		if (write_erg == 1)		/* Lesen der Antwort*/
			result=read(fd,empfbuf,1);

		sendbuf[0]=SETUP; 	/* Senden Request Setup */
		write_erg=write(fd,sendbuf,1);
		if (write_erg == 1)		/* Lesen der Antwort*/
		{
			sendbuf[0]=setup_wert;
			write_erg=write(fd,sendbuf,1);
			if (write_erg == 1)		/* Lesen der Antwort*/
			{
				result=read(fd,empfbuf,1);
				if(sendbuf[0] == empfbuf[0])
					printf("Senden des neuen Speicherkriteriums war erfolgreich. \n");
				else
					printf("Senden des neuen Speicherkriteriums war nicht erfolgreich. Returnwert: 0x%x\n",empfbuf[0]);
			}
		}
	    /* restore the old port settings before quitting */
    	tcsetattr(fd,TCSANOW,&oldtio);

    	return(0);
	} /* Ende usb_zugriff */

	if (ip_zugriff)
	{
		sock = socket(PF_INET, SOCK_STREAM, 0);
    	if (sock == -1)
		{
	  		perror("socket failed()");
	  		return 2;
		}
	    if (connect(sock, (const struct sockaddr *)&SERVER_sockaddr_in, sizeof(SERVER_sockaddr_in)) == -1)
		{
	  		perror("connect failed()");
	  		return 3;
		}

		sendbuf[0]=VERSIONSABFRAGE; 	/* Senden der Versionsabfrage */
		send_bytes=send(sock,sendbuf,1,0);
		if (send_bytes == 1)		/* Lesen der Antwort */
		{
			result  = recv(sock,empfbuf,1,0);
//			fprintf(stderr,"Version erhalten: %x\n",empfbuf[0]);
			if ( empfbuf[0] == 0xD1 )
			{
				fprintf(stderr,"BL im Datenloggingmodus 2 Geraete wird nicht unterstuetzt.\n");
				return(-4);
			}
		}

		sendbuf[0]=SETUP; 	/* Senden Request Setup */
		sendbuf[1]=setup_wert;
		send_bytes=send(sock,sendbuf,2,0);
		if (send_bytes == 2)		/* Lesen der Antwort */
		{
//			fprintf(stderr,"Setup_Wert: %x\n",sendbuf[1]);
			result  = recv(sock,empfbuf,1,0);
			if (result == 1)
			{
				if(sendbuf[1] == empfbuf[0])
					printf("Senden des neuen Speicherkriteriums war erfolgreich. \n");
				else
					printf("Senden des neuen Speicherkriteriums war nicht erfolgreich. Returnwert: 0x%x\n",empfbuf[0]);
			}
		}

		return(0);
	}

	return(-5);
} /* Ende main */

/* Statische Bildschirmausgabe / Hilfetext */
static int print_usage()
{
  	fprintf(stderr,"\n    Speicherkriterium im UVR1611 einstellen \n");
  	fprintf(stderr,"    Version 0.3 vom 18.02.2007 \n");
  	fprintf(stderr,"\nsetup-dlogg (-p USB-Port | -i IP:Port) -w Wert [-h] [-v]\n");
  	fprintf(stderr,"    -p USB-Port -> Angabe des USB-Portes,\n");
  	fprintf(stderr,"                   an dem der D-LOGG angeschlossen ist.\n");
  	fprintf(stderr,"    -i IP:Port  -> Angabe der IP-Adresse und des Ports,\n");
  	fprintf(stderr,"                   an dem der BL-Net angeschlossen ist.\n");
  	fprintf(stderr,"          Wert  -> das Speicherkriterium, nachdem UVR1611 die Daten aufzeichnet\n");
  	fprintf(stderr,"                   Format: entweder xx:yy (min:sec) \n");
  	fprintf(stderr,"                   im Bereich von 20 sec bis max. 40 min\n");
  	fprintf(stderr,"                   oder x,y im Bereich von 0,5 bis 12 Grad Kelvin\n");
  	fprintf(stderr,"          -h    -> diesen Hilfetext\n");
  	fprintf(stderr,"          -v    -> Versionsangabe\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"Beispiel: setup-dlogg -p /dev/ttyUSB0 -w 1:20\n");
	fprintf(stderr,"          Setzt das Speicherkriterium im UVR1611 auf 80 Sekunden.\n");
	fprintf(stderr,"Beispiel: setup-dlogg -i 192.168.1.1:40000 -w 3,0\n");
	fprintf(stderr,"          Setzt das Speicherkriterium im UVR1611 auf eine Temperaturdifferenz\n");
	fprintf(stderr,"          von 3 Grad Kelvin.\n\n");
	fprintf(stderr,"\n");
  return 0;
}

/* Ueberpruefung der uebergebenen Argumente und entsprechende Aktions-Varaiblen fuellen */
/* Flag zur Auswahl des Formates der Logdatei (csv/winsol) */
/* Flag "Reset des DL" nach dem Einlesen der Daten */
int check_arg_getopt(int arg_c, char *arg_v[])
{
  int min, sek, vor_komma, nach_komma;
  int c = 0;
  int p_is_set=-1;
  int i_is_set=-1;
  int w_is_set=-1;
  unsigned char float_setup_wert;
  unsigned char doppelpunkt = 0x3a;
  unsigned char komma = 0x2c;

  char trennzeichen[] = ":";
  char trennkomma[] = ",";

  /* arbeitet alle argumente ab  */
  while (1)
    {

      int option_index = 0;
      static struct option long_options[] =
	{
	  {0, 0, 0, 0}
	};

      c = getopt_long(arg_c, arg_v, "hvi:p:w:", long_options, &option_index);


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
		fprintf(stderr,"\n    Speicherkriterium im UVR1611 einstellen \n");
  		fprintf(stderr,"    Version 0.3 vom 18.02.2007 \n");
	    printf("\n");
	    return -1;
	  }


	case 'h':
	case '?':
	  print_usage();
	  return -1;

	case 'p': /* USB */
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
			fprintf(stderr," portname zu lang: %s\n",optarg);
			print_usage();
		return -1;
	      }
	    break;
	  }

	case 'i': /* IP */
	  {
	    if ( optarg && strlen(optarg) < 22 && strlen(optarg) > 6 )
	      {
	      	char * c = NULL;
	      	c=strtok(optarg,trennzeichen);
	      	if ( c )
	      	{
	      		SERVER_sockaddr_in.sin_addr.s_addr = inet_addr(c);
	      		c=strtok(NULL,trennzeichen);
	      		if ( c )
	      			SERVER_sockaddr_in.sin_port = htons((unsigned short int) atol(c) );
	      		else
	      		{
	      			fprintf(stderr," IP-Adresse falsch: %s\n",optarg);
	      			return -1;
	      		}
	      	}
	      	else
	      	{
	      		fprintf(stderr," IP-Adresse falsch: %s\n",optarg);
	      		return -1;
	      	}
	      	SERVER_sockaddr_in.sin_family = AF_INET;
	      	i_is_set=1;
	      	ip_zugriff = 1;
	      }
	    else
	      {
	      	if ( optarg)
				fprintf(stderr," IP-Adresse zu lang: %s\n",optarg);
			else
				fprintf(stderr," keine IP-Adresse angegeben!\n");

		print_usage();
		return -1;
	      }

	    break;
	  }

	case 'w':
	  {
	    if ( optarg && strlen(optarg) < 6 && strlen(optarg) > 2 )
	      {
	      	char * c = NULL;
		  if( strchr(optarg, doppelpunkt) != NULL)
			{
				c=strtok(optarg,trennzeichen);
				if (c)
				{
	      			min = atoi(c);
	      			c=strtok(NULL,trennzeichen);
	      			if (c)
	      				sek = atoi(c);
	      			else
	      			{
						fprintf(stderr," Das Zeitkriterium ist falsch: %s\n",optarg);
						return -1;
	      			}
				}
				else
				{
					fprintf(stderr," Das Zeitkriterium ist falsch: %s\n",optarg);
					return -1;
				}
				if ( (min >= 40  && sek > 0) || (min < 1 && sek < 20 ) )
				{
					fprintf(stderr," Das Zeitkriterium ist zu falsch!\n");
					return -1;
				}
				setup_wert= 128+(((min*60)+sek)/20);
				w_is_set=1;
			}
		  if( strchr(optarg, komma) != NULL)
			{
				c=strtok(optarg,trennkomma);
				if (c)
				{
					vor_komma = atoi(c);
					c=strtok(NULL,trennkomma);
					if (c)
					{
						nach_komma = atoi(c);
	      				if ( nach_komma > 9)
	      				{
							fprintf(stderr," Zu hoher Nachkommawert (Temperatur) im Speicherkriterium!\n");
							return -1;
	      				}
					}
					else
					{
						fprintf(stderr," Kein Nachkommawert (Temperatur) im Speicherkriterium!\n");
						return -1;
					}
				}
	      		else
				{
					fprintf(stderr," Kein Temperaturwert im Speicherkriterium!\n");
					return -1;
				}
				float_setup_wert=vor_komma*10 + nach_komma;
//				fprintf(stderr,"float_setup_wert: %i  -  %x \n",float_setup_wert,float_setup_wert);
				if (float_setup_wert > 120)
				{
					fprintf(stderr,"Zu hoher Temperaturwert im Speicherkriterium!\n");
					return -1;
				}
				if (float_setup_wert < 5)
				{
					fprintf(stderr,"Zu kleiner Temperaturwert im Speicherkriterium!\n");
					return -1;
				}
				setup_wert=float_setup_wert*10;
				w_is_set=1;
			}
	      }
		else
	    {
	    	if ( optarg)
				fprintf(stderr," Falsche Parameterangabe:  -w %s \n",optarg);
			else
				fprintf(stderr," Kein Speicherkriterium angegeben!\n");

			print_usage();
			return -1;
	     }
	    break;
	  }

	case 0:
	  {
	    break;
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
      fprintf(stderr,"\n USB-Port- oder IP-Angabe fehlt!\n");
      print_usage();
      return -1;
    }
  if (( p_is_set  > 0) && ( i_is_set > 0 ) )
    {
      fprintf(stderr,"\n Auslesen nicht gleichzeitig von USB-Port- und IP-Port moeglich!\n");
      print_usage();
      return -1;
    }
  if ( w_is_set < 1)
    {
      fprintf(stderr,"\n Das neue Speicherkriterium fehlt oder ist ungueltig!\n");
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
