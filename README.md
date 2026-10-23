** ESP_DEPARTURE_BOARD
This is my little handy tabletop departure board. I can finally know when the bus is leaving and never miss it again. **

---------------------------------------------------------------------------------------------------------------------------------------------------------
**EN:**

_ESP32 Tabletop Departure Board (IDS JMK Edition)_

This is my handy tabletop departure board. I can finally know exactly when the bus is leaving and never miss it again! I was able to get it running with live data from my local public transport in Brno (IDS JMK). It was not easy, as the city doesn't make this simple, so the code directly parses HTML data from the transit portal.

Main Features:
Live Departures: Displays up to 4 upcoming connections. Long destination names smoothly scroll across the screen.
Web Interface: Clean, dark-mode web UI to configure everything directly from your phone or PC.
Wi-Fi Manager: No hardcoded passwords. Connect to the board's setup Wi-Fi to enter your home network credentials via a captive portal.
5 Stop Slots & Filtering: Save up to 5 favorite stops. For each stop, you can filter specific lines (e.g., show only bus 44). Switch between stops via the web UI or a physical button.
Smart Sleep Schedule: Set daytime and nighttime schedules to automatically turn off the OLED screen, saving power and preventing burn-in.
Dynamic Footer: Cycles between the current time, date, local temperature, and scrolls official transit alerts/detour information.
Street Lamp Effect: A dedicated GPIO pin drives an optional LED with a realistic flickering street lamp effect that can be scheduled to run only at night.

Hardware Required:
ESP32 (38-pin DevKit recommended)
0.96" OLED display (SSD1306, I2C)
Jumper wires
4x Self-tapping screws
3mm hollow aluminum rod (available at local hobby markets)
Optional: 1x White or yellow LED (for the street lamp)
Optional: 1x Push button (for switching saved stops)

BUT IF YOU ARE ABLE TO CONVERT THIS TO YOUR PUBLIC TRANSPORT SYSTEM, PLEASE SHARE THE CODE! :D I would love to include adaptations for other cities. Contact me here: 

info@bojler.org.

---------------------------------------------------------------------------------------------------------------------------------------------------------
**CZ:**

_ESP32 Stolní Odjezdová Tabule (IDS JMK)_

Zdravím všechny! Tohle je moje šikovná stolní odjezdová tabule. Konečně vím, kdy mi to přesně jede, a už mi neujede bus před nosem. Tento projekt ocení hlavně Brňáci, protože se mi povedlo zprovoznit stahování živých dat z lokálního systému IDS JMK. Nebylo to úplně jednoduché, město k tomu nenabízí čisté API, takže kód přímo parsuje data z HTML portálu.

Hlavní funkce:
Živé odjezdy: Zobrazuje 4 nejbližší spoje. Dlouhé názvy cílových stanic plynule rotují.
Webové rozhraní: Čistý Dark Mode design pro kompletní nastavení tabule přes mobil nebo prohlížeč v PC.
Wi-Fi Manager: Žádné heslo napevno v kódu. Tabule si při prvním spuštění vytvoří vlastní síť (Captive portal), přes kterou ji připojíte k domácí Wi-Fi.
Paměť na 5 zastávek a filtry: Uložte si až 5 zastávek a u každé si vyfiltrujte jen linky, které vás zajímají. Přepínat mezi nimi lze přes web nebo fyzickým tlačítkem.
Plánovač spánku: Nastavte si denní a noční intervaly, kdy se má OLED displej automaticky vypnout. V noci nesvítí do očí a zabraňuje se vypalování pixelů.
Informační patička: Automaticky cykluje aktuální čas, datum, venkovní teplotu v Brně a v případě mimořádností roluje text s informacemi z dopravy.
Efekt pouliční lampy: Volitelná LED dioda umí realisticky simulovat poblikávající starou výbojku. Lze nastavit, aby svítila/blikala pouze v noci.

Potřebný hardware:
ESP32 (doporučeno 38-pinové)
0,96" OLED displej (SSD1306, I2C)
Propojovací kablíky (Wires)
4x samořezné šrouby
3mm hliníková dutá tyč (z hobby marketu)
Volitelné: 1x bílá nebo žlutá LED (pouliční lampa)
Volitelné: 1x tlačítko (přepínání zastávek)

POKUD BY SE ALE NĚKOMU POVEDLO PŘIZPŮSOBIT KÓD NA MHD V JEHO MĚSTĚ, URČITĚ SE PODĚLTE! Rád další města začlením do projektu. Pokud máte dotazy nebo vylepšení, kontaktujte mě na: 

info@bojler.org.
---------------------------------------------------------------------------------------------------------------------------------------------------------
