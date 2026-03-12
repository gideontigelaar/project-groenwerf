# Meetsensoren voor meting grashoogte

## Ultrasoon sensor (5V | <8mA)
Bijvoorbeeld: https://www.okaphone.com/artikel.xhtml?id=493353

### Voordelen:
- Goedkoop, prijs ligt gemiddeld rond de €10
- Waterdicht verkrijgbaar, dit scheelt werk met afsealen
- Groot bereik, tot 450cm

### Nadelen:
- Nauwkeurigheid van ± 10mm, dit is een best grote range als je gras wil meten
- Minimale afstand van grond tot sensor is 20cm. Of dit bruikbaar is, is afhankelijk van de afmetingen van de maaier die we gaan gebruiken
- Trillingen van maaidek kunnen valse metingen veroorzaken
- Brede meethoek van ≤75°, kan dus makkelijk omliggende obstakels meepakken

## Time of Flight sensor (3,3 - 5 V | 35mA)
Bijvoorbeeld: https://www.okaphone.com/artikel.xhtml?id=498851

### Voordelen:
- Goedkoop, prijs gemiddelde is rond de €10
- Goed bereik, gemiddeld min. 10cm, max. 180cm
- Smalle meethoek, kan heel precies meten

### Nadelen:
- Meestal niet waterdicht, vereist extra behuizing om te voorkomen dat het kapot gaat
- Werkt minder goed als er buiten fel zonlicht is
- Gevoelig voor stof op de lens

## Infrarood afstandssensor

### Voordelen:
- Goedkoop
- Klein van formaat

### Nadelen:
- Werkt niet goed in lichte omgevingen
- Niet waterdicht
- Max. bereik van ± 80cm. Dit is erg laag

<br><br>

# Sensoren/technieken voor afvangen/verwerken van trillingen

## Accelerometer & gyroscoop
Het opmerken en mogelijk wegfilteren van niet-accurate metingen door trillingen die worden opgepakt door de sensor.

### Voor Ultrasone sensor:
- Trillingen maken hier veel uit voor de echo terugkomsttijd
- Grote winst als je met de accelerometer inconsistenties door trillingen wegfiltert

### Voor Time of Flight sensor:
- Hier maken trillingen minder uit doordat het werkt met een laser
- Winst aan accuracy door trillingen weg te filteren met accelerometer is met ToF sensor kleiner

## Extra: afgeronde metingen nemen over een bepaald stuk
Het software-matig verwerken van de data voordat het gebruikt wordt om uitschieters door obstakels en trillingen weg te filteren.

- Outliers verwijderen: metingen die sterk afwijken van de rest worden weggegooid
- Mediaan: van de resterende metingen wordt de middelste waarde genomen, dit is beter dan een gemiddelde omdat het minder gevoelig is voor uitschieters
- 80e of 90e percentiel: dit betekent dat de waarde wordt genomen waarbij 80% of 90% van de metingen daaronder valt. Dit kan handig zijn omdat gras ongelijk groeit: de hogere pieken zijn bepalend voor of het maaimoment nodig is, niet het gemiddelde

<br><br>

# Conclusie: na testen van sensoren

### Infrarood afstandssensor
- De IR-afstandssensor valt meteen af vanwege de gevoeligheid voor zonlicht, het beperkte bereik en het ontbreken van waterbestendigheid. Deze sensor hebben we dan verder ook niet getest omdat deze zeer ongeschikt bleek.

### Time of Flight sensor
- Deze sensor is erg accuraat, ook bij trillingen. Het zal, als we deze sensor gaan gebruiken, veel werk schelen bij het maken van systemen voor het opvangen en tegenwerken van schokken.
- De meeste ToF sensoren die we konden vinden zijn niet waterdicht, wat betekent dat er een extra behuizing gemaakt moet worden.
- De metingen zijn eenvoudig uit te lezen met redelijk eenvoudige code.

### Ultrasoon sensor
- Na side-to-side comparison tussen de Ultrasoon sensor en de ToF sensor, kwamen we erachter dat de ToF sensor veel meer accurate metingen gaf.
- De ultrasoon sensor is vaak waterdicht verkrijgbaar en dus wel makkelijker in te bouwen.

### Accelerometer
- Met het aansluiten van een accelerometer kun je makkelijk trillingen afmeten, en eventuele compensaties maken in de meetdata als de trillingen heftig zijn.

# Datasheets
Onderstaand de datasheets van de sensoren die we getest hebben. Deze sensoren lijken geschikt te zijn voor ons project.

### VL53L1X ToF sensor - datasheet
- https://www.st.com/resource/en/datasheet/vl53l1x.pdf

### RCWL-1601 ultrasonic sensor - datasheet
- https://media.digikey.com/pdf/Data%20Sheets/Adafruit%20PDFs/4007_Web.pdf

### ADXL345 accelerometer - datasheet
- https://www.analog.com/media/en/technical-documentation/data-sheets/adxl345.pdf