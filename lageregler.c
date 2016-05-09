/**
 * Lageregler über Pointer ansprechen
 * TSR-Programm (Terminate and Stay Ready)
 */

int es_tsr;
union REGS regs;        /* Register */
struct SREGS sregs;     /* Segmentadressen */

char far    *TE_GLT;
int  far    *TESOWE;
long far    *ZSOLL;

#define ADR_TE_GLT  0x164 /* Teilsollwert gütig */
#define ADR_TESOWE  0x13A /* Teilsollwert */
#define ADR_ZSOLL   0x140 /* Sollwert */

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


#define pulses_per_10_um 400

// Übergabe der Teilsollwerte an den Lageregler:

while (*TE_GLT == 1);       /* warten bis neuer Wert an die
                               Lagereglung uebergeben werden kann */
*TESOWE = dx * Pulse;       /* Pulse fuer X-Achse */
*(TESOWE + 1) = dy * Pulse; /* Pulse fuer y-Achse */
*TE_GLT = 1;                /* Uebergabe ist gueltig */
