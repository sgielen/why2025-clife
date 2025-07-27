#include <stdio.h>

#include <badgevms/device.h>

#define I2C_SCAN_ADDR_NUM                 (100)
#define MININUM_EXPECTED_I2C_DEVICE_COUNT 3

bool i2c_test() {
    printf("Starting i2c bus tests\n");
    i2c_bus_device_t *bus = (i2c_bus_device_t *)device_get("I2CBUS0");
    if (!bus) {
        printf("Failed to open I2C bus");
        return false;
    }

    i2c_scanresult_t addrs[I2C_SCAN_ADDR_NUM] = {0};
    uint8_t          device_count             = bus->_scan(bus, addrs, I2C_SCAN_ADDR_NUM);

    if (device_count < MININUM_EXPECTED_I2C_DEVICE_COUNT) {
        printf("Unexpected device count: expected %u, got %u\n", MININUM_EXPECTED_I2C_DEVICE_COUNT, device_count);
        return false;
    }

    printf("I2c bus tests success\n");
    return true;
}

int main(int argc, char *argv[]) {
    printf("Badge test\n");

    i2c_test();
}
