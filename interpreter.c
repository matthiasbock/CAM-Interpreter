
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

void interpolate(point_t *start, point_t *stop)
{
    // stub
}

void interpret_block(char block[], point_t* target, bool* move)
{
    printf("%s\n", block);
}

void interpret_line(char line[])
{
    // leere Zeile
    if (strlen(line) < 2 || line[0] == ' ' || line[0] == '\t')
        return;

    // Kommentarzeile
    if (line[0] == '%')
        return;

    // Syntax pruefen: Muss mit 'N' anfangen
    if (line[0] != 'N')
    {
        printf("Syntaxfehler: 'N' am Zeilenanfang erwartet.\n");
        printf("%s\n", line);
        return;
    }

    // dahin wollen wir in dieser Zeile verfahren:
    point_t *target;
    // verfahren wir in dieser Zeile ueberhaupt?
    bool move = false;

    // Zeile auftrennen mit Leertaste als Zwischenraum
    char *p = strtok(line, " ");

    // jeden Teilblock verarbeiten
    while (p != NULL)
    {
        interpret_block(p, target, &move);
        p = strtok(NULL, " ");
    }

    if (move)
    {
        // Interpolator aufrufen
        interpolate(&current_position, target);
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
        //printf("%s", line);
        interpret_line(line);
    }

    // Datei schliessen
    fclose(fp);
    if (line)
        free(line);

    return 0;
}
