#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <HomeSpan.h>

// ======================================================
// ThinkNode M2 / SX1262
// ======================================================

constexpr int LORA_CS = 10;
constexpr int LORA_SCK = 12;
constexpr int LORA_MOSI = 11;
constexpr int LORA_MISO = 13;
constexpr int LORA_RST = 21;
constexpr int LORA_BUSY = 14;
constexpr int LORA_DIO1 = 3;
constexpr int LORA_POWER = 48;

// ======================================================
// Warnzeit
// ======================================================

// Warnung, wenn das Tor länger als 10 Minuten offen ist
constexpr unsigned long OPEN_WARNING_MS =
    10UL * 60UL * 1000UL;

// ======================================================
// SX1262
// ======================================================

SX1262 radio = new Module(
    LORA_CS,
    LORA_DIO1,
    LORA_RST,
    LORA_BUSY);

// ======================================================
// Garagenstatus
// ======================================================

bool garageOpen = false;
bool warningActive = false;

unsigned long garageOpenedAt = 0;

// ======================================================
// HomeKit Contact Sensor
// ======================================================

struct GarageContactSensor : Service::ContactSensor
{
    SpanCharacteristic *contactState;

    GarageContactSensor(const char *name)
        : Service::ContactSensor()
    {
        new Characteristic::Name(name);

        // HomeKit:
        // 0 = geschlossen
        // 1 = offen
        contactState =
            new Characteristic::ContactSensorState(0);
    }

    void setOpen(bool open)
    {
        int value = open ? 1 : 0;

        if (contactState->getVal() != value)
        {
            contactState->setVal(value);

            Serial.print("HomeKit ");
            Serial.print(open ? "OPEN" : "CLOSED");
            Serial.println();
        }
    }
};

// ======================================================
// HomeKit Sensoren
// ======================================================

GarageContactSensor *garageSensor = nullptr;
GarageContactSensor *warningSensor = nullptr;

// ======================================================
// HomeKit Setup
// ======================================================

void setupHomeKit()
{
    // Etwas längere WLAN-Verbindungszeiten
    homeSpan.setConnectionTimes(20, 60, 3);

    // WLAN-Credentials und HomeKit-Pairing-Code
    // befinden sich im NVS.
    homeSpan.begin(
        Category::Sensors,
        "Garagentor");

    // ==================================================
    // Accessory 1: Garagentor
    // ==================================================

    new SpanAccessory();

    new Service::AccessoryInformation();
    new Characteristic::Identify();
    new Characteristic::Name("Garagentor");

    garageSensor =
        new GarageContactSensor("Garagentor");

    // ==================================================
    // Accessory 2: Warnung
    // ==================================================

    new SpanAccessory();

    new Service::AccessoryInformation();
    new Characteristic::Identify();
    new Characteristic::Name("Garagentor Warnung");

    warningSensor =
        new GarageContactSensor(
            "Garagentor Warnung");
}

// ======================================================
// LoRa Setup
// ======================================================

void setupLoRa()
{
    // SX1262 einschalten
    pinMode(LORA_POWER, OUTPUT);
    digitalWrite(LORA_POWER, HIGH);

    delay(100);

    // SPI starten
    SPI.begin(
        LORA_SCK,
        LORA_MISO,
        LORA_MOSI,
        LORA_CS);

    Serial.print("SX1262 init ... ");

    // ==================================================
    // EXAKT dieselben LoRa-Einstellungen wie beim
    // funktionierenden Sender/Receiver-Test.
    // ==================================================

    int state = radio.begin(
        868.0,                             // Frequenz MHz
        125.0,                             // Bandbreite kHz
        9,                                 // Spreading Factor
        7,                                 // Coding Rate
        RADIOLIB_SX126X_SYNC_WORD_PRIVATE, // Sync Word
        14,                                // TX Power
        8,                                 // Preamble
        3.3                                // TCXO Voltage
    );

    if (state != RADIOLIB_ERR_NONE)
    {
        Serial.print("FEHLER: ");
        Serial.println(state);

        while (true)
        {
            delay(1000);
        }
    }

    // ThinkNode M2 RF-Switch
    radio.setDio2AsRfSwitch(true);

    Serial.println("OK");
    Serial.println();
    Serial.println("Warte auf Garagentor ...");
}

// ======================================================
// Garagennachricht verarbeiten
// ======================================================

void handleGarageMessage(const String &message)
{
    // ==================================================
    // GARAGE_OPEN
    // ==================================================

    if (message == "GARAGE_OPEN")
    {
        // ------------------------------------------------
        // Tor war bisher geschlossen
        //
        // Nur hier starten wir den 10-Minuten-Timer.
        // ------------------------------------------------

        if (!garageOpen)
        {
            garageOpen = true;
            warningActive = false;

            garageOpenedAt = millis();

            Serial.println();
            Serial.println(
                "*** TOR WURDE GEOEFFNET ***");

            garageSensor->setOpen(true);

            // Warnsensor beim Öffnen zunächst deaktivieren
            warningSensor->setOpen(false);

            Serial.println(
                "10-Minuten-Timer gestartet");
        }

        // ------------------------------------------------
        // Tor war bereits offen
        //
        // Das ist der 60-Sekunden-Heartbeat.
        // Der Timer wird NICHT neu gestartet.
        // ------------------------------------------------

        else
        {
            Serial.println(
                "Heartbeat: Tor weiterhin offen");
        }
    }

    // ==================================================
    // GARAGE_CLOSED
    // ==================================================

    else if (message == "GARAGE_CLOSED")
    {
        if (garageOpen)
        {
            Serial.println();
            Serial.println(
                "*** TOR WURDE GESCHLOSSEN ***");
        }
        else
        {
            Serial.println(
                "Heartbeat: Tor weiterhin geschlossen");
        }

        garageOpen = false;
        warningActive = false;

        // Timer zurücksetzen
        garageOpenedAt = 0;

        // HomeKit aktualisieren
        garageSensor->setOpen(false);

        // Warnung ebenfalls zurücksetzen
        warningSensor->setOpen(false);
    }
}

// ======================================================
// Setup
// ======================================================

void setup()
{
    Serial.begin(115200);

    delay(2000);

    Serial.println();
    Serial.println(
        "=================================");
    Serial.println(
        " GARAGE RECEIVER + APPLE HOME");
    Serial.println(
        "=================================");

    // HomeKit
    setupHomeKit();

    // LoRa
    setupLoRa();

    Serial.println();
    Serial.println(
        "Receiver gestartet.");
}

// ======================================================
// Loop
// ======================================================

void loop()
{
    // ==================================================
    // Apple Home / HomeSpan
    // ==================================================

    homeSpan.poll();

    // ==================================================
    // LoRa
    // ==================================================

    String message;

    // Blockierender Empfang.
    //
    // Diese Variante entspricht unserem funktionierenden
    // LoRa-Test.
    int state = radio.receive(message);

    // --------------------------------------------------
    // Paket erfolgreich empfangen
    // --------------------------------------------------

    if (state == RADIOLIB_ERR_NONE)
    {
        Serial.println();
        Serial.println(
            "============================");

        Serial.print("EMPFANGEN: ");
        Serial.println(message);

        Serial.print("RSSI: ");
        Serial.print(radio.getRSSI());
        Serial.println(" dBm");

        Serial.print("SNR: ");
        Serial.print(radio.getSNR());
        Serial.println(" dB");

        // Nur unsere beiden bekannten Nachrichten
        if (
            message == "GARAGE_OPEN" ||
            message == "GARAGE_CLOSED")
        {
            handleGarageMessage(message);
        }
        else
        {
            Serial.print(
                "Unbekannte Nachricht: ");

            Serial.println(message);
        }

        Serial.println(
            "============================");
    }

    // --------------------------------------------------
    // CRC Fehler
    // --------------------------------------------------

    else if (
        state == RADIOLIB_ERR_CRC_MISMATCH)
    {
        Serial.println(
            "LoRa CRC Fehler");
    }

    // --------------------------------------------------
    // Andere Fehler
    // --------------------------------------------------

    else if (
        state != RADIOLIB_ERR_RX_TIMEOUT)
    {
        Serial.print(
            "LoRa RX Fehler: ");

        Serial.println(state);
    }

    // HomeKit wieder bedienen
    homeSpan.poll();

    // ==================================================
    // 10-Minuten-Warnung
    // ==================================================

    if (
        garageOpen &&
        !warningActive &&
        garageOpenedAt > 0 &&
        millis() - garageOpenedAt >=
            OPEN_WARNING_MS)
    {
        warningActive = true;

        Serial.println();
        Serial.println(
            "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");

        Serial.println(
            " WARNUNG: TOR SEIT 10 MINUTEN OFFEN");

        Serial.println(
            "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");

        // Zweiten HomeKit-Sensor aktivieren
        warningSensor->setOpen(true);
    }

    // HomeKit bedienen
    homeSpan.poll();
}