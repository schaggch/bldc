# SPEEDPED75100

Hardwareprofil auf Basis von `flipsky/hw_75_100_V2.h` und `.c`, fuer den
Flipsky 75100V202 mit **einem originalen R0005 (0,5 mOhm) pro Phase**.
Der Nutzer hat je zwei der drei parallelen Shunts entfernt, sechs von neun
insgesamt. Das Profil ueberschreibt CURRENT_SHUNT_RES auf 0.0005 Ohm;
CURRENT_AMP_GAIN bleibt 20. HW_HAS_3_SHUNTS bezeichnet die drei Messkanaele
und bleibt erhalten. Die Stromskalierung je ADC-Schritt ist damit ein Drittel
der bisherigen Skalierung. Das unveraenderte Flipsky-Originalprofil gilt
weiter fuer die serienmaessige Bestueckung mit drei parallelen Shunts.

Diese Firmware ist ausschliesslich fuer die geaenderte Bestueckung bestimmt.
Motor-/Batteriestromgrenzen und gespeicherte Motorparameter werden nicht
pauschal hoch- oder herunterskaliert. Vorgaben bleiben 40 A Motorstrom,
50 A Batteriestrom, -10 A Brems-/Rekuperationsstrom und 150 A Absolutgrenze.
Diese Werte und die geerbten Hardwarelimits sind keine thermische Freigabe
fuer den einzelnen Shunt. Bei gleichem Phasenstrom ist die Gesamtverlustleistung
in den Shunts dreimal so hoch, die Belastung des einzelnen verbliebenen
Widerstands neunmal so hoch wie vorher pro Widerstand. Widerstandsleistung,
Kuehlung und Messbereich muessen vor Lastbetrieb geprueft werden.
Stromanzeige extern gegenpruefen; Motorwerte, die mit unpassender
Stromskalierung ermittelt wurden, neu einmessen. Terminal `speedped` zeigt
die aktive Shuntkalibrierung.

## SPEEDPED in Jvesctool

Die eigene Rubrik **App Settings > SPEEDPED** liest und schreibt eine separate
Hardwarekonfiguration. Die App-Auswahl bleibt **ADC + UART** (oder ADC).
Die vorhandenen Motor- und ADC-Konfigurationen behalten ihr Datenformat.
Die neue Seite erfordert die passende neue SPEEDPED75100-Firmware; ohne diese
Verbindung sind die Felder und Schreibfunktionen deaktiviert.

Felder: Radumfang in mm, Signale pro Radumdrehung, maxSpeed pedaling in km/h,
maxSpeed AH in km/h, maxADC SH in Prozent sowie
PAS-Timeout in ms, Magnete pro Kurbelumdrehung, Startperioden und Low-Anteil
<50% oder >50%. Genau 50% gilt nie als gueltige Drehrichtung.

Die Geschwindigkeitsgrenzen sind ab Werk **0 = keine Begrenzung**. Dies gilt
fuer PAS-km/h und AH-km/h. Freigaben bleiben erforderlich;
maxADC SH = 0 sperrt SH in allen ADC-Modi. Radumfang (Vorgabe
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
SH begrenzt den normierten ADC-Gasgriffwert in jedem Betriebsmodus. Ein separates SH-eRPM-Limit gibt es nicht mehr. Die Kilometerzaehler sind davon nicht umgestellt.

- Kein SH: Pedalbetrieb mit PAS-Freigabe und maxSpeed pedaling. AH alleine
  erteilt keine Fahrfreigabe.
- Nur SH gehalten: Schiebehilfe mit maxADC SH (alle ADC-Modi) ohne separates eRPM-Limit, ohne PAS-Anforderung.
- SH und AH gehalten: Anfahrhilfe mit maxSpeed AH, ohne PAS-Anforderung.
- In jedem Modus bestimmt der Gasgriff die Leistung. Kein Tempomat.
- Wird SH losgelassen und lag in den letzten 500 ms kein AH-Signal an, werden PAS-Signale verworfen
  und die Erkennung fuer 1000 ms gesperrt. Anschliessend neuer PAS-Safe-Start.
- Ist AH gehalten oder war innerhalb der letzten 500 ms aktiv, entfaellt diese PAS-Sperre. Die
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

SH begrenzt den normierten Gasgriffwert in allen ADC-Betriebsmodi auf
maxADC SH (bisher maxDuty SH). Bei X=15 bleibt die Anforderung bis 15%
unveraendert und wird darueber auf 15% begrenzt. Die Umsetzung in Strom,
Duty oder PID-Drehzahl erfolgt erst danach. Explizite Bremsanforderungen
bleiben erhalten. 0 sperrt SH in allen Modi. Gespeicherte Werte bleiben erhalten.

Die Speed-Funktion (Hall C/PC8, Radumfang und Radimpulszahl) bleibt erhalten,
einschliesslich Anzeige und PAS-/AH-Geschwindigkeitsgrenzen. Ohne Radimpulse
gibt es in diesem Branch keine zusaetzliche Begrenzung oder eRPM-Ersatzgrenze.
Nach zehn Sekunden ohne Radimpuls zeigt Speed null; vorher sinkt der Schaetzwert
bei ueberfaelligen Flanken. Die drei alten Ersatzbegrenzungsfelder sind aus
der Speed-Seite entfernt und werden nur fuer die Speicher-/Protokollkompatibilitaet
mitgefuehrt. Auch vorhandene gespeicherte Werte koennen keine Ersatzbegrenzung ausloesen.

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

## Keydisplay KM5S: echte Fahrdaten, Einstellungen nur lesen

Im Target `SPEEDPED75100` ist der Haupt-UART (Stecker UART1, TX PB10,
RX PB11 / USART3) fuer das Display reserviert: 9600 Baud, 8N1.
App **ADC + UART** verwenden. Bluetooth bleibt am separaten UART2
(TX PC10, RX PC11 / UART4) mit 115200 Baud und VESC-Protokoll.
USB bleibt fuer VESC Tool verfuegbar.

S7/S10 und R2 werden geprueft, beantwortet und fuer das Terminalkommando
`keydisplay` gespeichert. Die Antworten liefern gemessene Radgeschwindigkeit,
Batteriestrom und Controller-/Motortemperatur. S5-Handshake und R7 mit separaten
Temperaturbytes entsprechen dem getesteten Display. Stillstand wird als FFFF
codiert. Unterspannung sowie ADC-Bereichsfehler/Absolut-Ueberstrom werden im
begrenzten KM5S-Fehlerformat gemeldet; weitere VESC-Fehler stehen im Terminal.

Keine Display-Einstellung schreibt Motor-, App- oder SPEEDPED-Konfiguration.
PWM-Anforderung, Walk, Licht und Geschwindigkeitswunsch wirken nicht auf den
Antrieb. PAS-Stufen werden nicht eingefuehrt. Verbindungsabbruch veraendert die
bestehende PAS-/SH-/AH-Steuerung nicht. Bluetooth bleibt unabhaengig.

Die Speed-Codierung verwendet den aus den Anzeigetests abgeleiteten Display-
Umfang 2277 mm fuer Radcode 6. Der reale Radumfang und die reale Magnetzahl
bleiben ausschliesslich in SPEEDPED eingestellt; daraus wird die gemessene
Geschwindigkeit berechnet. Eine andere Display-Radgroesse ist noch nicht
kalibriert und kann die angezeigte Geschwindigkeit veraendern.
Ist-Strom wird vorlaeufig mit Code = Batterieampere * 3 uebertragen (0..85 A);
Rekuperation wird als 0 dargestellt. Die genaue Ampereanzeige ist noch numerisch
am Display zu pruefen. Negative/ungueltige Temperaturen werden als 0 codiert.
Perioden ueber 3500 ms werden nicht mehr kuenstlich begrenzt; die Anzeige sehr
langsamer Fahrt und der gesamte Betrieb am Controller sind noch zu testen.

## Bluetooth: Radsensor-Speed auch in der normalen Werteabfrage

Die Serienfelder von COMM_GET_VALUES (4) und COMM_GET_VALUES_SELECTIVE (50)
bleiben unveraendert. Insbesondere bleibt rpm die echte Motor-eRPM.
Nur fuer dieses SPEEDPED-Profil wird Bit 22 als Erweiterung verwendet:
Radgeschwindigkeit aus Hall C und SPEEDPED-Radumfang/-Impulszahl, identisch
zu COMM_GET_VALUES_SETUP (47), Setup-Selective (51) und dem Keydisplay-Grundwert.

- Kommando 4: Die Antwort hat inklusive Kommando-ID 78 Payloadbytes statt 74.
  Speed liegt bei nullbasiertem Payload-Offset 74..77 (ohne VESC-Rahmen/CRC).
- Kommando 50: Nur Speed anfordern mit Payload `32 00 40 00 00`.
  Antwort `32 00 40 00 00 SS SS SS SS` (Maske 0x00400000 = Bit 22).
- Kommando 51 bleibt unveraendert: `33 00 00 00 40` (Bit 6).
- Speed ist immer signed int32, Big Endian, m/s * 1000, kein IEEE-Float.
  km/h = Rohwert * 3.6 / 1000. Beispiel 36 km/h: Rohwert 10000, `00 00 27 10`.
- Bei fehlendem/abgelaufenem Radsignal ist Speed 0; Motor-eRPM ersetzt ihn nie.

Die Handy-App muss dieses Feld explizit dekodieren oder weiterhin Kommando
47/51 verwenden. Eine vorhandene Berechnung aus rpm liefert keine
Radsensorgeschwindigkeit. Alte Clients, die nur die Standardfelder lesen,
koennen die vier angehaengten Bytes ignorieren. Fuer andere Hardwareprofile
wird das Protokoll nicht erweitert.

## Batteriestrom aus ACS758 auf ADC2 (R3)

ACS758LCB-050B-PFF-T, 5 V, Spannungsteiler 3 kOhm + 3 kOhm auf ADC2/PA6.
Kalibrierung aus den Nutzermessungen direkt am ADC2-Pin:
1.34 V = 1.2 A, 1.55 V = 10 A.
I[A] = 1.2 + (U_ADC2[V] - 1.34) * (8.8 / 0.21).
Nullpunkt damit etwa 1.31136 V; negative Werte bleiben als Ladestrom erhalten.
Keine automatische Nullung beim Start oder im Stillstand, keine Begrenzung
auf 0..50 A. Gemessene Nebenverbraucher bleiben auch bei stehendem Motor sichtbar.
Die Messung verwendet die bestehende ADC_VOLTS-Skalierung des Hardwareprofils.
Bei systematischen Abweichungen gegen Multimeter muessen die Punkte in dieser
Spannungsskalierung neu abgeglichen werden.

Die Eingangsstrom-Getter fuer FOC und BLDC/DC verwenden jetzt den Sensor.
Der vorhandene FOC-Eingangssensorpfad ersetzt i_bus im Regelzyklus; dessen
Modellrechnung des Batteriestroms wird fuer dieses Profil nicht mehr verwendet.
Auch der BLDC-Eingangsstrombegrenzer und die gemessene Eingangsstromrueckfuehrung
in mc_interface nutzen den Sensor. Die normalen Stromfelder in BT/USB/CAN,
Leistung, Keydisplay und die Eingangsstrom-Mapping-Begrenzung erhalten damit
den externen Wert. Die uebrigen Regelalgorithmen und Stromgrenzen bleiben erhalten;
insbesondere die modellbasierte Umrechnung von Batterie- in q-Stromgrenzen in
FOC bleibt als Bestandteil der bestehenden Regelung erhalten. Die bisherigen
Bedingungen der Ah-/Wh-Zaehler bleiben ebenfalls erhalten (keine vollstaendige
Batteriebilanzierung aller Nebenverbraucher im Stillstand).

ADC2 ist fuer den Stromsensor reserviert: Rohspannung bleibt diagnostizierbar,
aber der Wert erzeugt in keinem ADC-Steuertyp eine analoge Bremsanforderung.
Die PAS-/SH-/AH-Freigabe sperrt ADC-Antrieb bei ADC2 unter 0.1 oder ueber 3.2 V.
Das ist nur eine Plausibilitaetspruefung, keine sichere Leitungsbrucherkennung;
direkte USB/CAN/Lisp-Motorbefehle bleiben wie bisher von der App-Freigabe unabhaengig.
Kein zusaetzlicher Hardwarefilter und kein neuer Software-Tiefpass. Bestehende
Filter der Strombegrenzung/Telemetrie bleiben aktiv. Kein Rueckfall auf den
berechneten Batteriestrom. Terminal `speedped` zeigt ADC2, Ampere und Plausibilitaet.
Hardware-Abgleich der Kalibrierung und Funktion unter Last sind noch erforderlich.

## ADC2-Kalibrierung im SPEEDPED-Tab (R4)

Neuer Reiter **ADC2 Kalibrierung** in Windows und Android:
- **ADC2 nullen (nur bei 0 A)** fragt eine aktuelle ADC2-Spannung ab und
  uebernimmt sie als Nullpunkt in den Editor. Die Spannungsabfrage liest ADC2
  auch bei gestoppter ADC-App direkt aus dem Hardware-ADC. Nur bei wirklich 0 A durch
  den Sensor verwenden: vorhandene Nebenverbraucher werden sonst weggenullt.
- Zwei Messpunkte mit Spannung in Volt direkt nach dem 3k/3k-Teiler an ADC2
  und extern gemessenem Batteriestrom in Ampere. Sie bestimmen ausschliesslich
  die Empfindlichkeit: mV/A = (V2-V1)*1000/(A2-A1).
- Der Nullpunkt ist separat sichtbar und editierbar (V, sechs Nachkommastellen).
  I[A] = (U_ADC2-U_null) * (A2-A1)/(V2-V1).
- Vorgaben bleiben aequivalent zu bisher: Nullpunkt 1.311364 V,
  Punkt 1 = 1.340 V / 1.200 A, Punkt 2 = 1.550 V / 10.000 A.
- Nullen aendert nur den Editor. **Write** speichert Nullpunkt und Messpunkte
  gemeinsam und stoppt dabei wie bisher den ADC-Antrieb. Read liest gespeicherte
  Werte; Read Default laedt Vorgaben. Keine automatische Nullung.
- Punkt 2 muss mindestens 0.010 V und 0.100 A oberhalb von Punkt 1 liegen.
  Ungueltige Paare oder Nullpunkte ausserhalb 0.100..3.200 V werden abgewiesen.
- Die gespeicherte Kennlinie gilt sofort auch fuer Telemetrie und Strombegrenzung.

Custom-Konfiguration jetzt 21 Felder / 85 Byte mit neuer Signatur.
Die vier Messpunkt-Double-Felder werden als signed int32 mit Faktor 1000
uebertragen (mV bzw. mA, Bereich 0..60 A). adc2_zero verwendet Faktor 1000000
(Mikrovolt). Zwei abwechselnde EEPROM-Slots benoetigen je 13 Woerter mit
Signatur, Sequenz und Pruefsumme. Der Nullpunkt nutzt das bisher reservierte
Wort 4. Aeltere 11-Wort-Datensaetze behalten Fahr-/PAS-/Sound-Einstellungen
und erhalten die bisherigen ADC2-Vorgaben. Das vorlaeufige 81-Byte-Format
wird ebenfalls gelesen, mit aus seinen Messpunkten abgeleitetem Nullpunkt.
Unterbrochene Migration behaelt den vorherigen Datensatz. Installationsmarker
an Adresse 31 bleibt unveraendert. Passendes neues Tool und Firmware verwenden.
Normale BT-Batteriestrom-Telegramme bleiben unveraendert.
