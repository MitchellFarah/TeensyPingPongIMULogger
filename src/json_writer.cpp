// json_writer.cpp - JSON metadata file generation implementation

#include "json_writer.h"
#include "config.h"
#include "types.h"
#include "imu_source.h"
#include "pingpong_buffer.h"
#include "SdFat.h"

// Helper to convert DLPF enum to string
static const char* accelDlpfToString(ICM_20948_ACCEL_CONFIG_DLPCFG_e dlpf) {
    switch (dlpf) {
        case acc_d246bw_n265bw: return "acc_d246bw_n265bw";
        case acc_d111bw4_n136bw: return "acc_d111bw4_n136bw";
        case acc_d50bw4_n68bw8: return "acc_d50bw4_n68bw8";
        case acc_d23bw9_n34bw4: return "acc_d23bw9_n34bw4";
        case acc_d11bw5_n17bw: return "acc_d11bw5_n17bw";
        case acc_d5bw7_n8bw3: return "acc_d5bw7_n8bw3";
        case acc_d473bw_n499bw: return "acc_d473bw_n499bw";
        default: return "unknown";
    }
}

static const char* gyroDlpfToString(ICM_20948_GYRO_CONFIG_1_DLPCFG_e dlpf) {
    switch (dlpf) {
        case gyr_d196bw6_n229bw8: return "gyr_d196bw6_n229bw8";
        case gyr_d151bw8_n187bw6: return "gyr_d151bw8_n187bw6";
        case gyr_d119bw5_n154bw3: return "gyr_d119bw5_n154bw3";
        case gyr_d51bw2_n73bw3: return "gyr_d51bw2_n73bw3";
        case gyr_d23bw9_n35bw9: return "gyr_d23bw9_n35bw9";
        case gyr_d11bw6_n17bw8: return "gyr_d11bw6_n17bw8";
        case gyr_d5bw7_n8bw9: return "gyr_d5bw7_n8bw9";
        case gyr_d361bw4_n376bw5: return "gyr_d361bw4_n376bw5";
        default: return "unknown";
    }
}

static const char* accelFsrToString(ICM_20948_ACCEL_CONFIG_FS_SEL_e fsr) {
    switch (fsr) {
        case gpm2: return "+/-2g";
        case gpm4: return "+/-4g";
        case gpm8: return "+/-8g";
        case gpm16: return "+/-16g";
        default: return "unknown";
    }
}

static const char* gyroFsrToString(ICM_20948_GYRO_CONFIG_1_FS_SEL_e fsr) {
    switch (fsr) {
        case dps250: return "+/-250dps";
        case dps500: return "+/-500dps";
        case dps1000: return "+/-1000dps";
        case dps2000: return "+/-2000dps";
        default: return "unknown";
    }
}

bool JsonWriter::write(const char* filename,
                       const char* binFilename,
                       const char* jsonFilename,
                       uint8_t fileNumber,
                       const ImuSource& imu,
                       const PingPongBuffer& buffer,
                       const Telemetry& telemetry,
                       uint32_t actualFileSize) {

    // Calculate derived values from actual config
    float odrHz = IMU_ODR_HZ;
    uint32_t samplesRecorded = telemetry.samplesRecorded;
    uint32_t dropCount = buffer.getDropCount();
    uint32_t isrCount = imu.getIsrCount();
    double durationSeconds = (samplesRecorded > 0) ? (double)samplesRecorded / odrHz : 0;
    double dropRate = (samplesRecorded + dropCount > 0) ?
        100.0 * dropCount / (samplesRecorded + dropCount) : 0.0;

    // Create JSON content in buffer
    char jsonBuffer[2048];
    int len = 0;

    // Header
    len += sprintf(jsonBuffer + len, "{\n");
    len += sprintf(jsonBuffer + len, "  \"header\": {\n");
    len += sprintf(jsonBuffer + len, "    \"version\": \"1.1\",\n");
    len += sprintf(jsonBuffer + len, "    \"created\": \"%lu\",\n", millis());
    len += sprintf(jsonBuffer + len, "    \"logger\": \"Teensy IMU Logger\"\n");
    len += sprintf(jsonBuffer + len, "  },\n");

    // Hardware
    len += sprintf(jsonBuffer + len, "  \"hardware\": {\n");
    len += sprintf(jsonBuffer + len, "    \"mcu\": \"Teensy 4.1\",\n");
    len += sprintf(jsonBuffer + len, "    \"imu_drdy_pin\": %d,\n", IMU_DRDY_PIN);
    len += sprintf(jsonBuffer + len, "    \"imu_cs_pin\": %d,\n", IMU_CS_PIN);
    len += sprintf(jsonBuffer + len, "    \"spi_clock_hz\": %lu\n", (uint32_t)IMU_SPI_CLOCK_HZ);
    len += sprintf(jsonBuffer + len, "  },\n");

    // IMU Configuration - Accelerometer (uses actual config values)
    len += sprintf(jsonBuffer + len, "  \"imu_config\": {\n");
    len += sprintf(jsonBuffer + len, "    \"accel\": {\n");
    len += sprintf(jsonBuffer + len, "      \"odr_hz\": %.2f,\n", odrHz);
    len += sprintf(jsonBuffer + len, "      \"sample_rate_divider\": %d,\n", IMU_SMPLRT_DIV);
    len += sprintf(jsonBuffer + len, "      \"full_scale_range\": \"%s\",\n", accelFsrToString(IMU_ACCEL_FSR));
    len += sprintf(jsonBuffer + len, "      \"dlpf\": \"%s\"\n", accelDlpfToString(IMU_ACCEL_DLPF));
    len += sprintf(jsonBuffer + len, "    },\n");

    // IMU Configuration - Gyroscope (uses actual config values)
    len += sprintf(jsonBuffer + len, "    \"gyro\": {\n");
    len += sprintf(jsonBuffer + len, "      \"odr_hz\": %.2f,\n", odrHz);
    len += sprintf(jsonBuffer + len, "      \"sample_rate_divider\": %d,\n", IMU_SMPLRT_DIV);
    len += sprintf(jsonBuffer + len, "      \"full_scale_range\": \"%s\",\n", gyroFsrToString(IMU_GYRO_FSR));
    len += sprintf(jsonBuffer + len, "      \"dlpf\": \"%s\"\n", gyroDlpfToString(IMU_GYRO_DLPF));
    len += sprintf(jsonBuffer + len, "    },\n");

    // IMU Configuration - Magnetometer
    len += sprintf(jsonBuffer + len, "    \"magnetometer\": {\n");
    len += sprintf(jsonBuffer + len, "      \"enabled\": true,\n");
    len += sprintf(jsonBuffer + len, "      \"odr_hz\": 100.0,\n");
    len += sprintf(jsonBuffer + len, "      \"full_scale_range\": \"±4900 µT\",\n");
    len += sprintf(jsonBuffer + len, "      \"lsb_per_ut\": %.4f\n", 1.0f/0.15f);
    len += sprintf(jsonBuffer + len, "    }\n");
    len += sprintf(jsonBuffer + len, "  },\n");

    // Buffer Configuration
    len += sprintf(jsonBuffer + len, "  \"buffer_config\": {\n");
    len += sprintf(jsonBuffer + len, "    \"chunk_bytes\": %d,\n", CHUNK_BYTES);
    len += sprintf(jsonBuffer + len, "    \"buffer_count\": 2,\n");
    len += sprintf(jsonBuffer + len, "    \"record_size_bytes\": %d,\n", sizeof(ImuRecord));
    len += sprintf(jsonBuffer + len, "    \"preallocate_bytes\": %llu\n", (unsigned long long)PREALLOC_BYTES);
    len += sprintf(jsonBuffer + len, "  },\n");

    // Performance Metrics
    len += sprintf(jsonBuffer + len, "  \"performance\": {\n");
    len += sprintf(jsonBuffer + len, "    \"isr_count\": %lu,\n", isrCount);
    len += sprintf(jsonBuffer + len, "    \"samples_recorded\": %lu,\n", samplesRecorded);
    len += sprintf(jsonBuffer + len, "    \"samples_dropped\": %lu,\n", dropCount);
    len += sprintf(jsonBuffer + len, "    \"drop_rate_percent\": %.2f,\n", dropRate);
    len += sprintf(jsonBuffer + len, "    \"max_write_time_us\": %lu,\n", telemetry.maxWriteTimeUs);
    len += sprintf(jsonBuffer + len, "    \"buffer_0_filled\": %lu,\n", buffer.getBufferFullCount(0));
    len += sprintf(jsonBuffer + len, "    \"buffer_1_filled\": %lu,\n", buffer.getBufferFullCount(1));
    len += sprintf(jsonBuffer + len, "    \"buffer_swaps\": %lu\n", buffer.getSwapCount());
    len += sprintf(jsonBuffer + len, "  },\n");

    // File Metadata
    len += sprintf(jsonBuffer + len, "  \"file_metadata\": {\n");
    len += sprintf(jsonBuffer + len, "    \"bin_filename\": \"%s\",\n", binFilename);
    len += sprintf(jsonBuffer + len, "    \"json_filename\": \"%s\",\n", jsonFilename);
    len += sprintf(jsonBuffer + len, "    \"actual_size_bytes\": %lu,\n", actualFileSize);
    len += sprintf(jsonBuffer + len, "    \"sample_count\": %lu,\n", samplesRecorded);
    len += sprintf(jsonBuffer + len, "    \"duration_seconds\": %.2f\n", durationSeconds);
    len += sprintf(jsonBuffer + len, "  }\n");
    len += sprintf(jsonBuffer + len, "}\n");

    // Write JSON file
    FsFile jsonFile;
    if (!jsonFile.open(filename, O_CREAT | O_TRUNC | O_WRONLY)) {
        SERIAL_PORT.print(F("JSON: Failed to open "));
        SERIAL_PORT.println(filename);
        return false;
    }

    size_t written = jsonFile.write(jsonBuffer, len);
    jsonFile.close();

    if (written == static_cast<size_t>(len)) {
        SERIAL_PORT.print(F("JSON: Successfully wrote "));
        SERIAL_PORT.print(len);
        SERIAL_PORT.print(F(" bytes to "));
        SERIAL_PORT.println(filename);
        return true;
    } else {
        SERIAL_PORT.print(F("JSON: Write failed, "));
        SERIAL_PORT.print(written);
        SERIAL_PORT.print(F(" bytes written, expected "));
        SERIAL_PORT.println(len);
        return false;
    }
}
