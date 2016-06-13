/**
 * Lageregler über Pointer ansprechen
 * TSR-Programm (Terminate and Stay Ready)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dos.h>
#include <math.h>

#define bool int
#define true 1
#define false 0

int    es_tsr;
union  REGS regs;        /* Register */
struct SREGS sregs;     /* Segmentadressen */

char far *TE_GLT;
int  far *TESOWE;
long far *ZSOLL;

#define ADR_TE_GLT  0x164 /* Teilsollwert gütig */
#define ADR_TESOWE  0x13A /* Teilsollwert */
#define ADR_ZSOLL   0x140 /* Sollwert */

typedef struct
{
    double x;
    double y;
} point_t;

#define pulses_per_10_um 400

#define v_Bahn  10.0  // Maximalgeschwindigkeit in mm/s
#define a       20.0   // Beschleunigung in mm/s^2
#define T       0.004 // Interpolationstakt in ms

int debug;

#define MODE_COORDINATE_ABSOLUTE   2
#define MODE_COORDINATE_RELATIVE   3
#define MODE_INTERPOLATION_QUICK   0
#define MODE_INTERPOLATION_LINEAR  1
#define MODE_INTERPOLATION_RAMP    2

int mode_interpolation = MODE_INTERPOLATION_RAMP;
int mode_coordinate    = MODE_COORDINATE_ABSOLUTE;
point_t current_position = {0.0, 0.0};
int speed = 100;

// Übergabe der Teilsollwerte an den Lageregler:
void set_position(point_t* P)
{
    while (*TE_GLT == 1);       /* warten bis neuer Wert an die
                                   Lagereglung uebergeben werden kann */
    *TESOWE = P->x * pulses_per_10_um;       /* Pulse fuer X-Achse */
    *(TESOWE + 1) = P->y * pulses_per_10_um; /* Pulse fuer y-Achse */
    *TE_GLT = 1;                /* Uebergabe ist gueltig */
}



/**
 * Dieses Programm zerlegt eine gegebene Strecke
 * zwischen einem Anfangspunkt P0 und einem Endpunkt P1
 * in Teilpunkte, zwischen denen jeweils dieselbe Zeit vergeht
 */

float distance(point_t *P0, point_t *P1)
{
    return sqrt(pow((P1->x - P0->x),2) + pow((P1->y - P0->y),2));
}

void interpolate_ramp(point_t *P0, point_t *P1)
{
	 float S_Gesamt,S_k, S_Rest,S_Brems, v_k, tau, dtau;
	 point_t  p_k;
	 point_t* P_k;
	 float old_x, old_y;

    /*
    ------------------------------------------------------------------
    Segment des TSR-Programms bestimmen
    ------------------------------------------------------------------
    */
    regs.h.ah = 0x35;                       /* Dos Interrupt 21H mit Funktion 35H aufrufen */
    regs.h.al = 0x1c;                       /* Codesegment von Interrupt 1CH bestimmen */
    int86x(0x21 , &regs , &regs , &sregs);  /* Beginn des CS holen */
    es_tsr = sregs.es;                      /* Zeigt auf CS des Lagereglers */
    
    /*
    ------------------------------------------------------------------
    Pointer für das TSR-Programm adressieren
    ------------------------------------------------------------------
    */
    TE_GLT  = (char far*) MK_FP(es_tsr,ADR_TE_GLT);
    TESOWE  = (int far*)  MK_FP(es_tsr,ADR_TESOWE);
    ZSOLL   = (long far*) MK_FP(es_tsr,ADR_ZSOLL);    
    
    // Laenge der insgesamt zu verfahrenden Strecke
	 S_Gesamt = distance(P0, P1);

    // wie weit man schon auf der Gesamtstrecke verfahren ist
	 p_k.x  = P0->x;
	 p_k.y  = P0->y;
	 P_k  = &p_k;
	 S_k     = 0.0;

    S_Rest  = S_Gesamt;

	v_k     = 0.0;
	tau = 0.0;

    // Prozentsatz, wie weit man schon auf der Gesamtstrecke verfahren ist

    while (S_Rest > 0.0)
    {
        S_k     = distance(P_k, P0);
        S_Rest  = S_Gesamt - S_k;
        S_Brems = pow(v_k,2) / (2*a);

        if (S_Rest <= 0.0)
            // Endpunkt erreicht
            return;

		if (S_Brems < S_Rest && v_k < v_Bahn)
		{
			// beschleunigen
			v_k = v_k + a*T;
			printf("beschleunigen\n");
		}
		else
		{
			// bremsen
			v_k = v_k - a*T;
			printf("bremsen\n");
		}

        dtau   = v_k*T/S_Gesamt;
        tau   += dtau;

        // der naechste Zwischenpunkt wird berechnet:
        old_x = P_k->x;
        old_y = P_k->y;

		if (tau >= 1.0 || S_Rest <= 0.0)
		{
			tau = 1.0;
			S_Rest = 0.0;

			P_k->x = P1->x;
            P_k->y = P1->y;
        }
        else
        {
            //P_k->x += (P1->x - P0->x) * dtau;
            //P_k->y += (P1->y - P0->y) * dtau;
            P_k->x = P0->x + tau*(P1->x - P0->x);
            P_k->y = P0->y + tau*(P1->y - P0->y);
        }

        set_position(P_k);
		  printf("tau=%.2f, delta: x=%.2f, y=%.2f, v_k = %.2f\n", tau, P_k->x-old_x, P_k->y-old_y, v_k);
	  }
}

void interpolate(point_t *P0, point_t *P1, int mode)
{
//    if (mode == MODE_INTERPOLATION_RAMP)
		  interpolate_ramp(P0, P1);

//	else
//        printf("Interpolations-Modus wird nicht unterstuetzt.\n");
}

/*
 * NC-Interpreter von
 * Matthias Bock und Nicole Loewel
 *
 * 25. April 2016
 *
 * Lizenz: GNU GPLv3
 */

void interpret_command(char input_str[], point_t* target, bool* move)
{

	char block[30];
	block[0] = 0;

//	printf("input_str = %s\n", input_str);
//	printf("block = %s\n", block);
	strcpy(block, input_str);

	if (block[4] == 10 || block[4] == 13)
	{
		block[4] = 0;
	}

//	block[0] = input_str[0];
//	block[1] = input_str[1];
//	block[2] = input_str[2];
//	block[3] = 0;
//	printf("input_str = %s\n", input_str);
//	printf("block = %s\n", block);

	if (block[0] == 'N')
	{
		printf("Das ist ein Kommentar.\n");
		  return;
	}

    else if (block[0] == 'G')
    {
        if (strlen(block) < 3)
        {
            printf("Syntaxfehler: G-Kommando muss aus genau 3 Zeichen bestehen\n");
            return;
        }
        

        if (strcmp(block, "G00") != 0)
        {
            mode_interpolation = MODE_INTERPOLATION_QUICK;
            printf("Eilgang: OK\n");
        }
        else if (strcmp(block, "G01") != 0)
        {
            mode_interpolation = MODE_INTERPOLATION_LINEAR;
            printf("Linearinterpolation: OK\n");
        }
        else if (strcmp(block, "G90") != 0)
        {
            mode_coordinate = MODE_COORDINATE_ABSOLUTE;
            printf("Koordinaten: absolut\n");
        }
        else if (strcmp(block, "G91") != 0)
        {
            mode_coordinate = MODE_COORDINATE_RELATIVE;
            printf("Koordinaten: relativ\n");
        }
        else
        {
            printf("G-Kommando nicht erkannt: %s\n", block);
        }
	 }

    else if (block[0] == 'X')
    {
        target->x = atoi(block+1);
        *move = true;
        printf("Verfahre nach X=%f\n", target->x);
    }

    else if (block[0] == 'Y')
    {
        target->y = atoi(block+1);
        *move = true;
        printf("Verfahre nach Y=%f\n", target->y);
    }

    else if (block[0] == 'F')
    {
        speed = atoi(block+1);
        printf("Verfahre mit Geschwindigkeit F=%i\n", speed);
    }

    else if (block[0] == 'M')
    {
        block[4] = '\0';
//        if (!strcmp(block, "M30"))
//            printf("Ich nehme an, sie meinten \"M30\": %s\n", block);
        printf("Programmende\n");
    }

    else
    {
        printf("Kommando nicht erkannt: %s\n", block);
    }
}

void interpret_line(char line[])
{
	 point_t target;
    bool move;
	 char* p;
    // leere Zeile
    if (strlen(line) < 3 || line[0] == ' ' || line[0] == '\t')
        return;

    // Kommentarzeile
    if (line[0] == '%')
        return;

    // Syntax pruefen: Muss mit 'N' anfangen
    if (line[0] != 'N')
    {
        printf("Syntaxfehler: 'N' am Zeilenanfang erwartet: %s\n", line);
        return;       
    }

    // dahin wollen wir in dieser Zeile verfahren:
	 target.x = 0.0;
	 target.y = 0.0;

    // sollen wir nach dieser Zeile verfahren?
    move = false;

    // Zeile auftrennen mit Leertaste als Zwischenraum/Trennzeichen
	 p = strtok(line, " ");

    // jeden Teilblock verarbeiten
    while (p != NULL)
	 {
	   printf("Interpretiere Teil-String %s...\n", (char*)p);
	   interpret_command(p, &target, &move);
	   p = strtok(NULL, " ");
    }

    if (move)
    {
        // Interpolator aufrufen
        interpolate(&current_position, &target, mode_interpolation);
	 }
	 else printf("haha move ist false, passiert ja gar nichts hier\n");
}

size_t getline(char **lineptr, size_t *n, FILE *stream) {
    char *bufptr = NULL;
    char *p = bufptr;
    size_t size;
    int c;

    if (lineptr == NULL) {
    	return -1;
    }
    if (stream == NULL) {
    	return -1;
    }
    if (n == NULL) {
    	return -1;
    }
    bufptr = *lineptr;
    size = *n;

    c = fgetc(stream);
    if (c == EOF) {
    	return -1;
    }
    if (bufptr == NULL) {
    	bufptr = malloc(128);
    	if (bufptr == NULL) {
    		return -1;
    	}
    	size = 128;
    }
    p = bufptr;
    while(c != EOF) {
    	if ((p - bufptr) > (size - 1)) {
    		size = size + 128;
    		bufptr = realloc(bufptr, size);
    		if (bufptr == NULL) {
    			return -1;
    		}
    	}
    	*p++ = c;
    	if (c == '\n') {
    		break;
    	}
    	c = fgetc(stream);
    }

    *p++ = '\0';
    *lineptr = bufptr;
    *n = size;

    return p - bufptr - 1;
}

int main()
{
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    size_t read;

    // Datei oeffnen
	fp = fopen("Nik.nc", "r");
    if (fp == NULL)
    {
        printf("Fatal: Datei konnte nicht geoeffnet werden\n");
        return -1;
    }

    // Datei Zeile fuer Zeile verarbeiten
    while ((read = getline(&line, &len, fp)) != -1)
    {
//        while (strlen(line) >0 && (line[strlen(line)-1] == 0x0D || line[strlen(line)-1]))
//            line[strlen(line)-1] = 0;
        //printf("%s", line);
        interpret_line(line);
    }

    // Datei schliessen
    fclose(fp);
    if (line)
        free(line);

    return 0;
}