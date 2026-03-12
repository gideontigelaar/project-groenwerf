# Onderzoek sensoren aansluiten
We gebruiken voor dit project een Raspberry pi pico w.

### Lijst van sensoren
- Time of flight sensoren 3.0V/5V 40 mA
- Ultrasoon sensor 5V 15mA

### Aansluitingen
- Time of flight sensor heeft 5 pins, maar we gebruiken er maar 4. Dit zijn de stroom en grond pins voor het stroom. en de SCL en de SDA pins voor de data die verstuurd moet worden. De SCL pin mag niet aan elke pin aangebracht worden. De SDA pin mag net zoals de SCL niet op elke pin aangesloten worden. Op de foto onderaan dit document kan je zien welke pins SDA of SCL zijn. Doordat je deze pins niet aan elke pins mag aansluiten kan je kabels hebben die overlappen, waardoor het er uitziet als een spaghetti gerecht.

- Ultrasoon sensor heeft 3 pins (Stroom, Grond en een data pin). De stroom en grond pin zijn alleen voor stroom leveren en stroom weer veilig terugkrijgen voor een stroomkring. De data pin verstuurd de data waar we wat mee kunnen. Niet elke soort pin kan op elke pin aangesloten worden op de raspberry pi pico, want deze moet wel om moeten kunnen gaan met de data. Dit kan zorgen dat je kabels moet koppelen op specifieke pins die niet aan de goede kant van de raspberry pi pico staan.

### Limieten

De raspberry pi pico kan per GPIO pin maximaal tot 12 mA en het wordt aangeraden om het onder 9 mA te houden per pin. de maximum aantal mA die de raspberry pi pico kan leveren over alle GPIO pins is 50 mA. Hierdoor kan als er veel sensoren aangesloten moeten worden dat het handiger is om het direct aan een stroom bron te brengen en niet aan de raspberry pi pico zelf. Daardoor hebben we een converter nodig die de stroom van de accu in de grasmaaier zou kunnen omzetten naar stroom die kan werken voor de sensoren. 

De aansluitingen kunnen verwarrend worden als er allemaal kabels door elkaar heen lopen. Dit kan ervoor zorgen dat het product niet goed onderhouden kan worden en erg onoverzichtelijk is.

De aansluitingen kunnen kapot gaan door middel van viezigheid, hitte en vocht. Dit zou ervoor zorgen dat het product niet meer zou gaan werken en er onderdelen vervangen moeten worden.

### Oplossingen

Om de aansluitingen goed te laten werken in een bewegend voorwerp moeten de kabels genoeg speling hebben om wat te bewegen, zodat de aansluiting niet breken. Maar niet genoeg speling dat ze te lang zijn en bijvoorbeeld in de maaier terecht kunnen komen. Nog een oplossing om er bij toe te voegen zou zijn om de kabels te solderen aan de raspberry pi zodat er nog minder kans is op ontkoppeling.

Om verwarring te verkomen op welke kabels welke aansluitingen zijn, moeten we werken met een kleur gecodeerd systeem. Bijvoorbeeld:

| Kleur  | Betekenis       |
|--------|-----------------|
| Rood   | Positief stroom |
| Zwart  | Negatief stroom |
| Oranje | PWM.MOSI,SPI    |
| Geel   | GPIO            |
| Groen  | SDA             |
| Blauw  | SCL             |
| Wit    | Reset           |

<br>

Om de aansluitingen te beschermen kan je gebruik maken van kabelgoten en kouzen. De kabelgoten zouden ervoor zorgen dat de kabels beschermd worden tegen groter vuil en klappen. De kouzen zouden ervoor zorgen dat er geen vocht en kleiner vuil binnen/op de kabels komt.

<br>

![Raspberry pi pico](https://devboards.info/images/boards/raspberry-pi-pico/raspberry-pi-pico-pinout.webp)

### Benodigde onderdelen
- Sensor die gekozen is.
- Kabels om deze aan te sluiten
- Raspberry pi pico
- Kabel kouzen
- Kabelgoten

### Conclusie
We hebben dus een paar methodes van aansluiten, maar het meest optimale voor dit project is de meest veilige optie. Dit is met kabel goten en kouzen de kabels beschermen. Speling in de kabels zodat deze niet gaan knappen door trekkracht. Kabels kleurgecodeerd organiseren om verwarring te voorkomen. En de kabels solderen op de raspberry pi pico.

### Bronnen
https://www.alldatasheet.com/html-pdf/1652177/EDATEC/RP2040/2306/9/RP2040.html
https://cdn.sparkfun.com/datasheets/Sensors/Proximity/HCSR04.pdf
https://www.st.com/resource/en/datasheet/vl53l0x.pdf

