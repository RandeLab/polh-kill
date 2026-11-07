// MPU-6500 / MPU-9250 / MPU-9255 ("MPU-92.65" breakout) accelerometer driver.
// Accel only, 1 kHz, +-16 g, read through the 512-byte FIFO over I2C.
//
// Wiring (I2C mode): VCC-3V3, GND-GND, SCL-GPIO9, SDA-GPIO8, INT-GPIO7,
// ADD/AD0-GND (address 0x68), NCS-3V3 (keeps the chip in I2C mode), FSYNC-GND.
#include "sensor.h"
#include "config.h"

#ifdef POLH_SENSOR_MPU6500

#include <Wire.h>

namespace {

constexpr uint8_t ADDR = 0x68;
// registers
constexpr uint8_t SMPLRT_DIV = 0x19, CONFIG = 0x1A, ACCEL_CONFIG = 0x1C, ACCEL_CONFIG2 = 0x1D;
constexpr uint8_t LP_ACCEL_ODR = 0x1E, WOM_THR = 0x1F, FIFO_EN = 0x23, INT_PIN_CFG = 0x37;
constexpr uint8_t INT_ENABLE = 0x38, MOT_DETECT_CTRL = 0x69, USER_CTRL = 0x6A;
constexpr uint8_t PWR_MGMT_1 = 0x6B, PWR_MGMT_2 = 0x6C, FIFO_COUNTH = 0x72, FIFO_R_W = 0x74;
constexpr uint8_t WHO_AM_I = 0x75;

constexpr float LSB_PER_G = 2048.0f;  // +-16 g
constexpr size_t CHUNK = 120;         // bytes per I2C read, multiple of 6

class Mpu6500 : public Accel {
 public:
  bool begin() override {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
    pinMode(PIN_ACC_INT, INPUT);
    whoami_ = readReg(WHO_AM_I);
    Serial.printf("[MPU 🧲] WHO_AM_I = 0x%02X\n", whoami_);
    if (whoami_ == 0x00 || whoami_ == 0xFF) return false;  // nothing on the bus

    writeReg(PWR_MGMT_1, 0x80);  // reset
    delay(100);
    writeReg(PWR_MGMT_1, 0x01);  // wake, PLL clock
    delay(10);
    writeReg(PWR_MGMT_2, 0x07);  // accel on, gyro off
    writeReg(SMPLRT_DIV, 0x00);  // 1 kHz
    writeReg(CONFIG, 0x01);
    writeReg(ACCEL_CONFIG, 0x18);   // +-16 g: a snap must never clip
    writeReg(ACCEL_CONFIG2, 0x07);  // DLPF 420 Hz bandwidth, 1 kHz output
    writeReg(INT_PIN_CFG, 0x00);
    writeReg(INT_ENABLE, 0x00);
    resetFifo();
    return true;
  }

  size_t read(Sample* out, size_t maxCount) override {
    uint16_t count = fifoCount();
    if (count >= 504) {  // overflow: data is out of order, start fresh
      resetFifo();
      overflows_++;
      return 0;
    }
    count -= count % 6;
    size_t n = 0;
    uint8_t buf[CHUNK];
    while (count > 0 && n < maxCount) {
      size_t want = min<size_t>(min<size_t>(count, CHUNK), (maxCount - n) * 6);
      if (!readBurst(FIFO_R_W, buf, want)) break;
      for (size_t i = 0; i < want; i += 6) {
        int16_t ax = (int16_t)((buf[i] << 8) | buf[i + 1]);
        int16_t ay = (int16_t)((buf[i + 2] << 8) | buf[i + 3]);
        int16_t az = (int16_t)((buf[i + 4] << 8) | buf[i + 5]);
        out[n++] = {ax / LSB_PER_G, ay / LSB_PER_G, az / LSB_PER_G};
      }
      count -= want;
    }
    return n;
  }

  uint16_t rateHz() const override { return 1000; }

  const char* name() const override {
    switch (whoami_) {
      case 0x70: return "MPU-6500";
      case 0x71: return "MPU-9250";
      case 0x73: return "MPU-9255";
      case 0x74: return "MPU-6515";
      case 0x68: return "MPU-6050?";
      default: return "MPU-65xx clone";
    }
  }

  // Battery mode (later): low-power accel, interrupt on motion above threshold.
  bool enableWakeOnMotion(float thresholdG) override {
    writeReg(PWR_MGMT_1, 0x00);
    writeReg(PWR_MGMT_2, 0x07);
    writeReg(ACCEL_CONFIG2, 0x01);
    writeReg(INT_ENABLE, 0x40);       // wake-on-motion interrupt
    writeReg(MOT_DETECT_CTRL, 0xC0);  // compare against previous sample
    writeReg(WOM_THR, (uint8_t)constrain(thresholdG * 250.0f, 1.0f, 255.0f));  // 4 mg/LSB
    writeReg(LP_ACCEL_ODR, 0x08);     // 62.5 Hz wake-up rate
    writeReg(PWR_MGMT_1, 0x20);       // cycle mode
    return true;
  }

 private:
  void writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
  }

  uint8_t readReg(uint8_t reg) {
    uint8_t v = 0xFF;
    readBurst(reg, &v, 1);
    return v;
  }

  bool readBurst(uint8_t reg, uint8_t* buf, size_t len) {
    Wire.beginTransmission(ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(ADDR, (uint8_t)len) != len) return false;
    for (size_t i = 0; i < len; i++) buf[i] = Wire.read();
    return true;
  }

  uint16_t fifoCount() {
    uint8_t b[2];
    if (!readBurst(FIFO_COUNTH, b, 2)) return 0;
    return ((b[0] & 0x1F) << 8) | b[1];
  }

  void resetFifo() {
    writeReg(USER_CTRL, 0x00);
    writeReg(FIFO_EN, 0x00);
    writeReg(USER_CTRL, 0x04);  // FIFO reset
    delay(1);
    writeReg(USER_CTRL, 0x40);  // FIFO enable
    writeReg(FIFO_EN, 0x08);    // accel into FIFO
  }

  uint8_t whoami_ = 0;
  uint32_t overflows_ = 0;
};

}  // namespace

Accel* createAccel() { return new Mpu6500(); }

#endif  // POLH_SENSOR_MPU6500
