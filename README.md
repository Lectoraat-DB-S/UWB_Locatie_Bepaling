# README

## Beschrijving
Dit project is een realisatie van een locatiebepalingsysteem op basis van UWB. Door de afstand te bepalen tussen een tag (de <ins>bewegende</ins> actor) en meerdere anchors (de <ins>statische</ins> actor), kan aan de hand van **trilateratie** de locatie van een tag bepaald worden.

### Hardware
Voor deze realisatie is gebruik gemaakt van de Makerfabs ESP32 UWB DW3000 (https://www.makerfabs.com/esp32-uwb-dw3000.html). Dit is een ontwikkelbord waarbij een UWB module en een ESP32 geïntergreerd zijn.

### Software
Er zijn verschillende softwareonderelen gemaakt voor dit project. De <ins>graphic user interface</ins> is geprogrammeerd in **C#**. Als dit programma uitgevoerd wordt opent er een window. Als er op dat moment tags en anchors bezig zijn met het verzenden van data dan wordt dit visueel weergegeven en gelogd. Deze log kan geëxporteerd worden naar een .csv bestand.

Vervolgens zijn er twee aparte programma's. Eén voor de <ins>tag</ins> en een ander voor de <ins>anchor</ins>. Deze programma's zijn geschreven in **C/C++**. Tijdens de ontwikkeling van het project is er getest in Visual Studio Code met de PlatformIO plugin. De code werkt ook in de Arduino IDE. Deze programmas bestaan uit communicatie over UWB en specifiek voor de tag zit er nog een onderdeel in wat zorgt voor UDP communicatie met de GUI.


## Imports en versies
Voor dit project zijn een heel aantal extra softwareonderdelen geïnstalleerd. 
Dit is getest op:  
Microsoft Windows 11 Pro Versie 10.0.22631 Build 22631  
Microsoft Windows 11 Home Versie 10.0.22631 Build 22631

### Imports
We hebben bij dit project gebruik gemaakt van twee libraries:  
- Arduino library. Deze zit ingebakken in Arduino IDE of PlatformIO met een simpele #include statement.  
- DW3000 library. Deze library is voor het bedienen van de DW3000 chip. De library wordt onderhouden door Makerfabs. Deze library is op dit moment (mei 2024) voor het laatst geüpdate op 05/05/2023.  
https://github.com/Makerfabs/Makerfabs-ESP32-UWB-DW3000  
- WiFi library. Deze library wordt gebruikt om een unieke ID voor de anchors te bepalen aan de hand van het MAC adres. Deze zit ingebakken in Arduino IDE of PlatformIO met een simpele #include statement.

### Drivers
Voor de ESP32 is een driver nodig zodat de COM-port herkend wordt door PlatformIO. Hiervoor hebben wij de CP210x driver geïnstalleerd.
https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers?tab=downloads


## Architectuur
Tag en anchors:  
- DW3000 library  
- WiFi library  
    
GUI:  
Zie Technisch Ontwerp.


## References
De UDP communicatie is gebaseerd op de volgende GitHub: https://github.com/firebitlab/iot/tree/master/udpTerminalTanpaParsing
De communicatie over UWB gebruikt de DW3000 library
De berekening van de afstand tussen een anchor en een tag gebruikt de DW3000 library


## Usage
In dit scenario wordt er vanuit gegaan dat elk onderdeel de juiste software heeft.  
Sluit de tags en anchors aan op stroom. Start de GUI via Visual Studio of Visual Studio Code. De tags en anchors resetten zichzelf bij een fout.
