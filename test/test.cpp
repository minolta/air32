#include <Arduino.h>
#include <unity.h>
#include <Wire.h>
#include "SHTSensor.h"
#include <LITTLEFS.h>
void testListDir()
{
    int open = LITTLEFS.begin(true);
    TEST_ASSERT_TRUE(open);

    File dir = LITTLEFS.open("/");
    File file = dir.openNextFile();
    while (file)
    {
        Serial.printf("File %s \n",file.name());
        file.close();
        file = dir.openNextFile();
    }

    Serial.println("End");
}
void testSht35()
{
    Wire.begin();
    SHTSensor sht;
    if (sht.init())
    {
        Serial.print("init(): success\n");
    }
    else
    {
        Serial.print("init(): failed\n");
    }
    sht.setAccuracy(SHTSensor::SHT_ACCURACY_MEDIUM); // only supported by SHT3x
}
void setup()
{
    Wire.begin();
    // NOTE!!! Wait for >2 secs
    // if board doesn't support software reset via Serial.DTR/RTS
    delay(2000);

    UNITY_BEGIN(); // IMPORTANT LINE!
    // RUN_TEST(testSht35);
    RUN_TEST(testListDir);
    // RUN_TEST(test_led_builtin_pin_number);
    // RUN_TEST(testI2c);
    // RUN_TEST(testOpenfile);
    // RUN_TEST(testWritefile);
    // RUN_TEST(testOpenDir);
    UNITY_END();
}

void loop()
{
}