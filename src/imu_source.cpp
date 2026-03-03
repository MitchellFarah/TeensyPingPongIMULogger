// imu_source.cpp - IMU hardware abstraction implementation

#include "imu_source.h"
#include "config.h"

// Global pointer for ISR access
ImuSource* g_imuSource = nullptr;

// ISR wrapper - calls the member function through global pointer
void drdy_isr_wrapper() {
    if (g_imuSource) {
        g_imuSource->handleIsr();
    }
}

ImuSource::ImuSource() {
    g_imuSource = this;
}

void ImuSource::handleIsr() {
    // Minimal ISR: set flag, capture timestamp, count
    isrCount++;
    dataReadyFlag = true;
    isrTimestamp = micros();
}

bool ImuSource::begin() {
    // Initialize SPI for IMU communication
    IMU_SPI_PORT.begin();

    SERIAL_PORT.print(F("Initializing ICM-20948 IMU..."));
    bool imuInitialized = false;
    int attempts = 0;
    while (!imuInitialized && attempts < 10) {
        myICM.begin(IMU_CS_PIN, IMU_SPI_PORT, IMU_SPI_CLOCK_HZ);
        if (myICM.status != ICM_20948_Stat_Ok) {
            attempts++;
            SERIAL_PORT.print(F(" Retry "));
            SERIAL_PORT.print(attempts);
            SERIAL_PORT.print(F(": "));
            SERIAL_PORT.println(myICM.statusString());
            delay(500);
        } else {
            imuInitialized = true;
        }
    }

    if (!imuInitialized) {
        SERIAL_PORT.println(F(" IMU initialization FAILED after 10 attempts"));
        return false;
    }
    SERIAL_PORT.println(F(" OK"));

    // Configure IMU registers
    SERIAL_PORT.print(F("Configuring IMU..."));

    // Do software reset to ensure known state
    myICM.swReset();
    delay(100);
    if (myICM.status != ICM_20948_Stat_Ok) {
        SERIAL_PORT.print(F("\n  Software Reset FAILED: "));
        SERIAL_PORT.println(myICM.statusString());
    }

    // Wake sensor (sleep parameter should be false for normal operation)
    myICM.sleep(false);
    myICM.lowPower(false);

    // Set accel and gyro to continuous mode
    myICM.setSampleMode((ICM_20948_Internal_Acc | ICM_20948_Internal_Gyr), ICM_20948_Sample_Mode_Continuous);

    // Configure DLPF settings first
    ICM_20948_dlpcfg_t myDLPcfg;
    myDLPcfg.a = IMU_ACCEL_DLPF;
    myDLPcfg.g = IMU_GYRO_DLPF;
    myICM.setDLPFcfg((ICM_20948_Internal_Acc | ICM_20948_Internal_Gyr), myDLPcfg);

    // Set full scale ranges
    ICM_20948_fss_t myFSS;
    myFSS.a = IMU_ACCEL_FSR;
    myFSS.g = IMU_GYRO_FSR;
    myICM.setFullScale((ICM_20948_Internal_Acc | ICM_20948_Internal_Gyr), myFSS);

    // Enable DLPF AFTER setFullScale - this sets FCHOICE=1 which is required for SMPLRT_DIV to work
    myICM.enableDLPF(ICM_20948_Internal_Acc, true);
    myICM.enableDLPF(ICM_20948_Internal_Gyr, true);

    // Set sample rate (ODR) - Must be done AFTER enabling DLPF
    ICM_20948_smplrt_t mySmplrt;
    mySmplrt.a = IMU_SMPLRT_DIV;
    mySmplrt.g = IMU_SMPLRT_DIV;
    myICM.setSampleRate(ICM_20948_Internal_Acc, mySmplrt);
    myICM.setSampleRate(ICM_20948_Internal_Gyr, mySmplrt);

    SERIAL_PORT.print(F("Accel sample rate: "));
    SERIAL_PORT.print(1125.0 / (mySmplrt.a + 1), 1);
    SERIAL_PORT.println(F(" Hz"));
    SERIAL_PORT.print(F("Gyro sample rate: "));
    SERIAL_PORT.print(1125.0 / (mySmplrt.g + 1), 1);
    SERIAL_PORT.println(F(" Hz"));

    // Initialize magnetometer
    SERIAL_PORT.print(F("Initializing magnetometer..."));
    ICM_20948_Status_e magStatus = myICM.startupMagnetometer();
    if (magStatus != ICM_20948_Stat_Ok) {
        SERIAL_PORT.print(F(" WARNING: Magnetometer init failed: "));
        SERIAL_PORT.println(myICM.statusString());
        // Continue anyway - logging should still work for accel/gyro
    } else {
        SERIAL_PORT.println(F(" OK"));
    }

    // Configure interrupt behavior
    myICM.cfgIntActiveLow(IMU_INT_ACTIVE_LOW);
    myICM.cfgIntOpenDrain(IMU_INT_OPEN_DRAIN);
    myICM.cfgIntLatch(IMU_INT_LATCH);

    // Enable Data Ready interrupt
    myICM.intEnableRawDataReady(true);

    SERIAL_PORT.println(F(" complete!"));
    return true;
}

void ImuSource::attachInterrupt() {
    ::attachInterrupt(digitalPinToInterrupt(IMU_DRDY_PIN), drdy_isr_wrapper, IMU_DRDY_MODE);
}

void ImuSource::detachInterrupt() {
    ::detachInterrupt(digitalPinToInterrupt(IMU_DRDY_PIN));
}

bool ImuSource::hasPendingSample() const {
    return dataReadyFlag;
}

bool ImuSource::tryRead(ImuRecord& out) {
    uint32_t ts;

    // Atomically check+clear flag and capture timestamp
    noInterrupts();
    if (!dataReadyFlag) {
        interrupts();
        return false;
    }
    dataReadyFlag = false;
    ts = isrTimestamp;
    interrupts();

    // Read IMU data via SPI (in main loop, not ISR)
    ICM_20948_AGMT_t agmt = myICM.getAGMT();

    // Clear the interrupt in IMU hardware AFTER reading data
    myICM.clearInterrupts();

    // Construct IMU record
    out.seq = seqCounter++;
    out.ts_us = ts;

    // Copy accelerometer data
    out.ax = (int16_t)agmt.acc.axes.x;
    out.ay = (int16_t)agmt.acc.axes.y;
    out.az = (int16_t)agmt.acc.axes.z;

    // Copy gyroscope data
    out.gx = (int16_t)agmt.gyr.axes.x;
    out.gy = (int16_t)agmt.gyr.axes.y;
    out.gz = (int16_t)agmt.gyr.axes.z;

    // Copy temperature
    out.temp = (int16_t)agmt.tmp.val;

    // Copy magnetometer data
    out.mx = (int16_t)agmt.mag.axes.x;
    out.my = (int16_t)agmt.mag.axes.y;
    out.mz = (int16_t)agmt.mag.axes.z;

    samplesTaken++;
    return true;
}

void ImuSource::clearInterrupts() {
    myICM.clearInterrupts();
}

bool ImuSource::readDirect(ImuRecord& out) {
    // Read without checking flag (used for draining)
    ICM_20948_AGMT_t agmt = myICM.getAGMT();
    myICM.clearInterrupts();

    out.seq = seqCounter++;
    out.ts_us = micros();
    out.ax = (int16_t)agmt.acc.axes.x;
    out.ay = (int16_t)agmt.acc.axes.y;
    out.az = (int16_t)agmt.acc.axes.z;
    out.gx = (int16_t)agmt.gyr.axes.x;
    out.gy = (int16_t)agmt.gyr.axes.y;
    out.gz = (int16_t)agmt.gyr.axes.z;
    out.temp = (int16_t)agmt.tmp.val;
    out.mx = (int16_t)agmt.mag.axes.x;
    out.my = (int16_t)agmt.mag.axes.y;
    out.mz = (int16_t)agmt.mag.axes.z;

    return true;
}

uint32_t ImuSource::getIsrCount() const {
    return isrCount;
}

uint32_t ImuSource::getSamplesTaken() const {
    return samplesTaken;
}

uint32_t ImuSource::getSeqCounter() const {
    return seqCounter;
}
