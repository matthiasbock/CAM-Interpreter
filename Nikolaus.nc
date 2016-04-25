%123

% G00: Eilgang
% G90: Absolutmasseingabe
N010 G00 G90 X3000 Y3000

% G91: Relativmasseingabe
% horizontal nach rechts
N020 G01 G91 X6000

% vertikal nach oben
N030 Y6000

% rechte Dachseite
N040 X-3000 Y4000

% linke Dachseite
N050 X-3000 Y-4000

% zum Anfangspunkt runter
N060 Y-6000

% diagonal nach rechts oben
N070 X6000 Y6000

% horizontal nach links
N080 X-6000

% diagonal nach rechts unten
N090 X6000 Y6000

% Ende
N100 M30