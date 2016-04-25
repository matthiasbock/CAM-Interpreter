
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define MODE_INTERPOLATION_QUICK   0
#define MODE_INTERPOLATION_LINEAR  1
#define MODE_COORDINATE_ABSOLUTE   2
#define MODE_COORDINATE_RELATIVE   3

typedef struct
{
    double x;
    double y;
} point_t;

int mode_interpolation = MODE_INTERPOLATION_QUICK;
int mode_coordinate    = MODE_COORDINATE_ABSOLUTE;
point_t current_position = {x:0.0, y:0.0};
int speed = 100;

void interpolate(point_t *start, point_t *stop)
{
    // stub
    printf("Go!\n");
}

void interpret_block(char block[], point_t* target, bool* move)
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
        interpret_block(p, &target, &move);
        p = strtok(NULL, " ");
    }

    if (move)
    {
        // Interpolator aufrufen
        interpolate(&current_position, &target);
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
