/**
 * CAM Steuersoftware
 *
 * Liest Steuerkommandos aus einer NC-Datei
 * und verfaehrt die Wambach-Maschine entsprechend
 *
 * Hinweise:
 *  - Vor dem Start muss nctsr durch einfachen Aufruf geladen werden.
 *  - Dieses Programm laeuft nur unter MS-DOS.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dos.h>
#include <math.h>
#include <time.h>

#define bool int
#define true 1
#define false 0

/**
 * Lageregler über Pointer ansprechen
 * TSR-Programm (Terminate and Stay Ready)
 */

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

#define pulses_per_mm 40000.0

#define v_Bahn  2.0    // Maximalgeschwindigkeit in mm/s
#define a       10.0   // Beschleunigung in mm/s^2
#define T       0.004  // Interpolationstakt in s, also 4ms

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

/**
 * Uebergibt die Teilsollwerte an den Lageregler
 */
void set_increment_vector(point_t* P)
{
//    printf("\n");
    while (*TE_GLT == 1)
    {
//        printf(".");
    }                           /* warten bis neuer Wert an die
                                   Lagereglung uebergeben werden kann */
//    printf(";\n");

    /* Pulse fuer X-Achse */
    *TESOWE = (int) (P->x * pulses_per_mm);
    
    /* Pulse fuer y-Achse */
    *(TESOWE + 1) = (int) (P->y * pulses_per_mm);
    
    /* Uebergabe ist gueltig */
    *TE_GLT = 1;
}

/**
 * Laderegelung initialisieren
 */
void init_tsr()
{
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

/**
 * Interpoliert zwischen zwei gegebenen Punkten
 * und verfaehrt die Achsen entsprechend
 * Das Geschwindigkeitsprofil ist eine Rampe:
 *  - zu Beginn beschleunigt der Roboter
 *  - in der Mitte verfaehrt der Roboter mit konstanter Bahngeschwindigkeit
 *  - gegen Ende bremst der Roboter
 */
void interpolate_ramp(point_t *P0, point_t *P1)
{
     float S_Gesamt,S_k, S_Rest,S_Brems, v_k, tau, dtau;
     point_t  p_k;
     point_t* P_k;
     point_t  Verfahrvektor;
     float old_x, old_y;
     int printf_counter = 0;
     int i;
    
    // Laenge der insgesamt zu verfahrenden Strecke
    printf("Verfahre von (%02f|%02f) nach (%02f|%02f)...\n", P0->x, P0->y, P1->x, P1->y);

    S_Gesamt = distance(P0, P1);
    printf("Laenge der insgesamt zu verfahrenden Strecke: %03f\n", S_Gesamt);

    // wie weit man schon auf der Gesamtstrecke verfahren ist
    S_k    = 0.0;
    P_k    = &p_k;
    P_k->x = P0->x;
    P_k->y = P0->y;

    S_Rest  = S_Gesamt;

    // Laenge der Rest-Strecke, ab der gebremst werden muss,
    // um am Ende mit Geschwindigkeit Null anzukommen
	S_Brems = pow(v_Bahn,2) / (2*a);
	if(S_Gesamt != 0)
	{
		printf("Verfahre mit Rampe; Beschleunigungs-/Bremsweg: %.6f (%.1f\045)\n", S_Brems, S_Brems/S_Gesamt);

	}
    // Startgeschwindigkeit: Null
    v_k = 0.0;

    // Prozentsatz, wie weit man schon auf der Gesamtstrecke verfahren ist
    tau = 0.0;

    Verfahrvektor.x = 0;
    Verfahrvektor.y = 0;
	printf("Fortschritt: %.2f (+%.2f); Verfahrvektor: (%.6f|%.6f); v_k = %.2f;\n", tau*100, dtau*100, Verfahrvektor.x, Verfahrvektor.y, v_k);

    // solange wir nicht am Ziel angekommen sind:
    while (S_Rest > 0.0)
    {
        // nur jedes zehnte mal ausgeben
        printf_counter = (printf_counter+1) % 10;

        S_k     = distance(P_k, P0);
        S_Rest  = S_Gesamt - S_k;

        if (S_Rest <= 0.0)
        {
            // Endpunkt erreicht
            return;
        }

        if (v_k < v_Bahn && S_Rest > S_Brems)
        {
            // beschleunigen
            v_k = v_k + a*T;
            // maximale Geschwindigkeit ueberschritten
            if (v_k > v_Bahn)
            {
                v_k = v_Bahn;
            }
            if (!printf_counter)
                printf("Bahngeschwindigkeit unterschritten => beschleunigen\n");
        }
        else if (S_Rest <= S_Brems)
        {
            // bremsen
            v_k = v_k - a*T;
            // Mindest-Geschwindigkeit unterschritten
            if (v_k < 0.0)
            {
				v_k = 0.0;
				tau = 1.0;
			}
			if (!printf_counter)
				printf("Bremsweg unterschritten => bremsen\n");
		}
		else
		{
			v_k = v_Bahn;
			if (!printf_counter)
				printf("konstante Bahngeschwindigkeit\n");
		}

        dtau   = v_k*T/S_Gesamt;
        tau   += dtau;

        /*
         * der naechste Zwischenpunkt wird berechnet
         */
         
        // alten Punkt merken
        old_x = P_k->x;
        old_y = P_k->y;

        if (tau >= 1.0 || S_Rest <= 0.0)
        {
            tau = 1.0;
            S_Rest = 0.0;

            // weiter gehts nicht
            P_k->x = P1->x;
            P_k->y = P1->y;
        }
        else
        {
            P_k->x = P0->x + tau*(P1->x - P0->x);
            P_k->y = P0->y + tau*(P1->y - P0->y);
        }
        
        Verfahrvektor.x = P_k->x - old_x;
        Verfahrvektor.y = P_k->y - old_y;

        set_increment_vector(&Verfahrvektor);

        if (!printf_counter)
			printf("Fortschritt: %.2f (+%.2f); Verfahrvektor: (%.6f|%.6f); v_k = %.2f;\n", tau*100, dtau*100, Verfahrvektor.x, Verfahrvektor.y, v_k);
	}

	printf("Fortschritt: %.2f (+%.2f); Verfahrvektor: (%.6f|%.6f); v_k = %.2f;\n", tau*100, dtau*100, Verfahrvektor.x, Verfahrvektor.y, v_k);
	printf("Endpunkt erreicht.\n");
}

/**
 * Interpoliert zwischen zwei gegebenen Punkten
 * durch Aufruf der mit mode gewaehlten Interpolations-Methode
 */
void interpolate(point_t *P0, point_t *P1, int mode)
{
//    if (mode == MODE_INTERPOLATION_RAMP)
          interpolate_ramp(P0, P1);

//    else
//        printf("Interpolations-Modus wird nicht unterstuetzt.\n");
}

/**
 * Interpretiert genau ein Kommando aus der eingelesenen NC-Datei
 * und fuehrt bedarfsfalls den Interpolator aus, um den Roboter zu verfahren
 */
void interpret_command(char input_str[], point_t* target, bool* move)
{

    point_t P;
    char block[30];
    block[0] = 0;

//    printf("input_str = %s\n", input_str);
//    printf("block = %s\n", block);
    strcpy(block, input_str);

    if (block[4] == 10 || block[4] == 13)
    {
        block[4] = 0;
    }

//    block[0] = input_str[0];
//    block[1] = input_str[1];
//    block[2] = input_str[2];
//    block[3] = 0;
//    printf("input_str = %s\n", input_str);
//    printf("block = %s\n", block);

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
        P.x = 0;
        P.y = 0;
		set_increment_vector(&P);
        printf("Programmende\n");
    }

    else
    {
        printf("Kommando nicht erkannt: %s\n", block);
    }
}

/**
 * Interpretiert eine Zeile aus der NC-Datei,
 * indem die Zeile in Teil-Kommandos zerlegt und
 * jedes Kommando einzeln an interpret_command uebergeben wird
 */
void interpret_line(char line[])
{
    point_t target;
    bool move;
    char* p;
    int i;

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
     else
     {
         printf("Stopp.\n");
     }
}

/**
 * Liest genau eine Zeile aus einem Datenstrom, z.B einer Datei
 */
size_t getline(char **lineptr, size_t *n, FILE *stream)
{
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

    // initialisiere das Interface zum Lageregler
    init_tsr();

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
