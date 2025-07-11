/*
 * ####################################################
 * ESP8266 D1 mini - Telegram Bot Citofono            #
 * Controllo porta e luce scale tramite bot Telegram  #
 * Visualizzazione stato e log su display OLED        #
 * ####################################################
 *
 * ISTRUZIONI SETUP:
 * 1. Installa le librerie necessarie dal Library Manager:
 *    - UniversalTelegramBot by Brian Lough (compatibile con ESP8266)
 *    - ArduinoJson by Benoit Blanchon (versione 6.x)
 *    - Adafruit_GFX
 *    - Adafruit_SSD1306
 * 
 * 2. Configura il bot Telegram:
 *    - Scrivi a @BotFather su Telegram
 *    - Usa /newbot per creare un nuovo bot
 *    - Salva il token ricevuto
 * 
 * 3. Trova il tuo Chat ID:
 *    - Invia un messaggio al tuo bot
 *    - Vai su: https://api.telegram.org/bot<TUO_TOKEN>/getUpdates
 *    - Cerca "chat":{"id":NUMERO} e salva il numero
 *    - Oppure usa @MyIDBot su Telegram e invia /getid
 * 
 * 4. Configura le credenziali WiFi e bot nel codice.
 *
 * 5. Utilizza un display Oled da 0.92 inch SSD1306
 *    di 128x64 pixel.
 * 
 * COLLEGAMENTI HARDWARE (per D1 mini):
 * - D5 (GPIO14)  -> Relè Porta 
 * - D8 (GPIO15)  -> Relè Luce 
 * - D4 (GPIO2)   -> LED Status (LED blu integrato sul D1 mini)
 * - D7 (GPIO13)  -> Buzzer (opzionale)
 * - D1 (GPIO5)   -> SCL OLED Dispaly
 * - D2 (GPIO4)   -> SDA OLED Dispaly
 * - D3 (GPIO0)   -> Pulante (NO=Normalmente Aperto) tra GPIO0 e massa
 * - 3.3V/5V -> VCC dei relè
 * - 3.3V Vcc OLED Display
 * - GND     -> GND comune/GND OLED Dispaly
 *
 * NOTA BENE - Il PULSANTE deve essere premuto fino a conferma sonora.
 *
 */


// ====== INCLUDE E DEFINIZIONI ======
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ================ CONFIGURAZIONE WIFI =====================
const char* ssid = "your_ssid";
const char* password = "your_password";

// ==== TOKEN DEL BOT TELEGRAM (ottienilo da @BotFather) ====
#define BOT_TOKEN "xxxxxxxx:xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

// ========= LISTA CHAT ID DEGLI UTENTI AUTORIZZATI =========
const long authorizedUsers[] = {
  41708465L,
  1646851004L,
  111541688L,
};

// =================== PIN CONFIGURATION ===================
#define RELAY_PORTA D5     // GPIO14 <-- Relè High per attivare
#define RELAY_LUCE  D8     // GPIO15 <-- Relè High per attivare
#define LED_STATUS  D4     // GPIO2  <-- LED BuiltIn
#define BUZZER_PIN  D7     // GPIO13 <-- Buzzer
#define BUTTON_PORTA D3    // GPIO0  <-- Pulsante fisico apriporta

// ================== TIMING CONFIGURATION =================
const unsigned long PORTA_TIMEOUT = 1000;   // Porta aperta per 1 secondi
const unsigned long LUCE_TIMEOUT = 1000;    // Luce accesa per 1 secondi
const int BOT_REQUEST_DELAY = 500;         // Controlla messaggi ogni secondo

// ===================== DISPLAY OLED ======================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ==================== VARIABILI GLOBALI ==================
const int numAuthorizedUsers = sizeof(authorizedUsers) / sizeof(authorizedUsers[0]);

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

unsigned long lastTimeBotRan;
unsigned long portaOpenTime = 0;
unsigned long luceOnTime = 0;

// ============= VARIABILI GLOBALI INTERRUPT ===============
volatile bool buttonInterruptFlag = false;
volatile unsigned long lastInterruptTime = 0;
const unsigned long interruptDebounceDelay = 100; // debounce in ms

// ============== STATO CONNESSIONE DISPALY ================
bool wifiOk = false;
bool telegramOk = false;
String lastDisplayContent = "";

// ================= STRUTTURA PER LOGGING =================
struct LogEntry {
  unsigned long timestamp;
  long chatId;
  String operation;
  String userName;
};
LogEntry operationLog[5];

// ============= FUNZIONE ISR PER IL PULSANTE ==============
void ICACHE_RAM_ATTR buttonISR() {
  unsigned long now = millis();
  // Debounce: accetta solo se è passato abbastanza tempo dall’ultimo trigger
  if (now - lastInterruptTime > interruptDebounceDelay) {
    buttonInterruptFlag = true;
    lastInterruptTime = now;
  }
}

// ============= FUNZIONI DISPLAY CENTRATO ================
// - Questa unzione serve per centrare e settare la dimensione voluta. 
//   utile anche per dispaly più grandi.
void drawCenteredText(String text, int y, int textSize = 1) {
  int16_t x1, y1;
  uint16_t w, h;
  display.setTextSize(textSize);
  display.getTextBounds(text.c_str(), 0, y, &x1, &y1, &w, &h);
  int x = (SCREEN_WIDTH - w) / 2;
  display.setCursor(x, y);
  display.print(text);
  display.setTextSize(1); // Ripristina la dimensione standard
}

// =================== FUNZIONI DISPLAY ====================
String getShortLogLine(int idx) {
  if (operationLog[idx].timestamp == 0) return "";
  String action;
  if (operationLog[idx].operation == "LUCE_ACCESA") action = "Luce";
  else if (operationLog[idx].operation == "PORTA_PULSANTE") action = "Porta";
  else action = "Porta";
  String name = operationLog[idx].userName;
  String ago = formatTimeAgoShort(millis() - operationLog[idx].timestamp);
  return name + " " + action + " " + ago;
}


String formatTimeAgoShort(unsigned long ago) {
  unsigned long seconds = ago / 1000;
  if (seconds < 60) return String(seconds) + "s ";
  unsigned long minutes = seconds / 60;
  if (minutes < 60) return String(minutes) + "m ";
  unsigned long hours = minutes / 60;
  if (hours < 24) return String(hours) + "h ";
  unsigned long days = hours / 24;
  return String(days) + "g ";
}

String getConnStatusLine() {
  String w = wifiOk ? "WiFi:OK" : "WiFi:--";
  String t = telegramOk ? "TG:OK" : "TG:--";
  return w + " " + t;
}

void updateDisplay() {
  String line1 = "D1 Mini Apriporta";
  String line2 = getShortLogLine(0);
  String line3 = getShortLogLine(1);
  String line4 = getConnStatusLine();
  String line5 = getShortLogLine(2);   
 

  String displayContent = line1 + "|" + line2 + "|" + line3 + "|" + line5 + "|" + line4;
  if (displayContent == lastDisplayContent) return;
  lastDisplayContent = displayContent;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  //display.setCursor(0, 0);
  //display.println(line1);
  drawCenteredText("D1 Mini Apriporta", 0, 1); // Centrata

  display.setCursor(0, 16);
  display.println(line2);

  display.setCursor(0, 26);
  display.println(line3);

  display.setCursor(0, 36);
  display.println(line5);

  drawCenteredText(getConnStatusLine(), 57);

  display.display();
}

// ====== SETUP ======
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== D1 mini Telegram Citofono ===");
  Serial.println("Inizializzazione sistema...");

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 non trovato!"));
    //for(;;);
  }
  display.clearDisplay();
  display.display();

  initializeLog();
  setupPins();
  connectToWiFi();

  client.setInsecure();

  testTelegramBot();
  systemReady();

  updateDisplay();
}

// ====== LOOP PRINCIPALE ======
void loop() {
  if (millis() > lastTimeBotRan + BOT_REQUEST_DELAY) {
    checkTelegramMessages();
    lastTimeBotRan = millis();
  }

  // Gestisce i timeout automatici
  handleTimeouts();
  // Lampeggia LED di stato
  blinkStatusLED();
  // Controlla connessione WiFi
  checkWiFiConnection();
  // Gestione pulsante fisico apriporta
  checkButtonApriPorta();
  // Aggiorna display se necessario
  updateDisplay();
}

// ==================== FUNZIONI SETUP =====================
void setupPins() {
  pinMode(RELAY_PORTA, OUTPUT);
  pinMode(RELAY_LUCE, OUTPUT);
  pinMode(LED_STATUS, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PORTA, INPUT_PULLUP); // Pulsante con pull-up interno
  attachInterrupt(digitalPinToInterrupt(BUTTON_PORTA), buttonISR, FALLING);


  digitalWrite(RELAY_PORTA, LOW);
  digitalWrite(RELAY_LUCE, LOW);
  digitalWrite(LED_STATUS, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.println("Pin configurati");
}

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connessione WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  wifiOk = (WiFi.status() == WL_CONNECTED);
  updateDisplay();

  if (wifiOk) {
    Serial.println("\nWiFi connesso!");
    Serial.print("Indirizzo IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Intensità segnale: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    successBeep();
  } else {
    Serial.println("\nErrore connessione WiFi!");
    errorBeep();
  }
}

void testTelegramBot() {
  Serial.print("Test connessione bot Telegram...");
  if (bot.getMe()) {
    Serial.println(" OK!");
    Serial.println("Bot pronto per ricevere comandi");
    telegramOk = true;
    successBeep();
  } else {
    Serial.println(" ERRORE!");
    Serial.println("Verifica il token del bot");
    telegramOk = false;
    errorBeep();
  }
  updateDisplay();
}

void systemReady() {
  Serial.println("\n=== SISTEMA PRONTO ===");
  Serial.println("Comandi disponibili:");
  Serial.println("- /apri  -> Apre la porta");
  Serial.println("- /luce  -> Accende luce scale");
  Serial.println("- /stato -> Mostra stato sistema");
  Serial.println("- /log   -> Mostra log operazioni");
  Serial.println("- /help  -> Mostra aiuto");
  Serial.println("=======================\n");

  digitalWrite(LED_STATUS, HIGH);
  playStartupMelody();
}

// =============== GESTIONE MESSAGGI TELEGRAM ==============
void checkTelegramMessages() {
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  telegramOk = (numNewMessages >= 0);
  updateDisplay();
  while (numNewMessages) {
    handleNewMessages(numNewMessages);
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    telegramOk = (numNewMessages >= 0);
    updateDisplay();
  }
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    if (bot.messages[i].type == "message") {
      String chat_id = bot.messages[i].chat_id;
      String text = bot.messages[i].text;
      String from_name = bot.messages[i].from_name;
      long user_id = chat_id.toInt();

      if (!isAuthorizedUser(user_id)) {
        handleUnauthorizedUser(chat_id, from_name);
        continue;
      }
      processCommand(chat_id, text, from_name, user_id);
    } else if (bot.messages[i].type == "callback_query") {
      String chat_id = bot.messages[i].chat_id;
      String callback_data = bot.messages[i].text;
      String from_name = bot.messages[i].from_name;
      String callback_query_id = bot.messages[i].query_id;
      long user_id = chat_id.toInt();

      if (!isAuthorizedUser(user_id)) {
        bot.answerCallbackQuery(callback_query_id, "❌ Non autorizzato");
        continue;
      }
      processCallback(chat_id, callback_data, from_name, user_id, callback_query_id);
    }
  }
}

void processCommand(String chat_id, String text, String from_name, long user_id) {
  if (text == "/start") {
    sendWelcomeMessage(chat_id);
  }
  else if (text == "/apri") {
    handleApriPorta(chat_id, from_name, user_id);
  }
  else if (text == "/luce") {
    handleAccendiLuce(chat_id, from_name, user_id);
  }
  else if (text == "/stato") {
    handleStatoSistema(chat_id);
  }
  else if (text == "/log") {
    handleMostraLog(chat_id);
  }
  else if (text == "/help") {
    sendHelpMessage(chat_id);
  }
  else if (text == "/menu") {
    sendMainMenu(chat_id);
  }
  else {
    handleUnknownCommand(chat_id);
  }
}

void processCallback(String chat_id, String callback_data, String from_name, long user_id, String query_id) {
  if (callback_data == "APRI_PORTA") {
    handleApriPortaCallback(chat_id, from_name, user_id, query_id);
  }
  else if (callback_data == "ACCENDI_LUCE") {
    handleAccendiLuceCallback(chat_id, from_name, user_id, query_id);
  }
  else if (callback_data == "STATO_SISTEMA") {
    handleStatoSistemaCallback(chat_id, query_id);
  }
  else if (callback_data == "MOSTRA_LOG") {
    handleMostraLogCallback(chat_id, query_id);
  }
  else if (callback_data == "AIUTO") {
    handleAiutoCallback(chat_id, query_id);
  }
  else {
    bot.answerCallbackQuery(query_id, "❌ Comando non riconosciuto");
  }
}

// ===================== GESTIONE COMANDI ==================
void sendWelcomeMessage(String chat_id) {
  String welcome = "🏠 *Benvenuto nel Sistema Citofono*\n\n";
  welcome += "Il sistema è operativo e pronto all'uso.\n\n";
  welcome += "Usa i pulsanti qui sotto per controllare il sistema o digita i comandi manualmente.\n\n";
  welcome += "*Comandi testuali disponibili:*\n";
  welcome += "🚪 /apri - Apre la porta d'ingresso\n";
  welcome += "💡 /luce - Accende luce delle scale\n";
  welcome += "ℹ️ /stato - Stato del sistema\n";
  welcome += "📋 /log - Registro operazioni\n";
  welcome += "🎛️ /menu - Mostra menu pulsanti\n";
  welcome += "❓ /help - Guida completa\n\n";
  welcome += "_Sistema sicuro con controllo accessi_";

  bot.sendMessage(chat_id, welcome, "Markdown");
  delay(500);
  sendMainMenu(chat_id);
}

// =================== GESTIONE APRIPORTA ==================
void handleApriPorta(String chat_id, String from_name, long user_id) {
  apriPorta();

  String message = "🚪 *Porta Aperta*\n\n";
  message += "✅ Comando eseguito con successo\n";
  message += "⏰ La porta è stata APERTA\n";
  message += "👤 Richiesta da: " + from_name;

  bot.sendMessage(chat_id, message, "Markdown");
  logOperation(user_id, "PORTA_APERTA", from_name);
  updateDisplay();

  Serial.println("✅ Porta aperta su richiesta di: " + from_name);
  confirmationBeep();

  delay(2000);
  sendMainMenu(chat_id);
}

// ================= GESTIONE ACCENDI LUCE =================
void handleAccendiLuce(String chat_id, String from_name, long user_id) {
  accendiLuce();

  String message = "💡 *Luce Accesa*\n\n";
  message += "✅ Luce delle scale accesa\n";
  message += "⏰ Si spegnerà automaticamente tra 30 secondi\n";
  message += "👤 Richiesta da: " + from_name;

  bot.sendMessage(chat_id, message, "Markdown");
  logOperation(user_id, "LUCE_ACCESA", from_name);
  updateDisplay();

  Serial.println("✅ Luce accesa su richiesta di: " + from_name);
  confirmationBeep();

  delay(2000);
  sendMainMenu(chat_id);
}



// =========== GESTIONE PULSANTE FISICO APRIPORTA ==========
void checkButtonApriPorta() {
  if (buttonInterruptFlag) {
    buttonInterruptFlag = false;
    apriPortaDaPulsante();
  }
}


// ================= APRIPORTA DA PULSANTE =================
void apriPortaDaPulsante() {
  apriPorta();
  logOperation(0, "PORTA_PULSANTE", "Pulsante");
  //portaApertaDaPulsante = true;
  Serial.println("✅ Porta aperta da pulsante fisico");
  confirmationBeep();
  updateDisplay();
}

// ============ VISUALIZZAZIONE STATO DI SISTEMA ===========
void handleStatoSistema(String chat_id) {
  String status = getDetailedSystemStatus();
  bot.sendMessage(chat_id, status, "Markdown");
  delay(1000);
  sendMainMenu(chat_id);
}

// ====================== MOSTRA LOG =======================
void handleMostraLog(String chat_id) {
  String logStr = getFormattedLog();
  bot.sendMessage(chat_id, logStr, "Markdown");
  delay(1000);
  sendMainMenu(chat_id);
}

// =================== MESSAGGIO DI HELP ===================
void sendHelpMessage(String chat_id) {
  String help = "❓ *Guida Comandi Sistema Citofono*\n\n";
  help += "*Comandi Principali:*\n";
  help += "🚪 `/apri` - Apre la porta d'ingresso\n";
  help += "💡 `/luce` - Accende luce scale\n";
  help += "*Comandi Informativi:*\n";
  help += "ℹ️ `/stato` - Stato completo del sistema\n";
  help += "📋 `/log` - Ultimi log operazioni\n";
  help += "❓ `/help` - Questa guida\n\n";
  help += "*Sicurezza:*\n";
  help += "🔒 Solo utenti autorizzati possono usare il sistema\n";
  help += "📊 Tutte le operazioni vengono registrate\n";
  help += "🔄 Spegnimento automatico per sicurezza";

  bot.sendMessage(chat_id, help, "Markdown");
  delay(1000);
  sendMainMenu(chat_id);
}

// ================== COMANDO SCONOSCIUTO ==================
void handleUnknownCommand(String chat_id) {
  String message = "❓ *Comando Non Riconosciuto*\n\n";
  message += "Il comando inserito non è valido.\n";
  message += "Usa /help per vedere tutti i comandi disponibili o /menu per i pulsanti.";
  bot.sendMessage(chat_id, message, "Markdown");
}

// ================ CONTROLLO AUTORIZZAZIONI ===============
void handleUnauthorizedUser(String chat_id, String from_name) {
  String message = "🚫 *Accesso Negato*\n\n";
  message += "Non sei autorizzato ad utilizzare questo sistema.\n";
  message += "Contatta l'amministratore per richiedere l'accesso.";
  bot.sendMessage(chat_id, message, "Markdown");
  Serial.println("🚫 Accesso negato per: " + from_name + " (ID: " + chat_id + ")");
  errorBeep();
}

// ==================== MENU E CALLBACK ====================
void sendMainMenu(String chat_id) {
  String menuText = "🎛️ *Menu Controllo Citofono*\n\n";
  menuText += "Scegli un'azione usando i pulsanti qui sotto:";
  String keyboard = "[[{\"text\":\"🚪 Apri Porta\",\"callback_data\":\"APRI_PORTA\"}],"
                    "[{\"text\":\"💡 Accendi Luce\",\"callback_data\":\"ACCENDI_LUCE\"}],"
                    "[{\"text\":\"ℹ️ Stato Sistema\",\"callback_data\":\"STATO_SISTEMA\"},"
                    "{\"text\":\"📋 Mostra Log\",\"callback_data\":\"MOSTRA_LOG\"}],"
                    "[{\"text\":\"❓ Aiuto\",\"callback_data\":\"AIUTO\"}]]";
  bot.sendMessageWithInlineKeyboard(chat_id, menuText, "Markdown", keyboard);
}

void handleApriPortaCallback(String chat_id, String from_name, long user_id, String query_id) {
  apriPorta();
  bot.answerCallbackQuery(query_id, "🚪 Porta Aperta! Apriporta Azionato!");
  String message = "🚪 *Porta Aperta*\n\n";
  message += "✅ Comando eseguito con successo\n";
  message += "⏰ La porta è stata aperta!\n";
  message += "👤 Richiesta da: " + from_name;
  bot.sendMessage(chat_id, message, "Markdown");
  logOperation(user_id, "PORTA_APERTA", from_name);
  updateDisplay();
  Serial.println("✅ Porta aperta su richiesta di: " + from_name);
  confirmationBeep();
  sendMainMenu(chat_id);
}

void handleAccendiLuceCallback(String chat_id, String from_name, long user_id, String query_id) {
  accendiLuce();
  bot.answerCallbackQuery(query_id, "💡 Luce Accesa! Si spegnerà automaticamente");
  String message = "💡 *Luce Accesa*\n\n";
  message += "✅ Luce delle scale accesa\n";
  message += "⏰ Si spegnerà automaticamente!\n";
  message += "👤 Richiesta da: " + from_name;
  bot.sendMessage(chat_id, message, "Markdown");
  logOperation(user_id, "LUCE_ACCESA", from_name);
  updateDisplay();
  Serial.println("✅ Luce accesa su richiesta di: " + from_name);
  confirmationBeep();
  delay(1000);
  sendMainMenu(chat_id);
}

void handleStatoSistemaCallback(String chat_id, String query_id) {
  bot.answerCallbackQuery(query_id, "📊 Recupero stato sistema...");
  String status = getDetailedSystemStatus();
  bot.sendMessage(chat_id, status, "Markdown");
  delay(1000);
  sendMainMenu(chat_id);
}

void handleMostraLogCallback(String chat_id, String query_id) {
  bot.answerCallbackQuery(query_id, "📋 Recupero log operazioni...");
  String logStr = getFormattedLog();
  bot.sendMessage(chat_id, logStr, "Markdown");
  delay(1000);
  sendMainMenu(chat_id);
}

void handleAiutoCallback(String chat_id, String query_id) {
  bot.answerCallbackQuery(query_id, "❓ Guida ai comandi...");
  sendHelpMessage(chat_id);
}



// ================ LOG, UTILITY E STATO ==================
void initializeLog() {
  for (int i = 0; i < 5; i++) {
    operationLog[i].timestamp = 0;
    operationLog[i].chatId = 0;
    operationLog[i].operation = "";
    operationLog[i].userName = "";
  }
}

void logOperation(long chatId, String operation, String userName) {
  for (int i = 4; i > 0; i--) {
    operationLog[i] = operationLog[i - 1];
  }
  operationLog[0].timestamp = millis();
  operationLog[0].chatId = chatId;
  operationLog[0].operation = operation;
  operationLog[0].userName = userName;
  updateDisplay();
}

String getFormattedLog() {
  String logStr = "📋 *Registro Operazioni*\n\n";
  int validEntries = 0;
  for (int i = 0; i < 5; i++) {
    if (operationLog[i].timestamp > 0) validEntries++;
  }
  if (validEntries == 0) {
    logStr += "Nessuna operazione registrata.";
    return logStr;
  }
  for (int i = 0; i < validEntries; i++) {
    String timeAgo = formatTimeAgo(millis() - operationLog[i].timestamp);
    String emoji = "🚪";
    String operationName = "PORTA APERTA";
    if (operationLog[i].operation == "LUCE_ACCESA") {
      emoji = "💡";
      operationName = "LUCE ACCESA";
    }
    logStr += "• " + emoji + " " + operationName + "\n";
    logStr += "  👤 " + operationLog[i].userName + "\n";
    logStr += "  ⏰ " + timeAgo + "\n\n";
  }
  return logStr;
}

String getDetailedSystemStatus() {
  String status = "ℹ️ *Stato Sistema Citofono*\n\n";
  status += "*Connettività:*\n";
  status += "📶 WiFi: " + String(WiFi.status() == WL_CONNECTED ? "✅ Connesso" : "❌ Disconnesso") + "\n";
  status += "📡 Segnale: " + String(WiFi.RSSI()) + " dBm\n\n";
  status += "*Dispositivi:*\n";
  status += "🚪 Porta: " + String(portaOpenTime > 0 ? "🟢 Aperta" : "🔴 Chiusa") + "\n";
  status += "💡 Luce: " + String(luceOnTime > 0 ? "🟢 Accesa" : "🔴 Spenta") + "\n\n";
  status += "*Sistema:*\n";
  status += "⏱️ Uptime: " + formatUptime(millis()) + "\n";
  status += "🔋 Memoria libera: " + String(ESP.getFreeHeap()) + " bytes\n";
  status += "👥 Utenti autorizzati: " + String(numAuthorizedUsers);
  return status;
}

String formatUptime(unsigned long milliseconds) {
  unsigned long seconds = milliseconds / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  unsigned long days = hours / 24;
  String uptime = "";
  if (days > 0) uptime += String(days) + "g ";
  if (hours % 24 > 0) uptime += String(hours % 24) + "h ";
  if (minutes % 60 > 0) uptime += String(minutes % 60) + "m ";
  uptime += String(seconds % 60) + "s";
  return uptime;
}

String formatTimeAgo(unsigned long ago) {
  unsigned long seconds = ago / 1000;
  if (seconds < 60) return String(seconds) + " secondi fa";
  unsigned long minutes = seconds / 60;
  if (minutes < 60) return String(minutes) + " minuti fa";
  unsigned long hours = minutes / 60;
  if (hours < 24) return String(hours) + " ore fa";
  unsigned long days = hours / 24;
  return String(days) + " giorni fa";
}

// ====== CONTROLLO RELÈ E TIMEOUT ======
void apriPorta() {
  digitalWrite(RELAY_PORTA, HIGH);
  portaOpenTime = millis();
  Serial.println("🚪 Porta aperta");
}

void accendiLuce() {
  digitalWrite(RELAY_LUCE, HIGH);
  luceOnTime = millis();
  Serial.println("💡 Luce accesa");
}

void handleTimeouts() {
  if (portaOpenTime > 0 && millis() - portaOpenTime > PORTA_TIMEOUT) {
    digitalWrite(RELAY_PORTA, LOW);
    portaOpenTime = 0;
    Serial.println("🚪 Porta chiusa automaticamente");
  }
  if (luceOnTime > 0 && millis() - luceOnTime > LUCE_TIMEOUT) {
    digitalWrite(RELAY_LUCE, LOW);
    luceOnTime = 0;
    Serial.println("💡 Luce spenta automaticamente");
  }
}

// ====== UTILITY ======
bool isAuthorizedUser(long chatId) {
  for (int i = 0; i < numAuthorizedUsers; i++) {
    if (authorizedUsers[i] == chatId) {
      return true;
    }
  }
  return false;
}

// ================= FEEDBACK SONORO E LED =================
void confirmationBeep() {
  int melody[] = {587, 659, 523, 262, 392};
  int duration[] = {200, 200, 200, 200, 800};
  for (int i = 0; i < 5; i++) {
    tone(BUZZER_PIN, melody[i], duration[i]);
    delay(250);
  }
  noTone(BUZZER_PIN);
}

void successBeep() {
  int melody[] = {262, 330, 392};
  int duration[] = {200, 200, 200};
  for (int i = 0; i < 3; i++) {
    tone(BUZZER_PIN, melody[i], duration[i]);
    delay(250);
  }
  noTone(BUZZER_PIN);
  delay(2000);
}

void errorBeep() {
  tone(BUZZER_PIN, 1000);
  delay(100);
  noTone(BUZZER_PIN);
  delay(80);
  tone(BUZZER_PIN, 1500);
  delay(100);
  noTone(BUZZER_PIN);
}

void playStartupMelody() {
  int melody[] = {587, 659, 523, 262, 392};
  int duration[] = {200, 200, 200, 200, 800};
  for (int i = 0; i < 5; i++) {
    tone(BUZZER_PIN, melody[i], duration[i]);
    delay(250);
  }
  noTone(BUZZER_PIN);
}

void blinkStatusLED() {
  static unsigned long lastBlink = 0;
  static bool ledState = false;
  if (millis() - lastBlink > 1000) {
    ledState = !ledState;
    digitalWrite(LED_STATUS, ledState ? HIGH : LOW);
    lastBlink = millis();
  }
}

// ================= CONTROLLO CONNESSIONE =================
void checkWiFiConnection() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 30000) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnesso, riconnessione...");
      WiFi.reconnect();
      wifiOk = false;
    } else {
      wifiOk = true;
    }
    updateDisplay();
    lastCheck = millis();
  }
}
