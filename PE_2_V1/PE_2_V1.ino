#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLEAdvertising.h>

// Partes del sensor de temperatura
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include "ppg_data.h"
#include <stdint.h>
#define I2C_SDA 21
#define I2C_SCL 22

Adafruit_BMP280 bmp; // sensor BMP280 (RECORDAR ESTO ES IMPORTANTE)

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

#define FS 125
#define MIN_PROM 0.6
#define MIN_DIST 50
#define WINDOW 200

BLEServer* pServer = nullptr;
BLECharacteristic* pTxCharacteristic = nullptr;
//BLECharacteristic* pRxCharacteristic = nullptr;

//PARA EL RX


const int PPG_LEN = 1024;

typedef struct __attribute__((packed)) {
    float bpm;
    float spo2;
    float temperatura;
    float hr2;
    uint16_t crc;
} DataPacket;

float detrended[PPG_LEN];
float normalized[PPG_LEN];
float detrended2[PPG_LEN];
float normalized2[PPG_LEN];
int peaks[PPG_LEN];

bool deviceConnected = false;

#include "bpm_estimator.h"
#include "esp_gap_ble_api.h"

// Devuelve true si el central (Android) ya escribió el CCCD para habilitar NOTIFY
bool clientSubscribedToNotify(BLECharacteristic* ch) {

  BLEDescriptor* desc = ch->getDescriptorByUUID(BLEUUID((uint16_t)0x2902)); //Se espera el cccd para continuar con la conexion.
  if (!desc) return false;

  BLE2902* cccd = (BLE2902*)desc;           // 
  return cccd && cccd->getNotifications();  // true si el cliente habilitó NOTIFY
}


class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
    Serial.println("Cliente conectado");
  }

  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
    Serial.println("Cliente desconectado");
    pServer->startAdvertising();
    Serial.println("Reiniciando publicidad BLE");
  }
};


void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0));  // Semilla para valores aleatorios para simular la presión.

  //Iniciamos el sensor de Temperatura
  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.println("Inicializando sensor BMP280...");
  if (!bmp.begin(0x76)) {
    Serial.println("No se encontró sensor BMP280 en 0x76");
    while (1);
  }
  Serial.println("Sensor BMP280 inicializado.");

  BLEDevice::init("NombreDeTuWearable");
  BLEDevice::setPower(ESP_PWR_LVL_N8);  //PARA TODOS LOS COMANDOS BLE  
  
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);

  pTxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_TX,
    BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
  );
  pTxCharacteristic->addDescriptor(new BLE2902());

  //BLECharacteristic* pRxCharacteristic = pService->createCharacteristic(
  //  CHARACTERISTIC_UUID_RX,
  //  BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  //);

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // valores típicos
  pAdvertising->setMaxPreferred(0x12);
  //pAdvertising->setScanResponse(true);
  //pAdvertising->setMinPreferred(0x0c);
  //pAdvertising->setMaxPreferred(0x12);

  pServer->startAdvertising();
  Serial.println("Esperando conexión BLE...");
}

void loop() {

  if (!deviceConnected) {
    delay(100);
    return;
  }

//Falta que app habilite el NOTIFY
  static uint32_t t0 = 0;
  static bool logged = false;

  if (!clientSubscribedToNotify(pTxCharacteristic)) {
    if (!logged) {
      Serial.println("Conectado, esperando suscripción CCCD (NOTIFY)...");
      logged = true;
      t0 = millis();
    }
    // ESTO FALTA PREGUNTAR CON EL GRUPO PARA DEFINIR TIEMPO
    if (millis() - t0 > 8000) { // 8 s
      Serial.println("Timeout esperando.");
      t0 = millis();
    }
    delay(50);
    return;
  }

  if (logged) {
    Serial.println("Cliente suscrito a NOTIFY. Enviando datos...");
    logged = false;
  }

  remove_trend(ppg_data, detrended, PPG_LEN, WINDOW);
  normalize_minmax(detrended, normalized, PPG_LEN);
  int n_peaks = find_peaks(normalized, PPG_LEN, peaks, MIN_PROM, MIN_DIST);

  remove_trend(ppg2_data, detrended2, PPG_LEN, WINDOW);
  normalize_minmax(detrended2, normalized2, PPG_LEN);

  DataPacket pkt;
  pkt.bpm         = estimate_bpm(peaks, n_peaks, FS);
  pkt.spo2        = estimate_spo2(normalized2, normalized, PPG_LEN);
  pkt.temperatura = bmp.readTemperature();
  pkt.hr2         = pkt.bpm + random(-2, 3);
  pkt.crc         = computeCRC((uint8_t*)&pkt, 16);// CRC de 2 BYTES

  Serial.printf("BPM: %.2f | SpO2: %.2f | Temp: %.2f | HR2: %.2f | CRC: 0x%04X\n",
                pkt.bpm, pkt.spo2, pkt.temperatura, pkt.hr2, pkt.crc);


  unsigned long tStart = micros();
  pTxCharacteristic->setValue((uint8_t*)&pkt, sizeof(pkt));
  pTxCharacteristic->notify();        // hay que esperar a la entrega
  unsigned long tEnd = micros();
  Serial.printf("Tiempo set+notify(): %lu us\n", (tEnd - tStart));

  delay(350); 

  esp_sleep_enable_timer_wakeup(3 * 1000000);
  Serial.println("Entrando a deep sleep...");
  esp_deep_sleep_start();
}
