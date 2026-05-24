/*
 * @Description:
 *      A library for the PCF85063 chip, based on the Arduino_DriveBus library.
 *      The core functionalities have been implemented; some features are not listed here.
 * @version: V1.0.0
 * @Author: LILYGO_L
 * @Date: 2023-11-16 15:42:22
 * @LastEditors: LILYGO_L
 * @LastEditTime: 2024-06-06 15:40:04
 * @License: GPL 3.0
 */

/*
// The following are enumerations related to PCF85063 operations (please use the parameters within these enumerations when controlling the PCF85063 chip):
class Arduino_IIC_RTC
{
public:
 enum Device_State
 {
   RTC_DEVICE_ON,  // Device function enabled
   RTC_DEVICE_OFF, // Device function disabled
 };
 enum Device
 {
   RTC_CLOCK_RTC,          // RTC (Real-Time Clock)
   RTC_CLOCK_OUTPUT_VALUE, // Clock output value

   RTC_INT_AIE, // Alarm Interrupt Enable (Scheduled Alarm)
   RTC_INT_TIE, // Timer Interrupt Enable (Timer Alarm)

   RTC_TIMER_FREQUENCY_VALUE, // Timer frequency value

   RTC_TIMER,           // Timer
   RTC_AE_SECOND_ALARM, // Second-based scheduled alarm
   RTC_AE_MINUTE_ALARM, // Minute-based scheduled alarm
   RTC_AE_HOUR_ALARM,   // Hour-based scheduled alarm
   RTC_AE_DAY_ALARM,    // Day-based scheduled alarm

   RTC_CLOCK_TIME_FORMAT, // RTC clock time format
 };
 enum Device_Mode
 {
   RTC_CLOCK_OUTPUT_36768, // Clock output: 36768 Hz
   RTC_CLOCK_OUTPUT_16384, // Clock output: 16384 Hz
   RTC_CLOCK_OUTPUT_8192,  // Clock output: 8192 Hz
   RTC_CLOCK_OUTPUT_4096,  // Clock output: 4096 Hz
   RTC_CLOCK_OUTPUT_2048,  // Clock output: 2048 Hz
   RTC_CLOCK_OUTPUT_1024,  // Clock output: 1024 Hz
   RTC_CLOCK_OUTPUT_1,     // Clock output: 1 Hz
   RTC_CLOCK_OUTPUT_OFF,   // Turn off clock output

   RTC_TIMER_FREQUENCY_4096, // Timer frequency 4096Hz
   RTC_TIMER_FREQUENCY_64,   // Timer frequency 64Hz
   RTC_TIMER_FREQUENCY_1,    // Timer frequency 1Hz
   RTC_TIMER_FREQUENCY_1_60, // Timer frequency 1/60Hz

   RTC_CLOCK_TIME_FORMAT_12, // 12-hour format
   RTC_CLOCK_TIME_FORMAT_24, // 24-hour format
 };

 enum Device_Value
 {
   RTC_SET_SECOND_DATA, // Set second data
   RTC_SET_MINUTE_DATA, // Set minute data
   RTC_SET_HOUR_DATA,   // Set hour data
   RTC_SET_DAY_DATA,    // Sets day data
   RTC_SET_MONTH_DATA,  // Sets month data
   RTC_SET_YEAR_DATA,   // Sets year data

   RTC_TIMER_N_VALUE, // Value of timer n

   RTC_TIMER_FLAG_AF, // Timer AF flag
   RTC_TIMER_FLAG_TF, // Timer TF flag

   RTC_SET_ALARM_SECOND_DATA, // Sets alarm seconds data
   RTC_SET_ALARM_MINUTE_DATA, // Sets alarm minutes data
   RTC_SET_ALARM_HOUR_DATA,   // Sets alarm hours data
   RTC_SET_ALARM_DAY_DATA,    // Sets alarm days data
 };

 enum Status_Information
 {

   RTC_WEEKDAYS_DATA,

 };

 enum Value_Information

 {
   RTC_SECONDS_DATA,
   RTC_MINUTES_DATA,
   RTC_HOURS_DATA,
   RTC_DAYS_DATA,
   RTC_MONTHS_DATA,
   RTC_YEARS_DATA,

   RTC_ALARM_FLAG_AF_INFORMATION, // Alarm AF Flag
   RTC_TIMER_FLAG_TF_INFORMATION, // Timer TF Flag
 };
};
*/

#pragma once

#include "../Arduino_IIC.h"

#define PCF85063_DEVICE_ADDRESS 0x51

#define PCF85063_RD_WR_CONTROL_STATUS_1 0x00
#define PCF85063_RD_WR_CONTROL_STATUS_2 0x01
#define PCF85063_RD_WR_SECONDS 0x04
#define PCF85063_RD_WR_MINUTES 0x05
#define PCF85063_RD_WR_HOURS 0x06
#define PCF85063_RD_WR_DAYS 0x07
#define PCF85063_RD_WR_WEEKDAY 0x08
#define PCF85063_RD_WR_MONTHS 0x09
#define PCF85063_RD_WR_YEARS 0x0A
#define PCF85063_RD_WR_SECOND_ALARM 0x0B
#define PCF85063_RD_WR_MINUTE_ALARM 0x0C
#define PCF85063_RD_WR_HOUR_ALARM 0x0D
#define PCF85063_RD_WR_DAY_ALARM 0x0E
#define PCF85063_RD_WR_WEEKDAY_ALARM 0x0F
#define PCF85063_RD_WR_TIMER_VALUE 0x10
#define PCF85063_RD_WR_TIMER_MODE 0x11

#define PCF85063_RD_DEVICE_ID 0x00 // Device ID Register

static const uint8_t PCF85063_Initialization_BufferOperations[] = {
    // BO_BEGIN_TRANSMISSION,
    // BO_END_TRANSMISSION,
    // BO_DELAY, 100
};

class Arduino_PCF85063 : public Arduino_IIC
{
public:
  Arduino_PCF85063(std::shared_ptr<Arduino_IIC_DriveBus> bus, uint8_t device_address,
                   int8_t rst = DRIVEBUS_DEFAULT_VALUE, int8_t iqr = DRIVEBUS_DEFAULT_VALUE,
                   void (*Interrupt_Function)() = nullptr);

  bool begin(int32_t speed = DRIVEBUS_DEFAULT_VALUE) override;
  int32_t IIC_Device_ID(void) override;
  int32_t IIC_Device_Reset(void) override;
  bool IIC_Write_Device_State(uint32_t device, uint8_t state) override;
  bool IIC_Write_Device_Value(uint32_t device, int64_t value) override;
  String IIC_Read_Device_State(uint32_t information) override;
  double IIC_Read_Device_Value(uint32_t information) override;

protected:
  bool IIC_Initialization(void) override;
};