
/**
 * Dieses Programm zerlegt eine gegebene Strecke
 * zwischen einem Anfangspunkt P0 und einem Endpunkt P1
 * in Teilpunkte, zwischen denen jeweils dieselbe Zeit vergeht
 */

#include "interpolator.h"

#define v_Bahn  20.0  // Maximalgeschwindigkeit in mm/s
#define a      10.0   // Beschleunigung in mm/s^2
#define T       0.004 // Interpolationstakt in ms

int debug;

// method stub
void set_position(point_t *P)
{
//    printf("moveto (%.2f,%.2f)\n", P->x, P->y);
}

float distance(point_t *P0, point_t *P1)
{
    return sqrt(pow((P1->x - P0->x),2) + pow((P1->y - P0->y),2));
}

void interpolate_ramp(point_t *P0, point_t *P1)
{
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
