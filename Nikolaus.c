/**
 * Lageregler über Pointer ansprechen
 * TSR-Programm (Terminate and Stay Ready)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
//#include <dos.h>
#include <math.h>

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

#define v_Bahn  2.0   // Maximalgeschwindigkeit in mm/s
#define a      10.0   // Beschleunigung in mm/s^2
#define T       0.004 // Interpolationstakt in ms

int debug;

#define MODE_INTERPOLATION_QUICK   0
#define MODE_INTERPOLATION_LINEAR  1
#define MODE_COORDINATE_ABSOLUTE   2
#define MODE_COORDINATE_RELATIVE   3

int mode_interpolation = MODE_INTERPOLATION_QUICK;
int mode_coordinate    = MODE_COORDINATE_ABSOLUTE;
point_t current_position;
current_position = {x:0.0f, y:0.0f};
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
    float S_Gesamt = distance(P0, P1);

    // wie weit man schon auf der Gesamtstrecke verfahren ist
    point_t  p_k  = {x:P0->x, y:P0->y};
    point_t* P_k  = &p_k;
    float S_k     = 0.0;

    float S_Rest  = S_Gesamt;
    float S_Brems;

    float v_k     = 0.0;

    // Prozentsatz, wie weit man schon auf der Gesamtstrecke verfahren ist
    float tau;
    float dtau;

    while (S_Rest > 0.0)
    {
        S_k     = distance(P_k, P0);
        S_Rest  = S_Gesamt - S_k;
        S_Brems = pow(v_k,2) / (2*a);

        if (S_Rest <= 0.0)
            // Endpunkt erreicht
            return;

        if (S_Brems < S_Rest && v_k < v_Bahn)
            // beschleunigen
            v_k = v_k + a*T;
        else
            // bremsen
            v_k = v_k - a*T;

        dtau   = v_k*T/S_Gesamt;
        tau   += dtau;

        // der naechste Zwischenpunkt wird berechnet:
        float old_x = P_k->x;
        float old_y = P_k->y;

        if (tau > 1.0 || S_Rest <= 0.0)
        {
            tau = 1.0;
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
        //printf("moved\t\t\t\t\t\t\t (%.2f,%.2f), v_k = %.2f\n", P_k->x-old_x, P_k->y-old_y, v_k);
    }
}

void interpolate(point_t *P0, point_t *P1, int mode)
{
    // if mode == RAMP
    interpolate_ramp(P0, P1);
}

/*
 * NC-Interpreter von
 * Matthias Bock und Nicole Loewel
 *
 * 25. April 2016
 *
 * Lizenz: GNU GPLv3
 */

void interpret_command(char block[], point_t* target, bool* move)
{
    if (block[0] == 'N')
        return;

    else if (block[0] == 'G')
    {
        if (strlen(block) < 3)
        {
            printf("Syntaxfehler: G-Kommando muss aus genau 3 Zeichen bestehen\n");
            return;
        }
        block[4] = 0;

        if (strcmp(block, "G00"))
        {
            mode_interpolation = MODE_INTERPOLATION_QUICK;
            printf("Eilgang: OK\n");
        }
        else if (strcmp(block, "G01"))
        {
            mode_interpolation = MODE_INTERPOLATION_LINEAR;
            printf("Linearinterpolation: OK\n");
        }
        else if (strcmp(block, "G90"))
        {
            mode_coordinate = MODE_COORDINATE_ABSOLUTE;
            printf("Koordinaten: absolut\n");
        }
        else if (strcmp(block, "G91"))
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
        block[4] = 0;
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
    point_t target = {x: 0.0, y:0.0};
    // verfahren wir in dieser Zeile ueberhaupt?
    bool move = false;

    // Zeile auftrennen mit Leertaste als Zwischenraum
    char *p = strtok(line, " ");

    // jeden Teilblock verarbeiten
    while (p != NULL)
    {
        interpret_command(p, &target, &move);
        p = strtok(NULL, " ");
    }

    if (move)
    {
        // Interpolator aufrufen
        interpolate(&current_position, &target, mode_interpolation);
    }
}

int main()
{
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

    // Datei oeffnen
    fp = fopen("Nikolaus.nc", "r");
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