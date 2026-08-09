### Utfört arbete

* **Arkitektur fastställd:** Delad strömförsörjning. MCU drivs via VESC (COMM-port). Belysning (12V) drivs separat via batteri -> Step-down (XL7015) -> Step-up för att undvika överbelastning på VESC BEC eller XL7015.
* **Lödning av kraftmoduler:** Step-down (36V till 5V) och Step-up (5V till 12V) är ihoplödda.
* **Styrkrets för bakljus:** Low-side switch med IRLB8721 MOSFET färdiglödd (Gate med 100Ω serie- och 10kΩ pull-down-motstånd, Drain avsedd för lampans minus, Source till GND).
* **Kabelval:** 0.75 mm² flertrådig (röd/svart) framtagen för 36V-matning från XT60. Mjuka breadboard-kablar valda för internt lågströmsmontage (5V/12V/signal) för att hantera vibrationer.

### Nästa steg

1. **Dela upp strömmatningen:**
* Avlägsna eventuell befintlig 5V-kabel från Step-down (XL7015) `OUT+` till MCU.
* Anslut MCU:ns strömförsörjning (5V och GND) samt UART (TX/RX) direkt till VESC:ens COMM-port.


2. **Etablera Common Ground (Kritiskt):**
* Säkerställ att en jordledare går mellan MCU `GND` och Step-down `OUT-`. Detta krävs för att MOSFET-kretsarna ska få rätt referensspänning för styrsignalen.


3. **Löd framljusets MOSFET-krets:**
* Koppla en andra IRLB8721 för framljuset. Gate till ny GPIO på MCU (via 100Ω och 10kΩ pull-down mot Source). Drain till framljusets minus. Source till Common Ground.


4. **Slutanslutning av ström och belysning:**
* Parallellkoppla fram- och bakljusets pluskablar till Step-up `OUT+` (12V).
* Löd in 0.75 mm² kabel från batteriets XT60-kontakt (36V+ och GND) till Step-down `IN+` och `IN-`.


5. **Test och inkapsling:**
* Bänktesta belysningen i ca 5 minuter och övervaka temperaturen på XL7015-chippet (ska klara 4W last, får bli varmt men inte brännas).
* Trä transparent krympslang över Step-up och Step-down-korten. Fixera med smältlim/elektroniksilikon som dragavlastning och skydd mot vibrationer i chassit. Kylpasta ska inte användas.