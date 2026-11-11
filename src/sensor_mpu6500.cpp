// MPU-6500 / MPU-9250 / MPU-9255 ("MPU-92.65" breakout) accelerometer driver.
// Accel only, 1 kHz, +-16 g, read directly from the shadow-latched output registers over I2C.
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
constexpr uint8_t SMPLRT_DIV = 0x19, CONFIG = 0x1A, GYRO_CONFIG = 0x1B, ACCEL_CONFIG = 0x1C, ACCEL_CONFIG2 = 0x1D;
constexpr uint8_t LP_ACCEL_ODR = 0x1E, WOM_THR = 0x1F, FIFO_EN = 0x23, INT_PIN_CFG = 0x37;
constexpr uint8_t INT_ENABLE = 0x38, ACCEL_XOUT_H = 0x3B, MOT_DETECT_CTRL = 0x69, USER_CTRL = 0x6A;
constexpr uint8_t PWR_MGMT_1 = 0x6B, PWR_MGMT_2 = 0x6C, WHO_AM_I = 0x75;

constexpr float LSB_PER_G = 2048.0f;   // +-16 g
constexpr float LSB_PER_DPS = 32.8f;   // +-1000 deg/s

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
    writeReg(PWR_MGMT_2, 0x00);  // accel + gyro on
    writeReg(SMPLRT_DIV, 0x00);  // 1 kHz
    writeReg(CONFIG, 0x01);
    writeReg(GYRO_CONFIG, 0x10);    // +-1000 deg/s
    writeReg(ACCEL_CONFIG, 0x18);   // +-16 g: a snap must never clip
    writeReg(ACCEL_CONFIG2, 0x07);  // DLPF 420 Hz bandwidth, 1 kHz output
    // Read ACCEL_XOUT_H..ZOUT_L (0x3B..0x40) directly instead of the unframed FIFO:
    // the 512-byte FIFO is not a multiple of 6 bytes and drops framing on simultaneous R/W.
    writeReg(USER_CTRL, 0x00);
    writeReg(FIFO_EN, 0x00);
    writeReg(INT_PIN_CFG, 0x30);    // INT_ANYRD_2CLEAR
    writeReg(INT_ENABLE, 0x01);     // RAW_RDY_EN
    return true;
  }

  size_t read(Sample* out, size_t maxCount) override {
    if (maxCount == 0) return 0;
    const bool readAux = (++auxTick_ >= 20);  // read temp + gyro at 50 Hz (every 20th 1 kHz sample)
    if (readAux) auxTick_ = 0;
    uint8_t buf[14];
    if (!readBurst(ACCEL_XOUT_H, buf, readAux ? 14 : 6)) return 0;
    const int16_t raw[3] = {
        (int16_t)((buf[0] << 8) | buf[1]),
        (int16_t)((buf[2] << 8) | buf[3]),
        (int16_t)((buf[4] << 8) | buf[5]),
    };
    // reject bus-stuck reads (all 0x00 or all 0xFF)
    if ((raw[0] == 0 && raw[1] == 0 && raw[2] == 0) || (raw[0] == -1 && raw[1] == -1 && raw[2] == -1)) return 0;
    if (readAux) {
      const int16_t rawT = (int16_t)((buf[6] << 8) | buf[7]);
      const float tC = rawT / 333.87f + 21.0f;
      tempC_ = isnan(tempC_) ? tC : (tempC_ * 0.9f + tC * 0.1f);
      for (int a = 0; a < 3; a++) {
        const int16_t rg = (int16_t)((buf[8 + a * 2] << 8) | buf[9 + a * 2]);
        const float dps = rg / LSB_PER_DPS;
        if (!gBiasInit_) gBias_[a] = dps;
        else if (fabsf(dps - gBias_[a]) < 1.2f) gBias_[a] += (dps - gBias_[a]) * 0.005f;
        gyro_[a] = dps - gBias_[a];
      }
      gBiasInit_ = true;
    }
    // 3-sample median filter per axis: strips 1-sample high/low byte-tear spikes (e.g. 0xFFFF<->0x0000
    // rollover when an axis sits near 0) while preserving real >= 2 ms mechanical vibrations
    if (!histInit_) {
      for (int a = 0; a < 3; a++) hist_[0][a] = hist_[1][a] = raw[a];
      histInit_ = true;
    }
    float xyz[3];
    for (int a = 0; a < 3; a++) {
      const int16_t p0 = hist_[0][a], p1 = hist_[1][a], p2 = raw[a];
      hist_[0][a] = p1;
      hist_[1][a] = p2;
      const int16_t m = max(min(p0, p1), min(max(p0, p1), p2));
      xyz[a] = m / LSB_PER_G;
      accLp_[a] = oriInit_ ? (accLp_[a] + (xyz[a] - accLp_[a]) * 0.03f) : xyz[a];
    }
    if (readAux) {
      const float accRoll = atan2f(accLp_[1], accLp_[2]) * (180.0f / (float)M_PI);
      const float accPitch = atan2f(-accLp_[0], hypotf(accLp_[1], accLp_[2])) * (180.0f / (float)M_PI);
      if (!oriInit_) {
        ori_[0] = accRoll;
        ori_[1] = accPitch;
        ori_[2] = 0.0f;
        oriInit_ = true;
      } else {
        const float predRoll = ori_[0] + gyro_[0] * 0.02f;
        float dRoll = accRoll - predRoll;
        while (dRoll > 180.0f) dRoll -= 360.0f;
        while (dRoll < -180.0f) dRoll += 360.0f;
        ori_[0] = predRoll + 0.08f * dRoll;
        if (ori_[0] > 180.0f) ori_[0] -= 360.0f;
        if (ori_[0] < -180.0f) ori_[0] += 360.0f;
        ori_[1] = 0.92f * (ori_[1] + gyro_[1] * 0.02f) + 0.08f * accPitch;
        if (fabsf(gyro_[2]) > 0.3f) ori_[2] += gyro_[2] * 0.02f;
        if (ori_[2] > 180.0f) ori_[2] -= 360.0f;
        if (ori_[2] < -180.0f) ori_[2] += 360.0f;
      }
    }
    out[0] = {xyz[0], xyz[1], xyz[2]};
    return 1;
  }

  uint16_t rateHz() const override { return 1000; }
  float tempC() const override { return tempC_; }
  void gyro(float out[3]) const override {
    out[0] = gyro_[0];
    out[1] = gyro_[1];
    out[2] = gyro_[2];
  }
  void orientation(float out[3]) const override {
    out[0] = ori_[0];
    out[1] = ori_[1];
    out[2] = ori_[2];
  }

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

  uint8_t whoami_ = 0;
  int16_t hist_[2][3] = {};
  bool histInit_ = false;
  uint8_t auxTick_ = 0;
  float tempC_ = NAN;
  float gyro_[3] = {};
  float gBias_[3] = {};
  bool gBiasInit_ = false;
  float accLp_[3] = {0, 0, 1};
  float ori_[3] = {};  // roll, pitch, yaw (deg)
  bool oriInit_ = false;
};

}  // namespace

Accel* createAccel() { return new Mpu6500(); }

#endif  // POLH_SENSOR_MPU6500
