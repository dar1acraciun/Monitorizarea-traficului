Descriere

Acest proiect monitorizează traficul rutier din Iași, colectând date despre coordonate GPS și viteza vehiculelor în timp real. Scopul este de a analiza fluxul de trafic în zonele principale și de a oferi informații utile pentru optimizarea circulației.
Acesta este construit pe arhitectura client/server folosind threaduri pentru contectarea fiecarui client. In client se folosesc threaduri separate pentru viteza,coordonate si mesaje.
Caracteristici

Colectare date: Vehiculele trimit periodic coordonatele și viteza către server.

Zone monitorizate: Copou, Centru, Palas, Sărărie, Independenței și zonele dintre acestea.

Interogare bază de date: Posibilitatea de a analiza datele istorice și actuale ale traficului.

Afișare statistici: Generare de rapoarte despre densitatea traficului și viteza medie.

Cerințe de sistem

Limbaj: C

Sistem de operare: Linux

Dependențe: cURL pentru obținerea condițiilor meteo

Bază de date: SQLite/MySQL
