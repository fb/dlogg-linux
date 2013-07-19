/********************************************************/
/* Konvertierung Winsol-LogDatei in CVS- oder SQL-Datei */
/* (c) H. Roemer                                        */
/* Version 0.2  25.10.2006                              */
/* Version 0.3  11.01.2007                              */
/* Version 0.4  27.01.2008                              */
/* Version 0.5  10.07.2013                              */
/********************************************************/

//#define WINDOWS  /* unter Windows Absturz */
#define LINUX

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ctype.h>
#include <locale.h>
#include <string.h>
#include "../dl-lesen.h"

#ifdef WINDOWS
  #include <io.h>
#endif
#ifdef LINUX
  #include <unistd.h>
#endif

#define UVR61_3 0x90
#define UVR1611 0x80

typedef union{
      short short_word;
      struct
      {
        char lowbyte;
        char highbyte;
      } bytes;
    } BYTES2SHORT;

void aufrufhinweis();
void error_abbruch();
void ausgangsbyte1_belegen(char aus_byte, int uvr_typ);
void ausgangsbyte2_belegen(char aus_byte);
void get_JahrMonat(char *logfilename);
int eingangsparameter(UCHAR highbyte);
float berechnetemp(UCHAR lowbyte, UCHAR highbyte, int sensor);
float berechnevol(UCHAR lowbyte, UCHAR highbyte);
void drehzahlstufen(UCHAR buffer[],int uvr_typ);
void waermemengen(UCHAR buffer[],int uvr_typ);
int clrbit( int word, int bit );
int tstbit( int word, int bit );
int xorbit( int word, int bit );
int setbit( int word, int bit );
void temperaturanz(void);
void ausgaengeanz(void);
void drehzahlstufenanz(void);
void waermemengenanz(void);

int S_Art[17];/* Das erste Element [0] wird nicht beruecksichtigt, Art es EingangParameter*/
int fd_in;
FILE *fd_out;

struct WinsolLog_CSV {
    int tag;
    int monat;
    int jahr;
    int std;
    int min;
    int sec;
    int ausgang1;   /* Zustand der Ausg�nge */
    int ausgang2;   /* Zustand der Ausg�nge */
    int ausgang3;   /* Zustand der Ausg�nge */
    int ausgang4;   /* Zustand der Ausg�nge */
    int ausgang5;   /* Zustand der Ausg�nge */
    int ausgang6;   /* Zustand der Ausg�nge */
    int ausgang7;   /* Zustand der Ausg�nge */
    int ausgang8;   /* Zustand der Ausg�nge */
    int ausgang9;   /* Zustand der Ausg�nge */
    int ausgang10;  /* Zustand der Ausg�nge */
    int ausgang11;  /* Zustand der Ausg�nge */
    int ausgang12;  /* Zustand der Ausg�nge */
    int ausgang13;  /* Zustand der Ausg�nge */
    int dza1_aktiv; /* Drehzahlregelung aktiv*/
    int dza1_stufe; /* Drehzahlstufe */
    int dza2_aktiv; /* Drehzahlregelung aktiv*/
    int dza2_stufe; /* Drehzahlstufe */
    int dza6_aktiv; /* Drehzahlregelung aktiv*/
    int dza6_stufe; /* Drehzahlstufe */
    int dza7_aktiv; /* Drehzahlregelung aktiv*/
    int dza7_stufe; /* Drehzahlstufe */
    float tempt[17]; /* tempt[0] wird nicht benutzt*/
    int wmz1;       /* W�rmemengenz�hler aktiv */
    int wmz2;       /* W�rmemengenz�hler aktiv */
    float leistung1;
    float kwh1;
    float mwh1;
    float leistung2;
    float kwh2;
    float mwh2;
} struct_winsol; /* Anzahl Byte: 51 */

struct csv_UVR61_3 {
    int tag;
    int monat;
    int jahr;
    int std;
    int min;
    int sec;
    int ausgang1;   /* Zustand der Ausg�nge */
    int dza1_aktiv; /* Drehzahlregelung aktiv*/
    int dza1_stufe; /* Drehzahlstufe */
    int ausgang2;   /* Zustand der Ausg�nge */
    int ausgang3;   /* Zustand der Ausg�nge */
    float analog;     /* Analogausgang */
    float tempt[7]; /* tempt[0] wird nicht benutzt*/
    int wmz1;       /* W�rmemengenz�hler aktiv */
    float volstrom;
    float leistung1;
    float kwh1;
    float mwh1;
} struct_csv_UVR61_3;


int main(int argc, char **argv)
{
  int i, c, x, y, erg, csv, sql, wsol, tmp_i, argv4_laenge=0;
  unsigned char buffer[60], temp_byte;
  unsigned char uvr_typ;
  char tmp_kopf1[100], tmp_kopf2[100], tmp_kopf1_uvr61_3[100], tmp_kopf2_uvr61_3[100], sql_kopf[31];
  char *p_tmp_kopf1, *p_tmp_kopf2, *p_tmp_kopf1_uvr61_3, *p_tmp_kopf2_uvr61_3, *p_sql_kopf;
  char *tabelle;
  int letzter_tag = 0;

  sql = FALSE;
  csv = FALSE;
  wsol = FALSE;

  setlocale(LC_ALL, "");

  if ( argc > 1 )
  {
    if ( strcmp(argv[1], "-?") == 0 )
      aufrufhinweis();

    if ( argc==2 || argc ==3 )
      aufrufhinweis();

    if ((argc < 4) && (argc > 5))
      aufrufhinweis();
    else
    {
      if ( strcmp(argv[3], "-sql") == 0 )
      {
        sql = TRUE;
        if (argc != 5)
          aufrufhinweis();

        argv4_laenge = strlen(argv[4]);
        if  (argv4_laenge == 0)
          aufrufhinweis();
        else
        {
          tabelle = calloc(argv4_laenge+1,sizeof(char));
          strcpy(tabelle,argv[4]);
        }
      }
      else
      {
        if ( strcmp(argv[3], "-csv") == 0 )
          csv = TRUE;
        else if ( strcmp(argv[3], "-winsol") == 0 )
          wsol = TRUE;
        else
        {
          printf("Fehler bei Format-Parameter %s\n", argv[3]);
          exit(-1);
        }
      }
      if( (fd_out=fopen(argv[2],"w" )) == NULL )
      {
        printf("Fehler beim Erstellen der Datei %s\n", argv[1] );
        exit(-1);
      }
#ifdef WINDOWS
      if( (fd_in=open(argv[1], O_RDONLY | O_BINARY )) == -1 )
#endif
#ifdef LINUX
      if( (fd_in=open(argv[1], O_RDONLY )) == -1 )
#endif
      {
        printf("Fehler beim Oeffnen der Datei %s. Fehler ??? \n", argv[1] );
        exit(-1);
      }
    }
  }
  else
    aufrufhinweis();

  /*  Daten auslesen und in CVS/SQL-Form neu schreiben */
  if( (c=read(fd_in,buffer,59)) == -1 ) /* Kopfsatz lesen */
  {
    printf("Fehler beim Lesen der Logdatei.\n");
    error_abbruch();
  }
  if (wsol)
        fwrite(buffer, 59, 1, fd_out);
  uvr_typ=0;
  if (buffer[7] == 0x07)
    uvr_typ = UVR1611;
  else if  (buffer[7] == 0x06)
    uvr_typ = UVR61_3;

  p_tmp_kopf1 = tmp_kopf1;
  p_tmp_kopf2 = tmp_kopf2;
  p_tmp_kopf1_uvr61_3 = tmp_kopf1_uvr61_3;
  p_tmp_kopf2_uvr61_3 = tmp_kopf2_uvr61_3;
  p_sql_kopf = sql_kopf;
  sprintf(p_sql_kopf,"INSERT INTO %s VALUES(",tabelle);
  p_tmp_kopf1 = "Datum;Zeit;Sens1;Sens2;Sens3;Sens4;Sens5;Sens6;Sens7;Sens8;Sens9;\
Sens10;Sens11;Sens12;Sens13;Sens14;Sens15;Sens16;\
Ausg1;Drehzst_A1;Ausg2;Drehzst_A2;Ausg3;Ausg4;Ausg5;Ausg6;Drehzst_A6;Ausg7;Drehzst_A7;\
Ausg8;Ausg9;Ausg10;Ausg11;Ausg12;Ausg13;";
  p_tmp_kopf2 = "kW1;kWh1;kW2;kWh2";
  p_tmp_kopf1_uvr61_3 = "Datum;Zeit;Sens1;Sens2;Sens3;Sens4;Sens5;Sens6;\
Ausg1;Drehzst_A1;Ausg2;Ausg3;Analog;";
  p_tmp_kopf2_uvr61_3 = "volstrom;kW;kWh;";

  if (csv)
  {
    if (uvr_typ == UVR1611)
      fprintf(fd_out,"%s%s\n",p_tmp_kopf1, p_tmp_kopf2);
    else if (uvr_typ == UVR61_3)
      fprintf(fd_out,"%s%s\n",p_tmp_kopf1_uvr61_3, p_tmp_kopf2_uvr61_3);
  }

  i = 1;
  x = 10;
  tmp_i = 1;

  get_JahrMonat(argv[1]); /* Jahr und Monat aus Dateiname ermitteln */

  while( (c=read(fd_in,buffer,59)) > 0 )  /* solange lesen, bis EOF der Logdatei erreicht ist*/
  {
   /* Tag, Stunde, Minute und Sekunde */
    struct_winsol.tag = buffer[0];
    struct_csv_UVR61_3.tag = buffer[0];
    struct_winsol.std = buffer[1];
    struct_csv_UVR61_3.std = buffer[1];
    struct_winsol.min = buffer[2];
    struct_csv_UVR61_3.min = buffer[2];
    struct_winsol.sec = buffer[3];
    struct_csv_UVR61_3.sec = buffer[3];
   
   /* Pruefe Konsistenz des Datums */
    if (struct_winsol.tag<0 || struct_winsol.tag> 31 ||struct_winsol.std >23 ||
        struct_winsol.min> 59 || struct_winsol.sec> 59 ||
        struct_winsol.tag < letzter_tag || struct_winsol.tag > letzter_tag +1){
        int i;
        for (i=0; i<59; i++)
            printf("0x%02x ",buffer[i]);
        printf("\n");
        lseek(fd_in, -60, SEEK_CUR);
        continue;
    }
    letzter_tag = struct_winsol.tag;
	
    if (wsol) 
	{
        fwrite(buffer, 59, 1, fd_out);
        continue;
    }

    /* Zust�nde der Ausgangsbyte's */
    ausgangsbyte1_belegen(buffer[4],uvr_typ);
    drehzahlstufen(buffer,uvr_typ);

    if (uvr_typ == UVR1611)
    {
      ausgangsbyte2_belegen(buffer[5]);

      x = 10;
      for (y=1;y<17;y++)      /* Temperatur wird ermittelt und abgelegt */
      {
        S_Art[y] = eingangsparameter(buffer[x+1]);
        switch(S_Art[y])
        {
          case 0: struct_winsol.tempt[y] = 0; break;
          case 1: struct_winsol.tempt[y] = 0; break; // digit. Pegel (AUS)
          case 2: struct_winsol.tempt[y] = berechnetemp(buffer[x],buffer[x+1],S_Art[y]); break; // Temp.
          case 3: struct_winsol.tempt[y] = berechnevol(buffer[x],buffer[x+1]); break;
          case 6: struct_winsol.tempt[y] = berechnetemp(buffer[x],buffer[x+1],S_Art[y]); break; // Strahlung
          case 7: struct_winsol.tempt[y] = berechnetemp(buffer[x],buffer[x+1],S_Art[y]); break; // Raumtemp.
          case 9: struct_winsol.tempt[y] = 1; break; // digit. Pegel (EIN)
          case 10: struct_winsol.tempt[y] = berechnetemp(buffer[x],buffer[x+1],S_Art[y]); break; // Minus-Temperaturen
          case 15: struct_winsol.tempt[y] = berechnetemp(buffer[x],buffer[x+1],S_Art[y]); break; // Minus-Raumtemp.
        }
        x= x +2;
      }

      struct_winsol.wmz1 = 0;
      struct_winsol.wmz2 = 0;

      switch(buffer[42])
      {
        case 1: struct_winsol.wmz1 = 1; break; /* Waermemengenzaehler1 */
        case 2: struct_winsol.wmz2 = 1; break; /* Waermemengenzaehler2 */
        case 3: struct_winsol.wmz1 = 1;        /* Waermemengenzaehler1 */
                struct_winsol.wmz2 = 1;  break; /* Waermemengenzaehler2 */
      }
    }
    if (uvr_typ == UVR61_3)
    {
      x = 7;
      for (y=1;y<7;y++)      /* Temperatur wird ermittelt und abgelegt */
      {
        S_Art[y] = eingangsparameter(buffer[x+1]);
//      printf("S_Art[%x] = %x \n",y,S_Art[y]);
        switch(S_Art[y])
        {
          case 0: struct_csv_UVR61_3.tempt[y] = 0; break;
          case 1: struct_csv_UVR61_3.tempt[y] = 0; break; // digit. Pegel (AUS)
          case 2: struct_csv_UVR61_3.tempt[y] = berechnetemp(buffer[x],buffer[x+1],S_Art[y]); break; // Temp.
          case 3: struct_csv_UVR61_3.tempt[y] = berechnevol(buffer[x],buffer[x+1]);
                  struct_csv_UVR61_3.volstrom = struct_csv_UVR61_3.tempt[y]; break;
          case 6: struct_csv_UVR61_3.tempt[y] = berechnetemp(buffer[x],buffer[x+1],S_Art[y]); break; // Strahlung
          case 7: struct_csv_UVR61_3.tempt[y] = berechnetemp(buffer[x],buffer[x+1],S_Art[y]); break; // Raumtemp.
          case 9: struct_csv_UVR61_3.tempt[y] = 1; break; // digit. Pegel (EIN)
          case 10: struct_csv_UVR61_3.tempt[y] = berechnetemp(buffer[x],buffer[x+1],S_Art[y]); break; // Minus-Temperaturen
          case 15: struct_csv_UVR61_3.tempt[y] = berechnetemp(buffer[x],buffer[x+1],S_Art[y]); break; // Minus-Raumtemp.
        }
        x= x +2;
      }

      if (buffer[23] == 1)
      {
        struct_csv_UVR61_3.wmz1 = 1;
        temp_byte = buffer[6] & ~(1 << 8); /* oberestes Bit auf 0 setzen */
        struct_csv_UVR61_3.analog = temp_byte & ~(1 << 7);
        temp_byte = struct_csv_UVR61_3.analog;
        struct_csv_UVR61_3.analog = temp_byte & ~(1 << 6);
      }
      else
      {
        struct_csv_UVR61_3.wmz1 = 0;
        struct_csv_UVR61_3.analog = 0;
      }

    }

    waermemengen(buffer, uvr_typ);
    if (csv)
    {
      if (uvr_typ == UVR1611)
        erg = fprintf(fd_out,"%02d.%02d.%4d; %02d:%02d:%02d; \
%.1f; %.1f; %.1f; %.1f; \
%.1f; %.1f; %.1f; %.1f; \
%.1f; %.1f; %.1f; %.1f; \
%.1f; %.1f; %.1f; %.1f; \
%i; %i; %i; %i; \
%i; %i; %i; \
%i; %i; %i; %i; \
%i; %i; %i; \
%i; %i; %i; \
%.1f; %.1f; %.1f; %.1f;\n",
// %.1f; %.f%.1f; %.1f; %.f%.1f;\n",
        struct_winsol.tag,struct_winsol.monat,struct_winsol.jahr,struct_winsol.std,struct_winsol.min,struct_winsol.sec,
        struct_winsol.tempt[1],struct_winsol.tempt[2],struct_winsol.tempt[3],struct_winsol.tempt[4],
        struct_winsol.tempt[5],struct_winsol.tempt[6],struct_winsol.tempt[7],struct_winsol.tempt[8],
        struct_winsol.tempt[9],struct_winsol.tempt[10],struct_winsol.tempt[11],struct_winsol.tempt[12],
        struct_winsol.tempt[13],struct_winsol.tempt[14],struct_winsol.tempt[15],struct_winsol.tempt[16],
        struct_winsol.ausgang1,struct_winsol.dza1_stufe,struct_winsol.ausgang2,struct_winsol.dza2_stufe,
        struct_winsol.ausgang3,struct_winsol.ausgang4,struct_winsol.ausgang5,
        struct_winsol.ausgang6,struct_winsol.dza6_stufe,struct_winsol.ausgang7,struct_winsol.dza7_stufe,
        struct_winsol.ausgang8,struct_winsol.ausgang9,struct_winsol.ausgang10,
        struct_winsol.ausgang11,struct_winsol.ausgang12,struct_winsol.ausgang13,
//        struct_winsol.leistung1,struct_winsol.mwh1,struct_winsol.kwh1,
        struct_winsol.leistung1,(struct_winsol.mwh1*1000 + struct_winsol.kwh1),
//        struct_winsol.leistung2,struct_winsol.mwh2,struct_winsol.kwh2);
        struct_winsol.leistung2,(struct_winsol.mwh2*1000 + struct_winsol.kwh2));

      if (uvr_typ == UVR61_3)
        erg = fprintf(fd_out,"%02d.%02d.%4d; %02d:%02d:%02d; \
%.1f; %.1f; %.1f; %.1f; %.1f; %.1f; \
%i; %.1i; %i; %i; %.1f; \
%.1f; %.1f; %.1f;\n",
//%.1f; %.1f; %.f%.1f;\n",
        struct_csv_UVR61_3.tag,struct_csv_UVR61_3.monat,struct_csv_UVR61_3.jahr,struct_csv_UVR61_3.std,struct_csv_UVR61_3.min,struct_csv_UVR61_3.sec,
        struct_csv_UVR61_3.tempt[1],struct_csv_UVR61_3.tempt[2],struct_csv_UVR61_3.tempt[3],
        struct_csv_UVR61_3.tempt[4],struct_csv_UVR61_3.tempt[5],struct_csv_UVR61_3.tempt[6],
        struct_csv_UVR61_3.ausgang1,struct_csv_UVR61_3.dza1_stufe,
        struct_csv_UVR61_3.ausgang2,struct_csv_UVR61_3.ausgang3,struct_csv_UVR61_3.analog,
//        struct_csv_UVR61_3.volstrom,struct_csv_UVR61_3.leistung1,struct_csv_UVR61_3.mwh1,struct_csv_UVR61_3.kwh1);
        struct_csv_UVR61_3.volstrom,struct_csv_UVR61_3.leistung1,(struct_csv_UVR61_3.mwh1*1000 +struct_csv_UVR61_3.kwh1));
    }

    if (sql)
    {
      if (uvr_typ == UVR1611)
        erg = fprintf(fd_out,"%s'%04d-%02d-%02d','%02d:%02d:%02d',\
'%.1f', '%.1f', '%.1f', '%.1f', '%.1f', '%.1f', '%.1f', '%.1f', '%.1f', '%.1f', '%.1f', '%.1f', '%.1f', '%.1f', '%.1f', '%.1f', \
'%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', '%i', \
'%i', '%i', '%.1f', '%.1f', '%.1f', '%.1f');\n",
//'%i', '%i', '%.1f', '%.f%.1f', '%.1f', '%.f%.1f');\n",
        p_sql_kopf,struct_winsol.jahr,struct_winsol.monat,struct_winsol.tag,struct_winsol.std,struct_winsol.min,struct_winsol.sec, // Datum und Uhrzeit
        struct_winsol.tempt[1],struct_winsol.tempt[2],struct_winsol.tempt[3],struct_winsol.tempt[4],struct_winsol.tempt[5],
        struct_winsol.tempt[6],struct_winsol.tempt[7],struct_winsol.tempt[8],struct_winsol.tempt[9],struct_winsol.tempt[10],
        struct_winsol.tempt[11],struct_winsol.tempt[12],struct_winsol.tempt[13],struct_winsol.tempt[14],struct_winsol.tempt[15],struct_winsol.tempt[16],
        struct_winsol.ausgang1,struct_winsol.dza1_stufe,struct_winsol.ausgang2,struct_winsol.dza2_stufe,
        struct_winsol.ausgang3,struct_winsol.ausgang4,struct_winsol.ausgang5,
        struct_winsol.ausgang6,struct_winsol.dza6_stufe,struct_winsol.ausgang7,struct_winsol.dza7_stufe,
        struct_winsol.ausgang8,struct_winsol.ausgang9,struct_winsol.ausgang10,
        struct_winsol.ausgang11,struct_winsol.ausgang12,struct_winsol.ausgang13,
        struct_winsol.wmz1,struct_winsol.wmz2,
//        struct_winsol.leistung1,struct_winsol.mwh1,struct_winsol.kwh1,
        struct_winsol.leistung1,(struct_winsol.mwh1*1000 + struct_winsol.kwh1),
//        struct_winsol.leistung2,struct_winsol.mwh2,struct_winsol.kwh2);
        struct_winsol.leistung2,(struct_winsol.mwh2*1000 + struct_winsol.kwh2));

      if (uvr_typ == UVR61_3)
        erg = fprintf(fd_out,"%s'%04d-%02d-%02d','%02d:%02d:%02d',\
'%.1f', '%.1f', '%.1f', '%.1f', '%.1f', '%.1f', \
'%i', '%.1i', '%i', '%i', '%.1f', \
'%i', '%.1f', '%.1f', '%.1f');\n",
//'%i', '%.1f', '%.1f', '%.f%.1f');\n",
        p_sql_kopf,struct_csv_UVR61_3.jahr,struct_csv_UVR61_3.monat,struct_csv_UVR61_3.tag,struct_csv_UVR61_3.std,struct_csv_UVR61_3.min,struct_csv_UVR61_3.sec,
        struct_csv_UVR61_3.tempt[1],struct_csv_UVR61_3.tempt[2],struct_csv_UVR61_3.tempt[3],
        struct_csv_UVR61_3.tempt[4],struct_csv_UVR61_3.tempt[5],struct_csv_UVR61_3.tempt[6],
        struct_csv_UVR61_3.ausgang1,struct_csv_UVR61_3.dza1_stufe,
        struct_csv_UVR61_3.ausgang2,struct_csv_UVR61_3.ausgang3,struct_csv_UVR61_3.analog,
        struct_csv_UVR61_3.wmz1,struct_csv_UVR61_3.volstrom,
//        struct_csv_UVR61_3.leistung1,struct_csv_UVR61_3.mwh1,struct_csv_UVR61_3.kwh1);
        struct_csv_UVR61_3.leistung1,(struct_csv_UVR61_3.mwh1*1000 +struct_csv_UVR61_3.kwh1));
    }

        tmp_i++;
      i++;
//  printf("Es wurden %i Datensaetze geschrieben.\n",i);
    }

//      if (tmp_i == 5)
//      break;

  printf("Es wurden %i Datensaetze geschrieben.\n",i);

  c=fclose(fd_out);
  c=close(fd_in);
  exit(0);
}


void aufrufhinweis()
{
  printf("\nBenutzung: winsol2csv Logdatei Ausgabedatei -csv | -winsol | -sql Tabelle \n");
  printf("           Es dürfen keine Pfadangaben bei [Logdatei] angegeben werden!\n");
  printf("-csv          - speichern als CSV-Datei\n");
  printf("-winsol       - speichern als Winsol Datei ggf. nach Resynchronisation\n");
  printf("-sql Tabelle  - speichern als SQL-Import-Datei\n");
  printf("     Tabelle  - Tabellenname der (My)SQL-Dattenbank\n");
  printf("Beispiel:  winsol2csv Y200511.log 200511.csv -csv\n");
  printf("           winsol2csv Y200511.log 200511.sql -sql UVR_1611\n\n");
  exit(-1);
}

void error_abbruch()
{
  int c;
  c=fclose(fd_out);
  c=close(fd_in);
  exit(-1);
}

void ausgangsbyte1_belegen(char aus_byte, int uvr_typ)
{
  if (uvr_typ == UVR1611)
  {
    if ((aus_byte & 0x01) != 0)
      struct_winsol.ausgang1 = 1;
    else
      struct_winsol.ausgang1 = 0;

    if ((aus_byte & 0x02) != 0)
      struct_winsol.ausgang2 = 1;
    else
      struct_winsol.ausgang2 = 0;

    if ((aus_byte & 0x04) != 0)
      struct_winsol.ausgang3 = 1;
    else
      struct_winsol.ausgang3 = 0;

    if ((aus_byte & 0x08) != 0)
      struct_winsol.ausgang4 = 1;
    else
      struct_winsol.ausgang4 = 0;

    if ((aus_byte & 0x10) != 0)
      struct_winsol.ausgang5 = 1;
    else
      struct_winsol.ausgang5 = 0;

    if ((aus_byte & 0x20) != 0)
      struct_winsol.ausgang6 = 1;
    else
      struct_winsol.ausgang6 = 0;

    if ((aus_byte & 0x40) != 0)
      struct_winsol.ausgang7 = 1;
    else
      struct_winsol.ausgang7 = 0;

    if ((aus_byte & 0x80) != 0)
      struct_winsol.ausgang8 = 1;
    else
      struct_winsol.ausgang8 = 0;
  }
  if (uvr_typ == UVR61_3)
  {
    if ((aus_byte & 0x01) != 0)
      struct_csv_UVR61_3.ausgang1 = 1;
    else
      struct_csv_UVR61_3.ausgang1 = 0;

    if ((aus_byte & 0x02) != 0)
      struct_csv_UVR61_3.ausgang2 = 1;
    else
      struct_csv_UVR61_3.ausgang2 = 0;

    if ((aus_byte & 0x04) != 0)
      struct_csv_UVR61_3.ausgang3 = 1;
    else
      struct_csv_UVR61_3.ausgang3 = 0;
  }
}

void ausgangsbyte2_belegen(char aus_byte)
{
  if ((aus_byte & 0x01) != 0)
      struct_winsol.ausgang9 = 1;
    else
      struct_winsol.ausgang9 = 0;

  if ((aus_byte & 0x02) != 0)
      struct_winsol.ausgang10 = 1;
    else
      struct_winsol.ausgang10 = 0;

  if ((aus_byte & 0x04) != 0)
      struct_winsol.ausgang11 = 1;
    else
      struct_winsol.ausgang11 = 0;

  if ((aus_byte & 0x08) != 0)
      struct_winsol.ausgang12 = 1;
    else
      struct_winsol.ausgang12 = 0;

  if ((aus_byte & 0x10) != 0)
      struct_winsol.ausgang13 = 1;
    else
      struct_winsol.ausgang13 = 0;
}

/* Ermittle Jahr und Monat aus dem Namen der Winsol-Logdatei */
void get_JahrMonat(char *logfilename)
{
    int i;
    char chjahr[5], chmon[3], *jahr, *mon, *fname;

    jahr = chjahr;
    mon = chmon;

    fname = strrchr(logfilename, '/');
    if (fname)
        fname ++;
    else
        fname = logfilename;
    chmon[0] = fname[5];
    chmon[1] = fname[6];
	chmon[2] = '\0'; /* terminate string */

    for(i=1;i<5;i++)
        jahr[i-1] = fname[i];
	jahr[4]	 = '\0'; /* terminate string */

    struct_winsol.jahr = atoi(chjahr);
    struct_winsol.monat = atoi(chmon);
    struct_csv_UVR61_3.jahr = atoi(chjahr);
    struct_csv_UVR61_3.monat = atoi(chmon);
}

/* Ermittle die Art des Eingangsparameters */
int eingangsparameter(UCHAR highbyte)
{
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

/* Berechne die Temperatur / Strahlung */
float berechnetemp(UCHAR lowbyte, UCHAR highbyte, int sensor)
{
  UCHAR tmp_highbyte;
  int z;
  short wert;

  tmp_highbyte = highbyte;
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
          tmp_highbyte = tmp_highbyte & ~(1 << z); /* die oberen 4 Bit auf 0 setzen */
        if (sensor == 6)
          return(((float)tmp_highbyte*256) + (float)lowbyte); /* Strahlung in W/m2 */
        else
          return((((float)tmp_highbyte*256) + (float)lowbyte) / 10); /* Temperatur in C */
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

//  if( (highbyte & 0x80) != 0 )
  if( (highbyte & 0x80) == 0 ) /* Volumenstrom kann nicht negativ sein*/
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

/* Bearbeitung Drehzahlstufen */
void drehzahlstufen(UCHAR buffer[], int uvr_typ)
{
  if (uvr_typ == UVR1611) /* UVR1611 */
  {
    if ( ((buffer[6] & 0x80) == 0 ) && (buffer[6] != 0 ) ) /* ist das hochwertigste Bit gesetzt ? */
    {
      struct_winsol.dza1_aktiv = 1;
      struct_winsol.dza1_stufe = buffer[6] & 0x1F; /* die drei hochwertigsten Bits loeschen */
    }
    else
    {
      struct_winsol.dza1_aktiv = 0;
      struct_winsol.dza1_stufe = 0;
    }
    if ( ((buffer[7] & 0x80) == 0 )  && (buffer[7] != 0 ) )/* ist das hochwertigste Bit gesetzt ? */
    {
      struct_winsol.dza2_aktiv = 1;
      struct_winsol.dza2_stufe = buffer[7] & 0x1F; /* die drei hochwertigsten Bits loeschen */
    }
    else
    {
      struct_winsol.dza2_aktiv = 0;
      struct_winsol.dza2_stufe = 0;
    }
    if ( ((buffer[8] & 0x80) == 0 )  && (buffer[8] != 0 ) )/* ist das hochwertigste Bit gesetzt ? */
    {
      struct_winsol.dza6_aktiv = 1;
      struct_winsol.dza6_stufe = buffer[8] & 0x1F; /* die drei hochwertigsten Bits loeschen */
    }
    else
    {
      struct_winsol.dza6_aktiv = 0;
      struct_winsol.dza6_stufe = 0;
    }
    if (buffer[9] != 0x80 ) /* angepasst: bei 0x80 ist die Pumpe aus! (?)*/
    {
      struct_winsol.dza7_aktiv = 1;
      struct_winsol.dza7_stufe = buffer[9] & 0x1F; /* die drei hochwertigsten Bits loeschen */
    }
    else
    {
      struct_winsol.dza7_aktiv = 0;
      struct_winsol.dza7_stufe = 0;
    }
  }
  else if (uvr_typ == UVR61_3) /* UVR61-3 */
  {
    if ( ((buffer[6] & 0x80) == 0 ) && (buffer[6] != 0 ) ) /* ist das hochwertigste Bit gesetzt ? */
    {
      struct_csv_UVR61_3.dza1_aktiv = 1;
      struct_csv_UVR61_3.dza1_stufe = buffer[6] & 0x1F; /* die drei hochwertigsten Bits loeschen */
    }
    else
    {
      struct_csv_UVR61_3.dza1_aktiv = 0;
      struct_csv_UVR61_3.dza1_stufe = 0;
    }
  }
}

/* Bearbeitung Waermemengen-Register und -Zaehler */
void waermemengen(UCHAR buffer[], int uvr_typ)
{
  //BYTES_LONG temp;
  int tmp_wert;

  if (uvr_typ == UVR1611)
  {
    if (struct_winsol.wmz1 == 1)
    {
      // temp.bytes.highhighbyte=buffer[46];
      // temp.bytes.highlowbyte=buffer[45];
      // temp.bytes.lowhighbyte=buffer[44];
      // temp.bytes.lowlowbyte=buffer[43];
// //      struct_winsol.leistung1 = (16777216*(float)buffer[46]+65536*(float)buffer[45]+256*(float)buffer[44]+(float)buffer[43])/100;
      // struct_winsol.leistung1 = temp.long_word;
      // struct_winsol.leistung1 = struct_winsol.leistung1/100;

      if ( buffer[46] > 0x7f ) /* negtive Wete */
      {
        tmp_wert = (10*((65536*(float)buffer[46]+256*(float)buffer[45]+(float)buffer[44])-65536)-((float)buffer[43]*10)/256);
        tmp_wert = tmp_wert | 0xffff0000;
        struct_winsol.leistung1 = tmp_wert / 100;
      }
      else
        struct_winsol.leistung1 = (10*(65536*(float)buffer[46]+256*(float)buffer[45]+(float)buffer[44])+((float)buffer[43]*10)/256)/100;
/*	  
      if (temp.long_word != 0)
      {
        printf("Momentanleistung hex: %2x%2x%2x%2x\n",buffer[43],buffer[44],buffer[45],buffer[46]);
        printf("Momentanleistung: %.2f\n",struct_winsol.leistung1);
		printf("Momentanleistung: %.2f\n",(10*(65536*(float)buffer[46]+256*(float)buffer[45]+(float)buffer[44])+((float)buffer[43]*10)/256)/100);
        printf("Momentanleistung: %.2f\n",(16777216*(float)buffer[46]+65536*(float)buffer[45]+256*(float)buffer[44]+(float)buffer[43])/100);
      }
*/
      struct_winsol.kwh1 = ( (float)buffer[48]*256 + (float)buffer[47] )/10;
      struct_winsol.mwh1 = ((float)buffer[50]*0x100 + (float)buffer[49]);
    }
    else
    {
      struct_winsol.leistung1 = 0;
      struct_winsol.kwh1 = 0;
      struct_winsol.mwh1 = 0;
    }

    if (struct_winsol.wmz2 == 1)
    {
      // temp.bytes.highhighbyte=buffer[54];
      // temp.bytes.highlowbyte=buffer[53];
      // temp.bytes.lowhighbyte=buffer[52];
      // temp.bytes.lowlowbyte=buffer[51];
// //      struct_winsol.leistung2 = (10*(65536*(float)buffer[54]+256*(float)buffer[53]+(float)buffer[52])+((float)buffer[51]*10)/256)/100;
      // struct_winsol.leistung2 = temp.long_word;
      // struct_winsol.leistung2 = struct_winsol.leistung2/100;

      if ( buffer[54] > 0x7f ) /* negtive Wete */
      {
        tmp_wert = (10*((65536*(float)buffer[54]+256*(float)buffer[53]+(float)buffer[52])-65536)-((float)buffer[51]*10)/256);
        tmp_wert = tmp_wert | 0xffff0000;
        struct_winsol.leistung2 = tmp_wert / 100;
      }
      else
        struct_winsol.leistung2 = (10*(65536*(float)buffer[54]+256*(float)buffer[53]+(float)buffer[52])+((float)buffer[51]*10)/256)/100;

      struct_winsol.kwh2 = ((float)buffer[56]*256 + (float)buffer[55])/10;
      struct_winsol.mwh2 = ((float)buffer[58]*0x100 + (float)buffer[57]);
    }
    else
    {
      struct_winsol.leistung2 = 0;
      struct_winsol.kwh2 = 0;
      struct_winsol.mwh2 = 0;
    }
  }
  if (uvr_typ == UVR61_3)
  {
    struct_csv_UVR61_3.leistung1 = ( (float)buffer[27]+(float)buffer[26]);
    struct_csv_UVR61_3.kwh1 = ( (float)buffer[29]*256 + (float)buffer[28] )/10;
    struct_csv_UVR61_3.mwh1 = ((float)buffer[32]*16777216 + (float)buffer[32]*65536 + (float)buffer[32]*256 + (float)buffer[33]);
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

