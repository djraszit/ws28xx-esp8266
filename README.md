# ws28xx-esp8266
Jest to program do sterowania ledami ws28xx
z modułu esp8266 (ja używam esp12-e)

## Przygotowanie środowiska w systemach Debian i podobnych
* Tworzymy sobie folder i przechodzimy do tego folderu
```bash
mkdir ~/esp8266
cd ~/esp8266
```
* Pobieramy potrzebne narzędzia
```bash
git clone https://github.com/espressif/ESP8266_RTOS_SDK.git

#Pobieramy toolchaina w wersji v4.8.5 na odpowiednią architekture

wget https://dl.espressif.com/dl/xtensa-lx106-elf-linux64-1.22.0-88-gde0bdc1-4.8.5.tar.gz
```
* Pobieramy projekt
```bash
mkdir projects
cd projects
git clone https://github.com/djraszit/ws28xx-esp8266.git
```
Wymagane dodatkowe paczki
* python2

```bash
#Przechodzimy do folderu ws28xx-esp8266
cd ws28xx-esp8266
./build.sh
```
Jeśli wystąpi problem z kompilacją dotyczący pythona
```bash
#Zakładając że jest zainstalowana paczka python2
sudo update-alternatives --install /usr/bin/python python /usr/bin/python2 0
```
W moim przypadku powyższe polecenie pomogło z pythonem

# Flashowanie
* Wymagany esptool

Będąc w folderze projektu uruchamiamy
```bash
#Zakładając że mamy serial port /dev/ttyUSB0
#Zwieramy GPIO0 do masy, resetujemy moduł żyby wszedł do bootloadera
./flash.sh -p /dev/ttyUSB0
```
Po wgraniu, zwieramy GPIO5 do masy, resetujemy żeby wszedł w tryb konfiguracji

Moduł powinien emitować punkt dostępowy o nazwie ESP8266-config

Należy połączyć się z modułem

Następnie połączyć się netcatem i wydać kilka poleceń modułowi

Moduł nasłuchuje na porcie UDP 8000

```bash
nc -u 192.168.4.1 8000

#Konfiguracja
set ap_name:Nazwa sieci
set ap_passwd:hasło sieci
set ip_addr:192.168.1.128
set netmask:255.255.255.0
set gateway:192.168.1.1
set port:8000
set static
leds 192
save network
save params
#Po tych komendach rozłączamy się
#Resetujemy moduł z niezwartym GPIO5 do masy aby przejść do normalnego trybu
```
Moduł powinien się połączyć z twoją siecią wifi i nasłuchiwać na podanym przez ciebie porcie

Można przetestować następująco
```bash
echo -n -e "RAWDATA:\xff\x00\x00\xff\xff\x00\x00\x00\xff" > /dev/udp/192.168.1.128/8000
```
Powinny się zapalić pierwsze trzy ledy, pierwsza czerwona, druga zieona, trzecia niebieska

W przyszłości zamieszczę przykładowy program w języku C na linuxa,
do sterowania tym modułem
