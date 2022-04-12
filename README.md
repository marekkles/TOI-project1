# **TOI Project 1 - ESP + Raspberry Pi**
Internet-of-things zariadenie merajúce teplotu a svetlo založené na ESP32. Zariadenie komunikuje pomocou MQTT. Raspberry pi je využívané ako MQTT broker a zároveň ako gateway pre prístup na server založený na [thingsboard.io](https://thingsboard.io/).

# Setup
## Raspberry Pi OS
Využitie **Raspberry Pi Imager** z linku [https://www.raspberrypi.com/software/](https://www.raspberrypi.com/software/) , odporúčanie je nainštalovať 64 bitovú Lite verziu operačného systému Raspbian. Testované konkrétne pre verziu `arm64` z dňa `2022-01-28`.
## Raspberry Pi device tree overlay (optional)
V repozitári sa zároveň nachádza script `presetup.sh`, pomocou ktorého je možné nastaviť na Raspberry Pi 4 overlay pre nastavenie USB-OTG portu (USB-C) ako Ethernet Gadget. Toto umožní pripojenie Rasoberry Pi ku hosťovskému počíttaťu pomocou virtuálnej sieťovej karty.

Skript zároveň nakopíruje do domovského adresára uživateľa `/home/pi` súbor `rpisetup.sh` ktorý slúži k nainštalovanie iných požadovaných aplikácii ako je napríklad docker.

Návod na použitie `presetup.sh` scriptu 

**PO POUŽTÍ JE POTREBNÉ BEZPEČNE ODSTRÁNIŤ PRIPOJENÝ DISK**:
```
presetup.sh BOOT_DIR ROOT_DIR
    BOOT_DIR - mountpoint of RPi boot sector 
    ROOT_DIR - mountpoint of RPi root sector
```

Následne je možné Raspberry Pi pripojiť pomocou USB-C káblu priamo ku hosťovskému PC, ktoré bude zároveň slúžiť aj ako zdroj. Repozitár zároveň obsahuje utilitu `ip.sh` pomocou ktorej je možné nakonfigurovať ip adresu virtuálnej sieťovej karty a prípadne nastaviť `ip_tables` pre využitie ako hosťovského počítača ako **NAT**.

Návod na použitie `ip.sh` scriptu:

```
ip.sh forward-on|forward-off|ip-on|ip-off DEVICE
    forward-on  - set NAT iptable rules
    forward-off - delete NAT iptable rules
    ip-on       - set ip address of interface
    ip-off      - delete ip address of interface
    DEVICE      - Network device
```

## Raspberry Pi inštalácia **Docker**
Pre inštaláciu docker je možné využit priložený *convenience* script ktorý sa nachádza pod `rpiscripts/rpisetup.sh`, skript bude v domovskom adresáry pokiaľ bol využitý predchádzajúci krok. Inak je potrebné tento script na raspberry pi skopírovať napríklad pomocou `scp`. Následne stačí script spustiť:
```
./rpisetup.sh install|docker|hotspot
    install - install docker, docker compose and reboot
    docker - setup docker environment
    hotspot - setup hostspot
```
Je možné vybrať z troch inštalačných fáz.
V prvej fáze sa nainštaluje `docker` a `docker-compose`, Raspberry Pi sa následne reštartuje. V druhej fáze prebieha nastavenie pracovného prostredia docker pre **MQTT broker** a **Python**. Posledná možnosť slúži na vytvorenie hotspotu, po zvolení tejto možnosti sa rpi reštartuje a defaut konfigurácia hotspotu bude:
```
dhcp-range=10.10.0.2,10.10.0.20,255.255.255.0
gateway=10.10.0.1
ssid=rpiIotGateway
wpa_passphrase=abcdefgh
wpa_key_mgmt=WPA-PSK
wpa=2
```

## ESP **PlatformIO**

Pre Platformio je najlepšie nainštalovať Visual Studio Code package `platformio.platformio-ide` prípadne je možné využiť pip package pre platformio pomocou príkazu `pip install platformio`.

## Sieť IoT
Pre mesh sieť na komunikáciu medzi ESP uzlami sme použili knižnicu ESP-NOW. Jedno zariadenie je nutné zvoliť ako *master* uzol odkomentovaním:
`#define CONFIG_IS_GATEWAY`
Mac adresu je nutné definovať v zdrojovom súbore **configuration.h**:
`const static uint8_t master_mac[] = { 0xXX,0xXX,0xXX,0xXX,0xXX,0xXX };`
Ostatné zariadenia, nie je nutné manuálne pridávať (špecifikovať *mac*), zariadenia sa automaticky pridávajú. Všetky zariadenia komunikujú na wifi-channel 7.

Uzol **master** sa pripojí na hotspot na RPi a následne komunikuje s RPi pomocou MQTT. Zvyšné zariadenia posielajú dáta zo senzorov na *master* každých 5 sekúnd, pomocou ESP-NOW. Master uzol následne preposiela prijaté dáta cez MQTT na RPi. Zároveň sám posiela dáta z vlastných senzorov (tiež každých 5 sekúnd). Správy sú minimálne, reprezetnovné priamo štruktúrou a v jednej správe sa odosielajú obe hodnoty (teplota a intenzita svetla).

## Zber a spracovanie dát z ESP
Hodnoty z teplotného senzoru DS18B20 získavame zo zbernice one-wire z pinu 4, pomocou knižníc `owb.h`, `owb-rtm.h` a `ds18b20.h`. Inicializácia prebieha v `app_main()` a samotné získavanie pomocou volania `get_temperature()`. 

Hodnoty z fotorezistoru získavame cez ADC prevodník na pine 34 pomocou funkcie `get_light_intensity()`. Raw data sú prevádzané na hodnotu v lumenoch a vrátená, odpor fotorezistoru je počítaný ako:
```
RLDR = (R * (Vref - Vout))/Vout
Vref = 3.3V
R = 10kOhm
Vout - napätie na pine 34
```
Prevod napätia na lumeny je:
`lumens = 500/(RLDR/1000)`

Dáta sú agregované na RPi z každej minúty. Hodnoty `min`, `max`, `average`, `median` sú počítané pomocou funkcií knižnice `numpy` a odosielané pomocou MQTT na Thingsboard.

## Thingsboard.io
