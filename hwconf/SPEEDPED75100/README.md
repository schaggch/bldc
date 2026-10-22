# SPEEDPED75100

Hardwareprofil auf Basis von `flipsky/hw_75_100_V2.h` und `.c`, fuer den
Flipsky 75100V202 mit drei parallelen 0,5-mOhm-Shunts pro Phase.
Leistungsstufe, Strommessung und Hardwarelimits werden unveraendert uebernommen.

## SPEEDPED in Jvesctool

Die eigene Rubrik **App Settings > SPEEDPED** liest und schreibt eine separate
Hardwarekonfiguration. Die App-Auswahl bleibt **ADC + UART** (oder ADC).
Die vorhandenen Motor- und ADC-Konfigurationen behalten ihr Datenformat.
Die neue Seite erfordert die passende neue SPEEDPED75100-Firmware; ohne diese
Verbindung sind die Felder und Schreibfunktionen deaktiviert.

Felder: Radumfang in mm, Signale pro Radumdrehung, maxSpeed pedaling in km/h,
maxSpeed AH in km/h, maxDuty SH in Prozent sowie
PAS-Timeout in ms, Magnete pro Kurbelumdrehung, Startperioden und Low-Anteil
<50% oder >50%. Genau 50% gilt nie als gueltige Drehrichtung.

Die Geschwindigkeitsgrenzen sind ab Werk **0 = keine Begrenzung**. Dies gilt
fuer PAS-km/h und AH-km/h. Freigaben bleiben erforderlich;
maxDuty SH = 0 sperrt weiterhin SH im Duty-Modus. Radumfang (Vorgabe
2250 mm) und Radimpulse (Vorgabe 1) muessen zum Fahrrad passen. PAS startet mit
500 ms, 5 Magneten, 2 vollstaendigen Startperioden und Low >50%.
Mit **Write auf der SPEEDPED-Seite** speichern; Read liest gespeicherte Werte,
Read Default laedt Vorgaben in den Editor. XML-Export/-Import ist separat von
Motor-/App-XML. Zwei wechselnde EEPROM-Speicherplaetze mit Pruefsumme erhalten
die vorherige gueltige Konfiguration bei einem unterbrochenen Schreibvorgang.
Speichern stoppt den Antrieb und setzt Pedalerkennung und Tasterfreigabe zurueck.

## Hall-Stecker und Hilfsmodi

Der Motor muss sensorlos laufen. Die Sensorport-Einstellung bleibt Hall, damit
die Pins als digitale Eingaenge zur Verfuegung stehen; FOC-Sensormodus muss
Sensorless sein. Eine unpassende Sensorkonfiguration sperrt den ADC-Antrieb.
Encoder-Treiber sind fuer dieses Profil gesperrt, damit sie PC6/7/8 nicht
umkonfigurieren. Die Motor-Hall-Leser liefern den ungueltigen Zustand 0,
damit die Motorerkennung keine Taster-/Radimpulse als Rotorposition lernt.
Die SPEEDPED-Eingangsleser und Live-Punkte lesen weiterhin die echten Pegel.

| Eingang | Pin | Funktion |
| --- | --- | --- |
| Hall A / Hall 1 | PC6 | SH-Taster |
| Hall B / Hall 2 | PC7 | AH-Taster |
| Hall C / Hall 3 | PC8 | Radimpulse |
| PPM | PB6 | PAS |

Alle Eingaenge sind aktiv bei GND und verwenden Pull-ups. Entprellung 2 ms,
Abtastung 1 ms: High und Low muessen jeweils ausreichend lange stabil anliegen.
Die Motor-Hallsensoren duerfen nicht gleichzeitig diese Eingaenge verwenden.

Jvesctool zeigt auf der SPEEDPED-Seite vier Live-Punkte fuer SH, AH,
Speedsensor und PAS: Rot = GND/Low, Gruen = High, Grau = keine aktuellen Daten.
Sie zeigen die rohen elektrischen Pegel, nicht die Fahrfreigabe. Abfrage alle
100 ms bei sichtbarer Seite und passender Firmware; nach 500 ms ohne Antwort
wird die Anzeige grau. Kurze Impulse koennen zwischen zwei Abfragen liegen.
Das read-only Diagnosepaket COMM_CUSTOM_HW_DATA mit Kennung SPEDPIN1 liefert
Bits 0..3 in dieser Reihenfolge, jeweils 1 fuer Low. Andere Pakete bleiben
unveraendert. Aeltere Firmware ohne diesen Endpunkt zeigt graue Punkte.

Die normale Geschwindigkeits-Telemetrie an Jvesctool verwendet Hall C mit
dem SPEEDPED-Radumfang und der Radimpulszahl: dieselbe Berechnung wie die
Pedal-/AH-Begrenzung. Die Firmware uebertraegt m/s; das Tool zeigt km/h an.
Vor der zweiten Radflanke und nach mehr als 10 s ohne Impuls ist der Wert 0.
Radperioden bis 10 s werden akzeptiert (bei 2100 mm und einem Impuls 0,756 km/h).
Ist die naechste Flanke ueberfaellig, verwendet die Berechnung das Signalalter
statt der alten kuerzeren Periode: der Wert faellt beim Verzogern kontinuierlich.
Nach mehr als 10 s Luecke werden zwei neue Flanken benoetigt. Ein einzelner
Radsensor erkennt keine Fahrtrichtung; die Anzeige ist deshalb immer positiv.
SH nutzt im Duty-Modus eine Duty-Grenze. Ein separates SH-eRPM-Limit gibt es nicht mehr. Die Kilometerzaehler sind davon nicht umgestellt.

- Kein SH: Pedalbetrieb mit PAS-Freigabe und maxSpeed pedaling. AH alleine
  erteilt keine Fahrfreigabe.
- Nur SH gehalten: Schiebehilfe mit maxDuty SH (Duty-Modus) ohne separates eRPM-Limit, ohne PAS-Anforderung.
- SH und AH gehalten: Anfahrhilfe mit maxSpeed AH, ohne PAS-Anforderung.
- In jedem Modus bestimmt der Gasgriff die Leistung. Kein Tempomat.
- Wird SH ohne gehaltenen AH-Taster losgelassen, werden PAS-Signale verworfen
  und die Erkennung fuer 1000 ms gesperrt. Anschliessend neuer PAS-Safe-Start.
- Bleibt AH beim Loslassen von SH gehalten, entfaellt diese PAS-Sperre. Die
  Anfahrhilfe endet trotzdem; nur eine gueltige Pedalfreigabe kann weiterfahren.
- Beim Einschalten/Neukonfigurieren gehaltene Taster erst beide loslassen.

Im Duty-Modus reduzieren PAS-/AH-Geschwindigkeit und die eRPM-Ersatzgrenze
den Duty-Sollwert ab 80% der jeweiligen Grenze linear bis null bei 100%.
Bei null wird kein Antriebsstrom angefordert. Die normale positive ADC-Rampenzeit
begrenzt den Wiederanstieg und den Uebergang von SH zu AH ausgehend vom letzten
Sollwert. Andere Steuertypen behalten Abschaltung am Limit und Wiederfreigabe
unter 95% (Hysterese). Bergab kann die Geschwindigkeit hoeher sein.
Explizite Bremsanforderungen bleiben
moeglich. Die Radgeschwindigkeit benoetigt zwei fallende Flanken; die Reaktion
auf Beschleunigung haengt deshalb von der Radimpulszahl ab.

Ausnahme SH im ADC-Duty-Modus: maxDuty SH begrenzt den Sollwert auf X Prozent.
Ab 80% dieser Grenze wird der weitere Anstieg zunehmend langsamer, bis X
erreicht ist. Der Sollwert wird dabei nicht auf 10% reduziert. Gas wegnehmen
reduziert den Sollwert ohne weitere Verzoegerung durch diese SH-Rampe.
Eine separat aktivierte eRPM-Ersatzgrenze bei
fehlendem Radsignal bleibt wirksam. Vorgabe maxDuty SH = 15%. Ein Wert von 0 sperrt SH im
Duty-Modus. Alte Einstellungen bleiben bei
der Migration erhalten. Strom-Skalierung und zusaetzliche PID-Regler sind
nicht enthalten. Duty ist keine garantierte Drehzahl- oder Geschwindigkeitsgrenze.

Im Reiter **Speed** sind drei Werte fuer fehlende Radimpulse einstellbar:
Timeout in Sekunden (Vorgabe 100, Bereich 0..120), Leistungsschwelle in Watt
(elektrische Batterie-Eingangsleistung, Vorgabe 500) und Ersatzgrenze in eRPM
(Vorgabe 10000; 0 = Ersatzbegrenzung aus). Auch Timeout 0 s deaktiviert die
Ersatzbegrenzung vollstaendig und loescht eine bereits aktive Ersatzbegrenzung.
Die Zeit zaehlt seit der letzten entprellten Radflanke, anfangs seit Initialisierung.
Nach Ablauf des Timeouts ohne neue Radflanke und bei Gasanforderung mit
Leistung strikt oberhalb der Schwelle wird die Ersatzgrenze aktiviert.
Sie gilt in allen Fahrmodi zusaetzlich zu deren Grenzen: im Duty-Modus mit
Reduktion ab 80%, sonst mit 95%-Hysterese.
Sie bleibt bei sinkender Leistung aktiv, bis wieder eine gueltige Radperiode
vorliegt oder die Ersatzgrenze auf 0 gesetzt wird. Rekuperation loest sie nicht aus.
Die bisherige feste Zwei-Sekunden-Antriebssperre entfaellt; die Anzeige wird
erst nach zehn Sekunden ohne Radimpuls null und sinkt vorher bei ueberfaelliger
Flanke. Die Ersatzbegrenzung hat davon unabhaengig ihren eigenen Timeout.
Ein einzelner Sensor kann
Stillstand nicht von einem Kabelbruch unterscheiden. Alte SPEEDPED-Einstellungen
werden uebernommen; die drei neuen Werte beginnen mit 5 s, 0 W und 0 eRPM.

## Sound

Der Reiter Sound enthaelt Startton, Fahrmelodie, Lautstaerke und Tempo.
Beide Tonarten sind ab Werk aus. Der Startton ist ein eigenes zweitoeniges
Retro-Motiv; die Fahrmelodie ist ein eigenes achttoeniges Motiv, keine
Super-Mario-Melodie. Ihr Tempo folgt den gueltigen PAS-Magnetperioden.
Die Ausgabe verwendet die FOC-Audiofunktion des Motors, beim Start mit
Nullstrom-Anforderung. Die Tonspannung ist auf maximal 0,2 V begrenzt;
die Eignung am realen Motor ist noch nicht validiert.
Messsperre, Fehler und App-Sperre stoppen den Ton; ein Watchdog beendet die
Audioausgabe bei mehr als 20 ms ohne Aktualisierung. Die Konfiguration umfasst
jetzt 16 Felder (65 Byte) mit Migration der bisherigen Speicherformate.
Host-Tests und Tool-Integration sind bestanden; ein Hardware-Soundtest steht aus.

## Tretsensor und Freigabe

- Anschluss: PPM-Signal, PB6, Eingang mit Pull-up. Sensor zieht nach GND.
- Einstellbare Magnetperioden pro Kurbelumdrehung (Vorgabe 5).
- Eine Periode wird zwischen zwei fallenden Flanken vollstaendig gemessen.
- Vorwaerts: Low-Zeit groesser oder kleiner als High-Zeit, je nach Einstellung.
- Safe-Start: einstellbare Zahl vollstaendiger Vorwaertsperioden (Vorgabe 2).
  Die erste fallende Flanke synchronisiert nur; eine beim Einschalten bereits
  laufende Teilperiode wird nicht mitgezaehlt. Zwei Perioden entsprechen 144 Grad
  Kurbelbewegung ab dieser Synchronisationsflanke.
- Ein Signalzustand muss mindestens 2 ms stabil sein. Abtastung: 1 ms.
- Nach Ablauf des eingestellten Timeouts (Vorgabe 1000 ms) seit der letzten bestaetigten gueltigen
  Periode erlischt die Freigabe, mit zusaetzlich maximal einem ADC-Abtastzyklus
  (normal maximal 2 ms). Entprellung und Scheduling haben Millisekunden-Toleranz.
- Falsche Drehrichtung bzw. Low = High sperrt nach Abschluss der gemessenen Periode.
- Timeout, Motorfehler, neue App-Konfiguration oder eine Abtastluecke ueber
  20 ms erfordern erneut den Safe-Start. Dauer-High/Dauer-Low gibt nicht frei.

## ADC-Ansteuerung

Die Freigabe wirkt auf die **ADC-App**, inklusive ihrer Duty-,
Drehzahl- und CAN-Multi-ESC-Ausgaben. Ohne Freigabe wird der Antriebsstrom aktiv
auf null gesetzt; die Antriebsrampe wird geloescht. Explizite Bremsstrom-Modi
werden nicht durch den Pedalsensor gesperrt. Bestehende ADC-Fehlerpruefungen,
Gasgriff-Safe-Start und Controllerlimits gelten weiterhin.

Unterstuetzt werden `ADC` und `ADC + UART`; `NONE` und `UART` bleiben fuer
Service ohne ADC-Antrieb verfuegbar. Andere gespeicherte App-Auswahlen
werden bei diesem Hardwareprofil zu `ADC + UART`; Servoausgabe wird deaktiviert.
ADC-Updatefrequenzen unter 500 Hz werden auf 500 Hz angehoben.
PPM ist weder als Gassteuerung noch als ADC-Tempomat-/Rueckwaertstaster verfuegbar.
Im reinen ADC-Modus bleiben die separaten UART-RX/TX-Taster moeglich.

Die ADC-Spannungsbereiche und der ADC-Steuertyp muessen in VESC Tool passend zum
Gasgriff bzw. Bremsgeber eingestellt werden. Der Default-Steuertyp bleibt wie
im Original `NONE`, bis eine Ansteuerung konfiguriert wurde.

Die Pedalfreigabe ist **keine globale Motorsperre**: direkte Motorbefehle ueber
USB/UART/CAN, Lisp oder Motorerkennung sind davon unabhaengig. Dadurch bleibt
die Inbetriebnahme ueber VESC Tool moeglich.

Waehrend der manuellen R/L-Messung pausiert dieses Profil die App-Ausgabe und
setzt danach PAS und Tasterfreigabe zurueck. ADC-Steuertyp NONE gibt keine
Motorbefehle aus. Jvesctool zeigt den Messstart und nach 180 s ohne Antwort
eine Timeout-Meldung; diese Meldung bricht die Messung am Controller nicht ab.
Die Aenderungen wurden offline geprueft; eine erfolgreiche R/L-Messung am
angeschlossenen Controller muss noch bestaetigt werden.

## Diagnose und Pruefung

VESC-Tool-Terminal: `speedped`

Die Zeile WHEEL zeigt raw_low, period_ms, age_ms, valid und die GPIO-Register
fuer PC6/7/8: GPIO_modes=0 bedeutet alle drei Input, pullups=21 alle drei Pull-up.
Die 2-ms-Entprellung kann laengere elektrische Stoerimpulse nicht erkennen.
Die Korrekturen sind durch Host-Tests und Build geprueft, noch nicht am Fahrzeug.

Ausgabe: Pegel `low`, Anzahl gueltiger Perioden `periods`, Freigabe `enabled`,
Alter des letzten gueltigen Signals `age_ms` und Trittfrequenz `pedal_rpm`.
Bei 5 Magneten und 200-ms-Perioden werden 60 U/min angezeigt.

Vor einem Einsatz am Fahrzeug am entlasteten Antrieb pruefen: gesperrt nach
Einschalten, Freigabe erst nach zwei vollstaendigen Vorwaertsperioden, Sperre bei
Rueckwaertsbewegung, Signalunterbrechung sowie festem High/Low, anschliessender
erneuter Safe-Start und weiterhin verfuegbare Bremse. Gasgriff-Kalibrierung,
Signalpolaritaet und Low/High-Zeitverhaeltnis am realen Sensor verifizieren.

Host-Tests: `tests/speedped75100/` (PAS, Modi, Geschwindigkeitsgrenzen, Speicher).
Die Einstellbeschreibung wird mit `generate_bike_schema.py` aus diesem Ordner
erzeugt. Bei Aenderungen auch `bike.xml` nach `vesc_tool/res/config/bike.xml`
kopieren und beide Projekte neu bauen.
Build: `make SPEEDPED75100` oder mit dem lokalen Starter
`powershell -NoProfile -ExecutionPolicy Bypass -File outputs/build.ps1 -Board SPEEDPED75100`.

Das entfernte SH-eRPM-Feld wird bei Migration alter Konfigurationen verworfen.
Alle anderen Werte bleiben erhalten; das neue Schema wird vom Controller gelesen.

## Importierter Werksstand und einmalige Uebernahme

Die XML-Quellen liegen in defaults/. generate_stock_defaults.py erzeugt 325
Motor-/App-Vorgaben; generate_bike_schema.py erzeugt die 16 SPEEDPED-Vorgaben.
Beschreibende Motor-Metadaten existieren nur im Tool und werden nicht in die
Motor-Konfigurationsstruktur geschrieben. Der alte sh_erpm-Eintrag wird ignoriert.

Beim ersten Start dieses Presets werden die vorhandenen Motor-, App- und
SPEEDPED-Einstellungen ersetzt. Erst nach erfolgreichem Speichern aller drei
Konfigurationen wird EEPROM-HW-Wort 31 auf Revision 0x53503101 gesetzt.
Bei Abbruch erfolgt ein erneuter Versuch vor App-/Kommunikationsstart.
Spaetere Starts (auch erneutes Flashen desselben Presets) erhalten Benutzeraenderungen.
Bei einem kuenftigen neuen Preset muss die Revision bewusst erhoeht werden.
Read Default liefert jederzeit die importierten Vorgaben.
