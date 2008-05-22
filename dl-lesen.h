/* Standard-Technik um Konflikte bei mehrfachem inkludieren des headers zu vermeiden */

#ifndef _DL_LESEN_H
#define _DL_LESEN_H

/*****************************************************************************/
/* Header-File fuer das Lesen der Daten aus dem Datenlogger (UVR1611)        */
/* 19.04.2006 H. Roemer      - erste Version                                 */
/* 27.01.2007 C. Dolainsky   - angepasst und optimiert                       */
/* 28.01.2007 H. Roemer      - Aenderung fuer Kopfsatz (0xD1 / 0xA8)         */
/*                           - Kopfsatz Winsol entfernt                      */
/* 17.02.2007 C. Dolainsky   - aktualisiert                                  */
/* 22.10.2007 H. Roemer      - UVR61-3 hinzugefuegt                          */
/*    11.2007 H. Roemer      - Modus 2DL (0xD1) hinzugefuegt                 */
/*****************************************************************************/

#define VERSIONSABFRAGE 0x81
#define FWABFRAGE 0x82
#define MODEABFRAGE 0x21
#define KOPFSATZLESEN 0xAA
#define AKTUELLEDATENLESEN 0xAB
#define ENDELESEN 0xAD
#define RESETDATAFLASH 0xAF
#define DATENBEREICHLESEN 0xAC
#define TRUE 1
#define FALSE 0
typedef unsigned char UCHAR;

#define NEW_WSOL_TEMP

/* Datenstruktur des Datensatzes fuer Winsol / Solstat (Logdatei) UVR1611    */
/* DSWinsol */
typedef struct {
  /* timestamp */
  UCHAR tag;
  UCHAR std;
  UCHAR min;
  UCHAR sek;

  /*  Ausgaenge durch bits: */
  /*   a8 a7 a6 a5 a4 a3 a2 a1*/
  UCHAR ausgbyte1;
  /*  leer1  leer2 leer3 a13 a12 a11 a10 a9 */
  UCHAR ausgbyte2;

  /* drehzahlwerte Ausg"ange A1 A2 A6 A7 */
  UCHAR dza[4];
#if 0
  UCHAR dza2;
  UCHAR dza6;
  UCHAR dza7;
#endif

  /* Sensor-Eingaenge */
#ifdef NEW_WSOL_TEMP
  UCHAR sensT[16][2];
#else
  UCHAR t1[2];
  UCHAR t2[2];
  UCHAR t3[2];
  UCHAR t4[2];
  UCHAR t5[2];
  UCHAR t6[2];
  UCHAR t7[2];
  UCHAR t8[2];
  UCHAR t9[2];
  UCHAR t10[2];
  UCHAR t11[2];
  UCHAR t12[2];
  UCHAR t13[2];
  UCHAR t14[2];
  UCHAR t15[2];
  UCHAR t16[2];
#endif

  /* WMZ 1/2 aktiv in bit 1 und bit2 */
  UCHAR wmzaehler_reg;

  /* Solar1: momentane Leistung, Waermemenge kWh/MWh */
  UCHAR mlstg1[4];
  UCHAR kwh1[2];
  UCHAR mwh1[2];

  /* Solar1: momentane Leistung, Waermemenge kWh/MWh */
  UCHAR mlstg2[4];
  UCHAR kwh2[2];
  UCHAR mwh2[2];
} DS_Winsol;

/* Datenstruktur des Datensatzes fuer Winsol / Solstat (Logdatei) UVR61-3   */
/* DSWinsol_UVR61_3 */
typedef struct {
  /* timestamp */
  UCHAR tag;
  UCHAR std;
  UCHAR min;
  UCHAR sek;

  /*  Ausgaenge durch bits: */
  /*   leer1 leer2 leer3 leer4 leer5 a3 a2 a1*/
  UCHAR ausgbyte1;

  /* drehzahlwert Ausgang A1 */
  UCHAR dza;

  /* Analog Ausgang */
  UCHAR ausg_analog;

  /* Sensor-Eingaenge */
  UCHAR sensT[6][2];

  /* nicht benutzt */
  UCHAR unbenutzt1[4];

  /* WMZ 1 aktiv in bit 1 */
  UCHAR wmzaehler_reg;

  /* Volumenstrom */
  UCHAR volstrom[2];

  /* Solar: momentane Leistung, Waermemenge kWh/MWh */
  UCHAR mlstg[2];
  UCHAR kwh[2];
  UCHAR mwh[4];

  /* nicht benutzt */
  UCHAR unbenutzt2[25];
} DS_Winsol_UVR61_3;

/* Datenktur des Datensatzes aus dem Datenlogger / Bootloader kommend */
/* the_datum_zeit */
typedef struct{
  UCHAR sek;
  UCHAR min;
  UCHAR std;
  UCHAR tag;
  UCHAR monat;
  UCHAR jahr;
} the_datum_zeit;

/* Union fuer Datensatz der UVR1611 und UVR61-3 */
/* aus dem Datenlogger / Bootloader kommend     */
typedef union{
  /* UVR1611 Datensatz DS_UVR1611 */
  struct {
  /* Sensor-Eingaenge */
#ifdef NEW_WSOL_TEMP
    UCHAR sensT[16][2];
#else
    UCHAR t2[2];
    UCHAR t3[2];
    UCHAR t4[2];
    UCHAR t5[2];
    UCHAR t6[2];
    UCHAR t7[2];
    UCHAR t8[2];
    UCHAR t9[2];
    UCHAR t10[2];
    UCHAR t11[2];
    UCHAR t12[2];
    UCHAR t13[2];
    UCHAR t14[2];
    UCHAR t15[2];
    UCHAR t16[2];
#endif
    /*  Ausgaenge durch bits: */
    /*   a8 a7 a6 a5 a4 a3 a2 a1*/
    UCHAR ausgbyte1;
    /*  Ausgaenge durch bits: */
    /*  leer1  leer2 leer3 a13 a12 a11 a10 a9 */
    UCHAR ausgbyte2;

    /* Drehzahlausgaenge */
    UCHAR dza[4];

    /* WMZ 1/2 aktiv in bit 1 und bit2 */
    UCHAR wmzaehler_reg;

    /* Solar1: momentane Leistung, Waermemenge kWh/MWh */
    UCHAR mlstg1[4];
    UCHAR kwh1[2];
    UCHAR mwh1[2];

    /* Solar2: momentane Leistung, Waermemenge kWh/MWh */
    UCHAR mlstg2[4];
    UCHAR kwh2[2];
    UCHAR mwh2[2];

    the_datum_zeit datum_zeit;

    UCHAR zeitstempel[3];
    UCHAR pruefsum;  /* Summer der Bytes mod 256 */
  } DS_UVR1611;
  /* UVR61-3 Datensatz DS_UVR61_3 */
  struct {
    /* Sensor-Eingaenge */
    UCHAR sensT[6][2];

    /*  Ausgaenge durch bits: */
    /*   leer1 leer2 leer3 leer4 leer5 a3 a2 a1*/
    UCHAR ausgbyte1;

    /* Drehzahlausgaenge */
    UCHAR dza;

    /* Analog Ausgang */
    UCHAR ausg_analog;

    /* WMZ 1 aktiv in bit 1 */
    UCHAR wmzaehler_reg;

    /* Volumenstrom */
    UCHAR volstrom[2];

    /* Solar: momentane Leistung, Waermemenge kWh/MWh */
    UCHAR mlstg1[2];
    UCHAR kwh1[2];
    UCHAR mwh1[4];

    the_datum_zeit datum_zeit;

    UCHAR zeitstempel[3];
    UCHAR pruefsum;  /* Summer der Bytes mod 256 */
  } DS_UVR61_3;
} u_DS_UVR1611_UVR61_3;

/* Bitfeld Drehzahlreglung*/
struct drehzahlreglung{
  unsigned aktiv  : 1;  /* 1. Bit zeigt Ausgang aktiv */
  unsigned    : 2; /*  2 freie Bits */
  unsigned wert  : 5;  /* 5 bit fuer den Drehzahlwert 0-30 */
};

/* Bitfeld HighByte Temperatur */
struct temperatur_byte_high{
  unsigned vorzeichen    : 1; /* 1 = Minus */
  unsigned einheitstyp1  : 1; /* 010 -> Temp., 011 -> Volumenstrom, 110 -> Strahlung, 111 -> Raumtemp.-sensor */
  unsigned einheitstyp2  : 1;
  unsigned einheitstyp3  : 1;
  unsigned wert_byte2    : 4;
};

/* Datenstruktur "aktuelle Daten" aus dem Datenlogger / Bootloader kommend */
/* AktuelleDaten_UVR1611                                                   */
typedef struct{
  UCHAR kennung; /* Wert: 128 (0x80) */
  UCHAR sensT[16][2];
  UCHAR ausgbyte1;
  UCHAR ausgbyte2;
  UCHAR dza[4];
  UCHAR wmzaehler_reg;
  UCHAR mlstg1[4];
  UCHAR kwh1[2];
  UCHAR mwh1[2];
  UCHAR mlstg2[4];
  UCHAR kwh2[2];
  UCHAR mwh2[2];
  UCHAR pruefsum;  /* Summer der Bytes mod 256 */
} AktuelleDaten_UVR1611;

/* Datenstruktur "aktuelle Daten" aus dem Datenlogger / Bootloader kommend */
/* AktuelleDaten_UVR61_3                                                   */
typedef struct{
  UCHAR kennung; /* Wert: 144 (0x90) */
  UCHAR sensT[6][2];
  UCHAR ausgbyte1;
  UCHAR dza;
  UCHAR ausg_analog;
  UCHAR wmzaehler_reg;
  UCHAR volstrom[2];
  UCHAR mlstg1[2];
  UCHAR kwh1[2];
  UCHAR mwh1[2];
  UCHAR pruefsum;  /* Summer der Bytes mod 256 */
} AktuelleDaten_UVR61_3;

/* Bytefeld BYTES_LONG zur Behandlung der Momentanleistung */
typedef union{
  long long_word;
  struct
  {
    char lowlowbyte;
    char lowhighbyte;
    char highlowbyte;
    char highhighbyte;
  } bytes;
} BYTES_LONG;

/* Datenstruktur des Kopfsatzes aus dem D-LOGG bzw. BL-Net kommend */
/* Modus 0xD1 - Laenge 14 Byte   - KopfsatzD1 -                    */
typedef struct{
  UCHAR kennung;
  UCHAR version;
  UCHAR zeitstempel[3];
  UCHAR satzlaengeGeraet1;
  UCHAR satzlaengeGeraet2;
  UCHAR startadresse[3];
  UCHAR endadresse[3];
  UCHAR pruefsum;  /* Summer der Bytes mod 256 */
} KopfsatzD1;

/* Datenstruktur des Kopfsatzes aus dem D-LOGG bzw. BL-Net kommend */
/* Modus 0xA8 - Laenge 13 Byte  - KopfsatzA8 -                     */
typedef struct {
  UCHAR kennung;
  UCHAR version;
  UCHAR zeitstempel[3];
  UCHAR satzlaengeGeraet1;
  UCHAR startadresse[3];
  UCHAR endadresse[3];
  UCHAR pruefsum;  /* Summer der Bytes mod 256 */
} KopfsatzA8;

/* Union fuer Daten im Modus 0xD1 (2DL)     */
/* aus dem Datenlogger / Bootloader kommend */
typedef union{
  /* alle Bytes */
  struct{
    UCHAR all_bytes[127]; /* alle Daten  */
  }DS_alles;
  /* DS_1611_1611 */
  struct{
    /* ******************   1. Geraet  ****************** */
    /* Sensor-Eingaenge */
#ifdef NEW_WSOL_TEMP
    UCHAR sensT[16][2];
/*#else
    UCHAR t2[2];
    UCHAR t3[2];
    UCHAR t4[2];
    UCHAR t5[2];
    UCHAR t6[2];
    UCHAR t7[2];
    UCHAR t8[2];
    UCHAR t9[2];
    UCHAR t10[2];
    UCHAR t11[2];
    UCHAR t12[2];
    UCHAR t13[2];
    UCHAR t14[2];
    UCHAR t15[2];
    UCHAR t16[2]; */
#endif
    /*  Ausgaenge durch bits: */
    /*   a8 a7 a6 a5 a4 a3 a2 a1*/
    UCHAR ausgbyte1;
    /*  Ausgaenge durch bits: */
    /*  leer1  leer2 leer3 a13 a12 a11 a10 a9 */
    UCHAR ausgbyte2;
    /* Drehzahlausgaenge */
    UCHAR dza[4];
    /* WMZ 1/2 aktiv in bit 1 und bit2 */
    UCHAR wmzaehler_reg;
    /* Solar1: momentane Leistung, Waermemenge kWh/MWh */
    UCHAR mlstg1[4];
    UCHAR kwh1[2];
    UCHAR mwh1[2];
    /* Solar2: momentane Leistung, Waermemenge kWh/MWh */
    UCHAR mlstg2[4];
    UCHAR kwh2[2];
    UCHAR mwh2[2];

    the_datum_zeit datum_zeit;

    UCHAR zeitstempel[3];
    /* ******************   2. Geraet  ****************** */
    /* Sensor-Eingaenge */
#ifdef NEW_WSOL_TEMP
    UCHAR Z_sensT[16][2];
/* #else
    UCHAR Z_t2[2];
    UCHAR Z_t3[2];
    UCHAR Z_t4[2];
    UCHAR Z_t5[2];
    UCHAR Z_t6[2];
    UCHAR Z_t7[2];
    UCHAR Z_t8[2];
    UCHAR Z_t9[2];
    UCHAR Z_t10[2];
    UCHAR Z_t11[2];
    UCHAR Z_t12[2];
    UCHAR Z_t13[2];
    UCHAR Z_t14[2];
    UCHAR Z_t15[2];
    UCHAR Z_t16[2]; */
#endif
    /*  Ausgaenge durch bits: */
    /*   a8 a7 a6 a5 a4 a3 a2 a1*/
    UCHAR Z_ausgbyte1;
    /*  Ausgaenge durch bits: */
    /*  leer1  leer2 leer3 a13 a12 a11 a10 a9 */
    UCHAR Z_ausgbyte2;
    /* Drehzahlausgaenge */
    UCHAR Z_dza[4];
    /* WMZ 1/2 aktiv in bit 1 und bit2 */
    UCHAR Z_wmzaehler_reg;
    /* Solar1: momentane Leistung, Waermemenge kWh/MWh */
    UCHAR Z_mlstg1[4];
    UCHAR Z_kwh1[2];
    UCHAR Z_mwh1[2];
    /* Solar2: momentane Leistung, Waermemenge kWh/MWh */
    UCHAR Z_mlstg2[4];
    UCHAR Z_kwh2[2];
    UCHAR Z_mwh2[2];

    the_datum_zeit Z_datum_zeit;

    UCHAR pruefsum;  /* Summer der Bytes mod 256 */
  }DS_1611_1611;
  /* DS_1611_61_3 */
  struct{
    /* ******************   1. Geraet  ****************** */
    /* Sensor-Eingaenge */
#ifdef NEW_WSOL_TEMP
    UCHAR sensT[16][2];
/* #else
    UCHAR t2[2];
    UCHAR t3[2];
    UCHAR t4[2];
    UCHAR t5[2];
    UCHAR t6[2];
    UCHAR t7[2];
    UCHAR t8[2];
    UCHAR t9[2];
    UCHAR t10[2];
    UCHAR t11[2];
    UCHAR t12[2];
    UCHAR t13[2];
    UCHAR t14[2];
    UCHAR t15[2];
    UCHAR t16[2]; */
#endif
    /*  Ausgaenge durch bits: */
    /*   a8 a7 a6 a5 a4 a3 a2 a1*/
    UCHAR ausgbyte1;
    /*  Ausgaenge durch bits: */
    /*  leer1  leer2 leer3 a13 a12 a11 a10 a9 */
    UCHAR ausgbyte2;
    /* Drehzahlausgaenge */
    UCHAR dza[4];
    /* WMZ 1/2 aktiv in bit 1 und bit2 */
    UCHAR wmzaehler_reg;
    /* Solar1: momentane Leistung, Waermemenge kWh/MWh */
    UCHAR mlstg1[4];
    UCHAR kwh1[2];
    UCHAR mwh1[2];
    /* Solar2: momentane Leistung, Waermemenge kWh/MWh */
    UCHAR mlstg2[4];
    UCHAR kwh2[2];
    UCHAR mwh2[2];

    the_datum_zeit datum_zeit;

    UCHAR zeitstempel[3];
    /* ******************   2. Geraet  ****************** */
    /* Sensor-Eingaenge */
    UCHAR Z_sensT[6][2];
    /*  Ausgaenge durch bits: */
    /*   leer1 leer2 leer3 leer4 leer5 a3 a2 a1*/
    UCHAR Z_ausgbyte1;
    /* Drehzahlausgaenge */
    UCHAR Z_dza;
    /* Analog Ausgang */
    UCHAR Z_ausg_analog;
    /* WMZ 1 aktiv in bit 1 */
    UCHAR Z_wmzaehler_reg;
    /* Volumenstrom */
    UCHAR Z_volstrom[2];
    /* Solar: momentane Leistung, Waermemenge kWh/MWh */
    UCHAR Z_mlstg1[2];
    UCHAR Z_kwh1[2];
    UCHAR Z_mwh1[4];
    the_datum_zeit Z_datum_zeit;

    UCHAR pruefsum;  /* Summer der Bytes mod 256 */
  }DS_1611_61_3;
  /* DS_61_3_1611 */
  struct{
    /* ******************   1. Geraet  ****************** */
    /* Sensor-Eingaenge */
    UCHAR sensT[6][2];
    /*  Ausgaenge durch bits: */
    /*   leer1 leer2 leer3 leer4 leer5 a3 a2 a1*/
    UCHAR ausgbyte1;
    /* Drehzahlausgaenge */
    UCHAR dza;
    /* Analog Ausgang */
    UCHAR ausg_analog;
    /* WMZ 1 aktiv in bit 1 */
    UCHAR wmzaehler_reg;
    /* Volumenstrom */
    UCHAR volstrom[2];
    /* Solar: momentane Leistung, Waermemenge kWh/MWh */
    UCHAR mlstg1[2];
    UCHAR kwh1[2];
    UCHAR mwh1[4];

    the_datum_zeit datum_zeit;

    UCHAR zeitstempel[3];
    /* ******************   2. Geraet  ****************** */
  /* Sensor-Eingaenge */
#ifdef NEW_WSOL_TEMP
    UCHAR Z_sensT[16][2];
#else
    UCHAR Z_t2[2];
    UCHAR Z_t3[2];
    UCHAR Z_t4[2];
    UCHAR Z_t5[2];
    UCHAR Z_t6[2];
    UCHAR Z_t7[2];
    UCHAR Z_t8[2];
    UCHAR Z_t9[2];
    UCHAR Z_t10[2];
    UCHAR Z_t11[2];
    UCHAR Z_t12[2];
    UCHAR Z_t13[2];
    UCHAR Z_t14[2];
    UCHAR Z_t15[2];
    UCHAR Z_t16[2];
#endif
    /*  Ausgaenge durch bits: */
    /*   a8 a7 a6 a5 a4 a3 a2 a1*/
    UCHAR Z_ausgbyte1;
    /*  Ausgaenge durch bits: */
    /*  leer1  leer2 leer3 a13 a12 a11 a10 a9 */
    UCHAR Z_ausgbyte2;
    /* Drehzahlausgaenge */
    UCHAR Z_dza[4];
    /* WMZ 1/2 aktiv in bit 1 und bit2 */
    UCHAR Z_wmzaehler_reg;
    /* Solar1: momentane Leistung, Waermemenge kWh/MWh */
    UCHAR Z_mlstg1[4];
    UCHAR Z_kwh1[2];
    UCHAR Z_mwh1[2];
    /* Solar2: momentane Leistung, Waermemenge kWh/MWh */
    UCHAR Z_mlstg2[4];
    UCHAR Z_kwh2[2];
    UCHAR Z_mwh2[2];

    the_datum_zeit Z_datum_zeit;

    UCHAR pruefsum;  /* Summer der Bytes mod 256 */
  }DS_61_3_1611;
  /* DS_61_3_61_3 */
  struct{
    /* ******************   1. Geraet  ****************** */
    /* Sensor-Eingaenge */
    UCHAR sensT[6][2];
    /*  Ausgaenge durch bits: */
    /*   leer1 leer2 leer3 leer4 leer5 a3 a2 a1*/
    UCHAR ausgbyte1;
    /* Drehzahlausgaenge */
    UCHAR dza;
    /* Analog Ausgang */
    UCHAR ausg_analog;
    /* WMZ 1 aktiv in bit 1 */
    UCHAR wmzaehler_reg;
    /* Volumenstrom */
    UCHAR volstrom[2];
    /* Solar: momentane Leistung, Waermemenge kWh/MWh */
    UCHAR mlstg1[2];
    UCHAR kwh1[2];
    UCHAR mwh1[4];

    the_datum_zeit datum_zeit;

    UCHAR zeitstempel[3];
    /* ******************   2. Geraet  ****************** */
    /* Sensor-Eingaenge */
    UCHAR Z_sensT[6][2];
    /*  Ausgaenge durch bits: */
    /*   leer1 leer2 leer3 leer4 leer5 a3 a2 a1*/
    UCHAR Z_ausgbyte1;
    /* Drehzahlausgaenge */
    UCHAR Z_dza;
    /* Analog Ausgang */
    UCHAR Z_ausg_analog;
    /* WMZ 1 aktiv in bit 1 */
    UCHAR Z_wmzaehler_reg;
    /* Volumenstrom */
    UCHAR Z_volstrom[2];
    /* Solar: momentane Leistung, Waermemenge kWh/MWh */
    UCHAR Z_mlstg1[2];
    UCHAR Z_kwh1[2];
    UCHAR Z_mwh1[4];
    the_datum_zeit Z_datum_zeit;

    UCHAR pruefsum;  /* Summer der Bytes mod 256 */
  }DS_61_3_61_3;
} u_modus_D1;

#endif
