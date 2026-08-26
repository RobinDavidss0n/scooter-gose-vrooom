### Utfört arbete (uppdaterad 2026-08-26)

* **Arkitektur:** Enstegs 36V -> XL7015 -> 12V för all belysning (front, bak,
  båda blinkers). Ingen separat step-up-krets längre — det gamla
  två-stegs-upplägget (36V -> ~5V -> step-up -> 12V) är övergivet. MCU:ns
  5V/GND/UART går direkt från VESC:ens COMM-port, inte via step-down-kortet.
* **Styrkretsar (MOSFET, låg-sida IRLB8721) klara i diagrammet för alla 4
  lampor:** bak, fram, vänster blinker, höger blinker. Varje Gate har 100Ω
  serie- och 10kΩ pull-down-motstånd, Drain till lampans minus, Source till
  respektive jord (`CG` i chassit, `HB_GND` vid styret).
* **Blinkers = riktiga 12V-lampor** (uppmätt 0,11A@12V), inte små
  5V-indikatorlysdioder som först antaget — därför MOSFET-drivna som fram-
  /bakljus, inte transistor+resistor. Ingen extra strömbegränsande resistor
  behövs (XL7015 har gott om marginal, se nedan).
* **XL7015 effektbudget verifierad mot riktig databladssiffra:** rekommenderad
  <7W (583mA) vid 12V, hård gräns 9,6W/0,8A. Värsta tänkbara samtidiga last
  (fram + bakljus bromsning + en blinker) är ~3,6-4W — gott om marginal.
  Den gamla "~4W, blir varm"-anteckningen härifrån gällde det övergivna
  två-stegs-upplägget vid ~5V-steget, inte dagens 12V-arkitektur.
* **Kabelfärgschema fastställt** för både STEM-kabeln (6 ledare) och MCU:ns
  fasta 12-ledar-flatkabel, matchat funktion-för-funktion så långt möjligt
  (TX=Blå, RX=Grön, GND=Svart, 5V=Röd på båda kablarna). Se
  `wiring/diagram.mmd` för fullständig legend (nod-`style`-block).
  **Varning kvar att lösa fysiskt:** `VSYS` (5V) och `3V3` är BÅDA röda på
  MCU:ns flatkabel — måste märkas separat innan lödning, annars risk att mata
  5V in på den 3,3V-only MCP23017 (abs max ~5,5V).
* **MCP23017 I2C GPIO-expander tillagd** för blinker-knappar och
  blinker-utgångar (adress 0x20). Native `3V3`-pin på MCU-headern hittad och
  används direkt — ingen separat 5V->3,3V-LDO (`REG3V3`) behövs längre, den
  är borttagen ur diagrammet.
* **Firmware (`esp32/src/blinkers.cpp/.h`) klar och byggd:** MCP23017-drivna
  vänster/höger-knappar fungerar dubbelt som blink- och
  hastighetsgräns-justering (`mode unrestricted 0|1`-konsolkommando väljer
  vilket). `esp32/src/lights.cpp/.h` styr fram-/bakljus via PWM (GPIO15/21),
  inklusive broms-ljusstyrka.
* **Diagram (`wiring/diagram.mmd`) är nu den enda källan att följa** — full
  färgkodning per ledare/pinne, MOSFET-benkoppling (Gate/Drain/Source =
  Vit/Grå/Svart på alla 4), och en checked-in högupplöst rendering finns i
  `wiring/diagram.dist.png` (rendera om med kommandot i `/memories/repo/wiring.md`
  efter varje ändring).
* **Chassit är nu fysiskt klart:** batteri, XT60, VESC, XL7015 och bakljus
  inkopplade. XL7015-trimpotten är verifierad till 12V med multimeter innan
  något ljus kopplades in, och VESC:en startar och lyser som den ska. Kvar är
  styret (MCU, framljus, blinkers, MCP23017) plus den nya mjukstartskretsen
  nedan.
* **Mjuk på/av-strömbrytare tillagd i diagrammet** (`PWR_RELAY`/`PWR_LATCH` i
  `diagram.mmd`), löser att MCU:n inte kan stänga av och sedan starta om sig
  själv med bara batteriet rakt in på VESC:en (ingen brytare fanns alls
  tidigare — enda "av" var att dra ur XT60:n):
  - **Relä (JD1912, 12V-spole, 40A SPST-NO, inbyggd flyback-diod)** i serie
    på 36V+ mellan XT60 och VESC:en. Endast VESC:en gate:as — `SD`/12V-skenan
    är fortsatt alltid på, annars finns ingen ström att bootstrap:a reläet
    med från helt avstängt läge. Inget läcker ändå: alla lampors MOSFET-gates
    default:ar lågt via sina pull-downs när MCU:n/expandern är strömlös.
    Reläet är dimensionerat mot VESC:ens faktiska konfigurerade
    batteriströmsgräns (20-25A säkert/overdrive, ~30A teoretiskt tak med
    degradering — se `vesc/setup.md`), 40A ger gott om marginal.
  - **Latch-transistor** (liten NPN, t.ex. BC337 — finns redan i
    transistorsatsen) driver reläspolen. Basen får sitt "på"-kommando via en
    diod-OR från antingen strömknappen (bootstrap, fungerar helt utan MCU)
    eller MCU:ns hold-signal (håller kvar reläet efter att knappen släppts,
    och kan släppa den för avstängning).
  - **Strömknappen** (i styret) tappar det redan-alltid-på lokala 12V och
    skickar en switchad-12V-trigger nedåt på en ny ledare (`N2`) — ingen egen
    spänningsreferens behövs.
  - **MCU:n läser samma knapp lokalt** via `MCP_GPB1`, ingen fjärde ledare
    eller extra knapp-pol behövs: en 10kΩ/3,3kΩ-delare på knappens egen
    switchade 12V-signal skalar ner den till ~3V vid tryck (0V i vila via
    bottenmotståndet till `HB_GND`) — säkert under MCP23017:ans 3,3V-logik/
    5,5V-absolutgräns. Läses AKTIV-HÖG, till skillnad från BTN_L/BTN_R som är
    aktiv-låg via intern pull-up. Hålls den 2s medan systemet kör triggas en
    mjuk avstängning (nollar gas/broms, stoppar VESC:en, släpper hold-linjen)
    — en kort knapptryckning gör ingenting, så en debounce-glitch eller stöt
    kan inte stänga av strömmen mitt i en åktur.
  - **2 nya tunna signalledare ner till chassit** (`N2` trigger, `N3`
    MCU-hold), buntas med `N1` genom samma ferritkärna. Ingen kraftledning —
    bara logiknivå, delar `N1`:s befintliga jordretur.
  - **Firmware (`esp32/src/power.cpp/.h`) klar och byggd:** `power_init()`
    sätter hold-pinnen (`MCP_GPB0`) hög vid boot och konfigurerar
    knapp-sense-pinnen (`MCP_GPB1`); `power_poll()` (anropas varje loop-varv)
    avkänner det 2s-långa trycket; `power off`-konsolkommandot (se
    `console.cpp`) triggar samma avstängning direkt, utan att behöva hålla
    knappen.

### Nästa steg (fysiskt arbete kvar)

1. **Löd in rest-motstånden på bakljusets MOSFET:** 100Ω serie (mellan OG3 och
   Gate) samt 10kΩ pull-down (Gate -> `CG`) — diagrammet fick dessa i en
   audit men de är inte fysiskt monterade än.
2. **Bygg om blinker-drivkretsarna** till MOSFET-mönstret (var tidigare
   planerat som transistor+resistor från 5V, är nu fel arkitektur — se ovan).
3. **Dra den nya kabeln** (nu 3 ledare: `N1` GND + `N2`/`N3` för
   strömbrytaren) längs STEM-kabeln (utanför gångjärnet, buntad i segment,
   genom ferritkärnan) och koppla in MCP23017 + native 3V3-pinnen enligt
   färgschemat i diagrammet.
4. **Märk VSYS/3V3-ledarna** på MCU:ns flatkabel innan lödning (se varning
   ovan).
5. **Bygg latch-kretsen och montera reläet i chassit** per `PWR_RELAY`/
   `PWR_LATCH` i diagrammet (relä + latch-transistor + 2x 1N4148-diod +
   1kΩ basmotstånd) — kan göras när som helst, chassit är fortsatt åtkomligt
   efter att styret är stängt.
6. **Koppla in strömknappen i styret** till det lokala 12V och till `N2`.
7. **Bänktesta all belysning** (fram, bak, båda blinkers) samtidigt, övervaka
   XL7015-temperatur — förväntas vara långt under gränsen denna gång.
8. **Slutlig inkapsling:** krympslang + smältlim/elektroniksilikon som
   dragavlastning runt step-down-kortet (ingen kylpasta).