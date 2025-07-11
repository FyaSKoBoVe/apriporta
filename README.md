# ESP8266 D1 mini - Telegram Bot Citofono

Sistema per il controllo remoto di porta e luce scale tramite bot Telegram, con visualizzazione stato e log su display OLED.

## Descrizione del Progetto

Questo progetto consente di controllare un citofono (apriporta) e la luce delle scale tramite un bot Telegram, utilizzando una scheda ESP8266 D1 mini e un display OLED 128x64. Il sistema permette di:

- Aprire la porta d'ingresso da remoto

- Accendere la luce delle scale da remoto

- Visualizzare stato e log delle operazioni su display OLED

- Gestire l’accesso tramite una lista di utenti autorizzati



## Funzionalità principali

- **Controllo porta**: apertura della porta tramite comando Telegram o pulsante fisico.
- **Controllo luce scale**: accensione luce scale tramite comando Telegram.
- **Display OLED**: mostra stato connessione, log operazioni recenti e nome sistema.
- **Gestione accessi**: solo utenti autorizzati possono inviare comandi tramite Telegram.
- **Log operazioni**: registro delle ultime azioni, visibile via Telegram e su display.
- **Feedback sonoro**: buzzer opzionale per conferma sonora delle operazioni.
- **Sicurezza**: controllo accessi tramite lista di chat ID autorizzati.

## Hardware Necessario
- ESP8266 D1 mini
- Display OLED SSD1306 128x64 (I2C)
- 2 Relè (porta, luce)
- Buzzer (opzionale)
- Pulsante (NO - normalmente aperto)
- Alimentazione 3.3V/5V

## Schema collegamenti hardware

| Funzione | Pin D1 mini | GPIO | Note |
| :-- | :-- | :-- | :-- |
| Relè Porta | D5 | GPIO14 | Attivazione HIGH |
| Relè Luce | D8 | GPIO15 | Attivazione HIGH |
| LED Status | D4 | GPIO2 | LED blu integrato |
| Buzzer (opz.) | D7 | GPIO13 | Feedback sonoro |
| Pulsante apriporta | D3 | GPIO0 | Pulsante NO tra GPIO0 e GND |
| OLED SCL | D1 | GPIO5 | Display SSD1306 128x64 |
| OLED SDA | D2 | GPIO4 | Display SSD1306 128x64 |
| VCC relè | 3.3V/5V |  |  |
| VCC OLED | 3.3V |  |  |
| GND | GND |  | Comune a tutti i dispositivi |

## Librerie necessarie

Installare tramite Library Manager di Arduino IDE:

- [UniversalTelegramBot](https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot) (Brian Lough)
- [ArduinoJson](https://arduinojson.org/) (versione 6.x)
- [Adafruit_GFX](https://github.com/adafruit/Adafruit-GFX-Library)
- [Adafruit_SSD1306](https://github.com/adafruit/Adafruit_SSD1306)


## Configurazione iniziale

1. **Crea il bot Telegram**
    - Scrivi a [@BotFather](https://telegram.me/BotFather) su Telegram
    - Usa `/newbot` e segui le istruzioni
    - Copia il token fornito
2. **Trova il tuo chat ID**
    - Invia un messaggio al bot appena creato
    - Vai su `https://api.telegram.org/bot<TUO_TOKEN>/getUpdates`
    - Cerca `"chat":{"id":NUMERO}` e annota il numero
    - In alternativa, usa [@MyIDBot](https://telegram.me/myidbot) e invia `/getid`
3. **Imposta credenziali WiFi e Telegram nel codice**
    - Modifica le variabili `ssid`, `password`, e `BOT_TOKEN`
    - Inserisci i chat ID autorizzati nell’array `authorizedUsers`
4. **Collega l’hardware** come da schema sopra.

## Comandi Telegram disponibili

- `/apri` — Apre la porta d’ingresso
- `/luce` — Accende la luce delle scale
- `/stato` — Mostra lo stato del sistema
- `/log` — Mostra il registro delle ultime operazioni
- `/menu` — Mostra tastiera con pulsanti rapidi
- `/help` — Guida ai comandi


## Descrizione funzionamento

- All’avvio, il sistema si connette al WiFi e verifica la connessione al bot Telegram.
- Il display OLED mostra:
    - Nome sistema (es. “D1 Mini Apriporta”)
    - Ultime operazioni (utente + azione + tempo trascorso) **Nota:** niente al primo avvio.
    - Stato connessioni WiFi e Telegram
- Gli utenti autorizzati possono inviare comandi via Telegram.
- La porta può essere aperta anche tramite pulsante fisico collegato a D3.
- Tutte le operazioni vengono registrate in un log circolare (ultime 5 azioni).
- Feedback acustico tramite buzzer opzionale.
- Timeout automatici per chiusura porta e spegnimento luce.
- Sicurezza: solo i chat ID autorizzati possono comandare il sistema.


## Note

- Il sistema è pensato per un uso domestico (citofono, apertura cancello, ecc.).
- Il codice è facilmente adattabile ad altri relè, display o bot Telegram.
- Per modificare la durata di apertura porta/luce, cambiare le costanti `PORTA_TIMEOUT` e `LUCE_TIMEOUT`.


## Screenshot display OLED

```
+------------------------+
| D1 Mini Apriporta      |
| Mario Porta 2m         |
| Anna  Luce 1h          |
|                        |
| WiFi:OK TG:OK          |
+------------------------+
```


## Autore

- FyaSKoBoVe
- Basato su librerie open source per ESP8266 e Telegram Bot

**Per domande o suggerimenti, apri una issue**
