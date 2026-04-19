
#include <Arduino.h>

#define VERSION "GLUONiCA_WS_1.0 (" __DATE__ ")"

#if __has_include("credentials.h")
  #include "credentials.h"
#else
    #define SERIAL_NUMBER "PUBLIC"
    #define NAME "GLUONiCA WS"
    #define NETWORK_NAME "GLUONiCA_WS"
    #define API_OPENWEATHERMAP "апи-ключ" // пока или всегда в разработке 
    #define WIFI_SSID "MY_WIFI_NAME"
    #define WIFI_PASSS "PASSWORD"
    #define RESERV_WIFI_SSID "RESERV_WIFI_NAME"
    #define RESERV_WIFI_PASSS "PASSWORD"
    #define AP_PASSS "gluonpass" // пароль точки доступа
    #define MQTT_PASS "pass_mqtt"
    #define MQTT_USER "login_mqtt"
    #define WEATHER_CITY "МОСКВА"
    #define BUTTON_PIN 25
    #define SERIAL_SPEED 115200
#endif

#define DEFAULT_DEBUG Serial  // сериал порт для дебага по умолчанию

#include <WiFi.h>
// если вафля подключена
#define WIFI WiFi.status() == WL_CONNECTED
#define IF_WIFI if (WIFI)
// срочно тикаем, в скобках (что возращаем в ретран)
//- true
//- false
//- ничего
#define EXIT(...)           \
    if (exit_flag) {        \
        exit_flag = false;  \
        return __VA_ARGS__; \
    }
//    \/

#include <ArduinoJson.h>  // https://github.com/bblanchon/ArduinoJson
#include <ArduinoOTA.h>
#include <SPI.h>   // Built-in
#include <WiFi.h>  // Built-in


#include <GxEPD2_3C.h>
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>

#include "big_gluon_font_20.h"
#include "big_gluon_font_42.h"
#include "big_gluon_font_52.h"
#include "bitmaps.h"
#include "gluon_font.h"
#include "lang.h"
#include "medium_gluon_font.h"

#define SCREEN_WIDTH 400.0
#define SCREEN_HEIGHT 300.0

enum alignment { LEFT,
                 RIGHT,
                 CENTER };


#define CS_PIN (5)
#define BUSY_PIN (4)
#define RES_PIN (16)
#define DC_PIN (17)
GxEPD2_3C<GxEPD2_420c_GDEY042Z98, GxEPD2_420c_GDEY042Z98::HEIGHT> display(GxEPD2_420c_GDEY042Z98(/*CS=5*/ CS_PIN, /*DC=*/DC_PIN, /*RES=*/RES_PIN, /*BUSY=*/BUSY_PIN));  // 400x300, SSD1683
U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;  // Select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall

#include <GyverBME280.h>
GyverBME280 bme;
#include "SHTSensor.h"
SHTSensor sht;
#include <AHTxx.h>
AHTxx aht(AHTXX_ADDRESS_X38, AHT2x_SENSOR);  // sensor address, sensor type
#include "SparkFun_SCD4x_Arduino_Library.h"  //Click here to get the library: http://librarymanager/All#SparkFun_SCD4x
SCD4x co2;

// #include <GyverINA.h>
//// Сопротивление шунта, Макс. ожидаемый ток, I2c адрес;
// INA219 ina(0.1f, 1.8f, 0x40);
// #define INTERVAL_CURRENT_REACT 10000  // реагировать на изменение тока не чаще раз в 10 сек
// #define INTERVAL_CURRENT 1000         //  измерение тока раз в 1 сек

//=========================================================================================//
#define IF_WIFI (WiFi.status() == WL_CONNECTED)  // будет чутка покороче запись

struct __attribute__((packed)) SETTINGS {
    uint8_t mode = 0;  // на случай смены циферблата
    uint8_t previusMode = 0;
    uint8_t color_theme = 6;

    // WIFI
    char name[25] = NAME;
    char ssid[32] = WIFI_SSID;                     //  имя вайфай сети
    char password[20] = WIFI_PASSS;                //  пароль
    char ssid_reserv[32] = RESERV_WIFI_SSID;       //  имя вайфай сети
    char password_reserv[20] = RESERV_WIFI_PASSS;  //  пароль
    char ssid_ap[25] = NETWORK_NAME;
    char pass_ap[20] = AP_PASSS;
    uint8_t ap = 2;            // APMode: wifi.  NEVER = 0,  AP_NO_PASS = 1,  AP_PASS = 2,  ALWAYS_AP_NO_PASS = 3,  ALWAYS_AP_PASS = 4
    bool switchWifi = false;   // поменять местами // true -о сновной становится резервным
    uint8_t max_wf_conn = 20;  // максимальное кол-во попыток подключения к вайфай

    uint16_t wifi_timeout_off = 0;  // минут до выключения, 0 - не выключать

    // DEBUG
    // куда дебажим
    //---
    //- 0 - Debug::DBG_NO - никуда
    //- 1 - Debug::DBG_SERIAL - в сериал порт
    //- 2 - Debug::DBG_LOGGER - в логгер
    //- 3 - Debug::DBG_LCD -- на экран // тут неиспользуется
    uint8_t debug = 1;
    // уровень дебага
    //---
    //- 0 - DBG_LVL_ALL
    //- 1 - DBG_LVL_INFO
    //- 2 - DBG_LVL_WARNING
    //- 3 - DBG_LVL_ERROR
    //---
    // Debug::DBG_LVL_INFO
    uint8_t debug_lvl = 0;
    bool color_debug = true;  // метки уровня дебага
    bool show_logo_screen = true;
    bool sleep = false;
    uint16_t sleep_duration = 10;             // минут
    uint16_t sleep_duration_quiet_mode = 60;  // минут
    bool quiet_mode = true;
    uint32_t start_mute = 79200;  // начало тихого режима секнуд
    uint32_t end_mute = 34320;    // конец тихого режима секунд

    // MQTT
    bool mqtt = false;
    bool send_mqtt = false;
    uint8_t period_mqtt = 5;                 // период отправки данных / минут
    uint8_t count_mqtt = 0;                  // попыток подключения
    char mqtt_server[21] = "192.168.1.128";  //   IP адрес MQTT сервера
    char mqtt_user[16] = MQTT_USER;             //  логин  MQTT сервера
    char mqtt_pass[16] = MQTT_PASS;         //  пароль MQTT сервера
    unsigned int mqtt_port = 1883;           // local port to listen for UDP packets
    char mqtt_client_name[50] = NETWORK_NAME "_" SERIAL_NUMBER;
    char mqtt_topic_sub[61] = "gluonica/devices/GLUONiCA_MiNi_001/";
    char mqtt_topic_pub[61] = "gluonica/devices/" NETWORK_NAME "_" SERIAL_NUMBER "/";
    char mqtt_topic_msg[61] = NETWORK_NAME "_" SERIAL_NUMBER " - connected";

    // TIME
    uint8_t internet_time = 0;  // 0 - время из погоды, 1 - из интернета, 2 - rtc
    char ntp_host[30] = "pool.ntp.org";
    int timeZone = 3;
    char Timezone[30] = "MSK-3";  // Choose your time zone from: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv

    // WEATHER
    uint8_t weather = 2;  // погода - 0 нет | 1 - openweathermap | 2 -  open-meteo | 3 - mqtt

    float latitude = 56.13655;
    float longitude = 40.39658;

    // http://api.openweathermap.org/data/2.5/forecast?q=Melksham,UK&APPID=your_OWM_API_key&mode=json&units=metric&cnt=40
    // http://api.openweathermap.org/data/2.5/weather?q=Melksham,UK&APPID=your_OWM_API_key&mode=json&units=metric&cnt=1
    char server[30] = "api.openweathermap.org";
    char owm_api_key[36] = API_OPENWEATHERMAP;  // зарегится в openweathermap.org и скопировать апи ключ
    char city_owm[31] = "Vladimir";             // город для погоды по городу // список городов https://bulk.openweathermap.org/sample/?form=MG0AV3
    unsigned int localPort = 8888;              // local port to listen for UDP packets
    char city_om[61] = WEATHER_CITY;              // город для поиска координат и отображения на экране
    char Country[10] = "RU";                    // Your _ISO-3166-1_two-letter_country_code country code, on OWM find your nearest city and the country code is displayed
                                                // https://en.wikipedia.org/wiki/List_of_ISO_3166_country_codes
    char last_city_owm[5][61] = {
        "РИУ-БРАНКУ",      // Бразилия
        "ЛОНГЙИР",         // Самый северный город мира Норвегия
        "НУУК",            // Гренландия
        "ПУЭРТО-НАТАЛЕС",  // Чили
        "ВАИЛУКУ"          // Гавайи
    };

    int8_t hemisphere = 0;  // 0 - north, 1 -south

    int8_t source_temp = 3;   // 0 - BME/BMP, 1 - SHT, 2 - MQTT, 3 - SCD40, 4 - AHT
    int8_t source_hum = 3;    // 0 - BME/BMP, 1 - SHT, 2 - MQTT, 3 - SCD40, 4 - AHT
    int8_t source_press = 2;  // 0 - BME/BMP, 1 - MQTT, 2 - WEATHER
    int8_t source_co2 = 1;    // 0 - MQTT, 1 - SCD40
    int8_t source_pm = 0;     // 0 - MQTT, 1 - PLANTOWER

    // SENSORS
    int8_t temp_calibration_coeff = 0;  // калибровочный коэффициент
    int8_t hum_calibration_coeff = 0;   // калибровочный коэффициент
    int8_t co2_calibration_coeff = 0;   // калибровочный коэффициент

    bool scd40_snsr = true;
    uint8_t ADDR_SCD40 = 0x62;

    // sht
    bool sht_snsr = false;
    uint8_t ADDR_SHT = 0x44;
    // aht
    bool aht_snsr = false;
    uint8_t ADDR_AHT = 0x38;
    // BME bmp
    bool bmep_snsr = false;
    uint8_t ADDR_BME_P = 0x76;  // uint8_t ADDR_BMP  = 0x76;//ADDR_BMP
    // датчик тока ina219
    // bool ina_snsr = false;
    // uint8_t ADDR_INA = 0x40;  //
    // rtc  часы
    bool rtc_snsr = false;
    uint8_t ADDR_RTC = 0x68;  //
    //

    bool welcome_message = true;

    uint8_t alert_cpu_temp = 70;  // макс температура процессора

    bool extra_debug = false;
    bool mqtt_send_after_weather = true;

    bool show_ip_in_logo_screen = true;

} settings;
#include <FileData.h>
#include <LittleFS.h>
// #define TABLE_USE_FOLD true
// #include <TableFileStatic.h>
// #define MAX_ROWS_TABLE 2016  // максимальноое количество строк в таблице
//  база данных для хранения настроек

FileData base(&LittleFS, "/settings.dat", 'B', &settings, sizeof(settings));
// TableFileStatic snsr(&LittleFS, "/snsr1.tbl", MAX_ROWS_TABLE);
#define USE_MYFILE
// #define USE_MYFONT
#define SETT_NO_DB
#include <SettingsGyver.h>
SettingsGyver sett(NAME);

#define MAX_LENGHT_LOG 6000
sets::Logger logger(MAX_LENGHT_LOG);

#include <GTimer.h>
#include <GyverDS3231.h>
#include <GyverNTP.h>
#include <Stamp.h>
GTimer<millis> Timer_print(60 * 1000, true, GTMode::Interval);
GyverDS3231 rtc;
StampKeeper stamp;
uint8_t hour;     // час          -- обновить - вызвать updt_time()
uint8_t minute;   // минута       -- обновить - вызвать updt_time()
uint8_t second;   // секунда      -- обновить - вызвать updt_time()
uint8_t day;      // день         -- обновить - вызвать updt_time()
uint8_t month;    // месяц        -- обновить - вызвать updt_time()
uint16_t year;    // год          -- обновить - вызвать updt_time()
uint8_t weekDay;  // день недели  -- обновить - вызвать updt_time()

uint32_t unix;  // секунд
// записать время в переменные
// беррет время из ntp
// если год из ntp 1970 возьмет время напрямую из часов

boolean LargeIcon = true, SmallIcon = false;
#define Large 15            // For icon drawing, needs to be odd number for best effect
#define Small 5             // For icon drawing, needs to be odd number for best effect
String time_str, date_str;  // strings to hold time and received weather data
int wifi_signal, CurrentHour = 0, CurrentMin = 0, CurrentSec = 0;
long StartTime = 0;
#define NORTH 0  // north
#define SOUTH 1  // south
// ################ PROGRAM VARIABLES and OBJECTS ################
const int max_readings = 24;  // 5 * 24;
const int max_dayly_readings = 7;

#define autoscale_on true
#define autoscale_off false
#define barchart_on true
#define barchart_off false

float hourly_temperature_readings[max_readings] = {0};
float hourly_rain_readings[max_readings] = {0};
// float hourly_snow_readings[max_readings] = {0};
// float hourly_pressure_readings[max_readings] = {0};

int16_t daily_max_temperature_readings[max_dayly_readings] = {0};
int16_t daily_min_temperature_readings[max_dayly_readings] = {0};
float daily_rain_readings[max_dayly_readings] = {0};
// float daily_snow_readings[max_dayly_readings] = {0};

int WakeupTime = 0;  // Don't wakeup until after 07:00 to save battery power
int SleepTime = 24;  // Sleep after (23+1) 00:00 to save battery power

void count();
extern "C" uint8_t temprature_sens_read();

;
void BeginSleep();
void updt_time();
void appendDebugToLCD(const __FlashStringHelper* msg, bool new_line = false);
void appendDebugToLCD(const char* msg, bool new_line = false);
void appendDebugToLCD(int value, bool new_line = false);
void appendDebugToLCD(unsigned int value, bool new_line = false);
void appendDebugToLCD(long value, bool new_line = false);
void appendDebugToLCD(unsigned long value, bool new_line = false);
void appendDebugToLCD(float value, bool new_line = false);
void appendDebugToLCD(const String& msg, bool new_line = false);
void appendDebugToLCD(const su::Text& msg, bool new_line = false);
void listFiles(const char* dirname, uint8_t level = 0);
bool sendMqttData();
bool if_button();
// bool mqtt_init();

// cимволы для цветного вывода
enum SerialSymbol {
    SIMBOL_NONE = 0x00,
    SIMBOL_INFO = 0x01,
    SIMBOL_WAR = 0x02,
    SIMBOL_ERR = 0x03,
    SIMBOL_CUSTOM = 0x04
};
class Debug {
   private:
    HardwareSerial& _serial;  // Ссылка на текущий Serial

   public:
    enum DBG_TYPE {
        DBG_NO = 0,
        DBG_SERIAL = 1,
        DBG_LOGGER = 2,
        DBG_LCD = 3
    };
    enum DBG_LVL {
        DBG_LVL_ALL = 0,      // выводится все что отправляется
        DBG_LVL_INFO = 1,     // выводится только то что отмечено символом (инфо, варнинг, еррор)
        DBG_LVL_WARNING = 2,  // выводятся только варнинги и ошибки
        DBG_LVL_ERROR = 3     // выводятся только ошибки

    };
    // Конструктор
    Debug(unsigned long baud, HardwareSerial& serial = DEFAULT_DEBUG) : _serial(serial) {
        _serial.begin(baud);
    }

    // Установка другого Serial порта
    bool setSerial(HardwareSerial& serial, unsigned long baud) {
        _serial = serial;
        _serial.begin(baud);
        return true;
        // добавить ожидание и проверку инициализации порта
    }

    // Простой вывод в сериал
    template <typename T>
    void println(const T& msg) {
        if (settings.debug_lvl != DBG_LVL_ALL) return;
        if (settings.debug == DBG_SERIAL) {
            _serial.println(msg);
        } else if (settings.debug == DBG_LOGGER) {
            logger.println(msg);
            // sett.reload();
        } else if (settings.debug == DBG_LCD) {
            appendDebugToLCD(msg, true);
        }
    }
    template <typename T>
    void println(const T& msg, int16_t format) {
        if (settings.debug_lvl != DBG_LVL_ALL) return;
        if (settings.debug == DBG_SERIAL) {
            _serial.println(msg, format);
        } else if (settings.debug == DBG_LOGGER) {
            logger.println(msg, format);
            // sett.reload();
        } else if (settings.debug == DBG_LCD) {
            appendDebugToLCD(msg, true);
        }
    }
    void println(const __FlashStringHelper* msg) {
        if (settings.debug_lvl != DBG_LVL_ALL) return;
        if (settings.debug == DBG_SERIAL) {
            _serial.println(msg);
        } else if (settings.debug == DBG_LOGGER) {
            logger.println(msg);
            // sett.reload();
        } else if (settings.debug == DBG_LCD) {
            appendDebugToLCD(msg, true);
        }
    }
    template <typename T>
    void print(const T& msg) {
        if (settings.debug_lvl != DBG_LVL_ALL) return;
        if (settings.debug == DBG_SERIAL) {
            _serial.print(msg);
        } else if (settings.debug == DBG_LOGGER) {
            logger.print(msg);
            // //sett.reload();
        } else if (settings.debug == DBG_LCD) {
            appendDebugToLCD(msg);
        }
    }
    template <typename T>
    void print(const T& msg, int16_t format) {
        if (settings.debug_lvl != DBG_LVL_ALL) return;
        if (settings.debug == DBG_SERIAL) {
            _serial.print(msg, format);
        } else if (settings.debug == DBG_LOGGER) {
            logger.print(msg, format);
            // //sett.reload();
        } else if (settings.debug == DBG_LCD) {
            appendDebugToLCD(msg);
        }
    }
    void print(const __FlashStringHelper* msg) {
        if (settings.debug_lvl != DBG_LVL_ALL) return;
        if (settings.debug == DBG_SERIAL) {
            _serial.print(msg);
        } else if (settings.debug == DBG_LOGGER) {
            logger.print(msg);
            // //sett.reload();
        } else if (settings.debug == DBG_LCD) {
            appendDebugToLCD(msg);
        }
    }

    // Вывод с текстом и значением
    void println(const char* text, int16_t value) {
        if (settings.debug_lvl != DBG_LVL_ALL) return;
        if (settings.debug == DBG_SERIAL) {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "%s%d", text, value);
            _serial.println(buffer);
        } else if (settings.debug == DBG_LOGGER) {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "%s%d", text, value);
            logger.println(buffer);
            // sett.reload();
        } else if (settings.debug == DBG_LCD) {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "%s%d", text, value);
            appendDebugToLCD(buffer, true);
        }
    }
    template <typename T>
    void println(const char* text, const T& value, int16_t format) {
        if (settings.debug_lvl != DBG_LVL_ALL) return;
        if (settings.debug == DBG_SERIAL) {
            char buffer[64];
            if (format == HEX) {
                snprintf(buffer, sizeof(buffer), "%s%lX", text, (unsigned long)value);
            } else if (format == BIN) {
                char bin[33];
                itoa((unsigned long)value, bin, 2);
                snprintf(buffer, sizeof(buffer), "%s%s", text, bin);
            } else {
                snprintf(buffer, sizeof(buffer), "%s%ld", text, (long)value);
            }
            _serial.println(buffer);
        } else if (settings.debug == DBG_LOGGER) {
            char buffer[64];
            if (format == HEX) {
                snprintf(buffer, sizeof(buffer), "%s%lX", text, (unsigned long)value);
            } else if (format == BIN) {
                char bin[33];
                itoa((unsigned long)value, bin, 2);
                snprintf(buffer, sizeof(buffer), "%s%s", text, bin);
            } else {
                snprintf(buffer, sizeof(buffer), "%s%ld", text, (long)value);
            }
            logger.println(buffer);
            // sett.reload();
        } else if (settings.debug == DBG_LCD) {
            char buffer[64];
            if (format == HEX) {
                snprintf(buffer, sizeof(buffer), "%s%lX", text, (unsigned long)value);
            } else if (format == BIN) {
                char bin[33];
                itoa((unsigned long)value, bin, 2);
                snprintf(buffer, sizeof(buffer), "%s%s", text, bin);
            } else {
                snprintf(buffer, sizeof(buffer), "%s%ld", text, (long)value);
            }
            appendDebugToLCD(buffer, true);
        }
    }
    void println(const __FlashStringHelper* text, int16_t value) {
        if (settings.debug_lvl != DBG_LVL_ALL) return;
        if (settings.debug == DBG_SERIAL) {
            char buffer[64];
            snprintf_P(buffer, sizeof(buffer), PSTR("%S%d"), text, value);
            _serial.println(buffer);
        } else if (settings.debug == DBG_LOGGER) {
            char buffer[64];
            snprintf_P(buffer, sizeof(buffer), PSTR("%S%d"), text, value);
            logger.println(buffer);
            // sett.reload();
        } else if (settings.debug == DBG_LCD) {
            char buffer[64];
            snprintf_P(buffer, sizeof(buffer), PSTR("%S%d"), text, value);

            appendDebugToLCD(buffer, true);
        }
    }
    template <typename T>
    void println(const __FlashStringHelper* text, const T& value, int16_t format) {
        if (settings.debug_lvl != DBG_LVL_ALL) return;
        if (settings.debug == DBG_SERIAL) {
            char buffer[64];
            if (format == HEX) {
                snprintf_P(buffer, sizeof(buffer), PSTR("%S%lX"), text, (unsigned long)value);
            } else if (format == BIN) {
                char bin[33];
                itoa((unsigned long)value, bin, 2);
                snprintf_P(buffer, sizeof(buffer), PSTR("%S%s"), text, bin);
            } else {
                snprintf_P(buffer, sizeof(buffer), PSTR("%S%ld"), text, (long)value);
            }
            _serial.println(buffer);
        } else if (settings.debug == DBG_LOGGER) {
            char buffer[64];
            if (format == HEX) {
                snprintf_P(buffer, sizeof(buffer), PSTR("%S%lX"), text, (unsigned long)value);
            } else if (format == BIN) {
                char bin[33];
                itoa((unsigned long)value, bin, 2);
                snprintf_P(buffer, sizeof(buffer), PSTR("%S%s"), text, bin);
            } else {
                snprintf_P(buffer, sizeof(buffer), PSTR("%S%ld"), text, (long)value);
            }
            logger.println(buffer);
            // sett.reload();
        } else if (settings.debug == DBG_LCD) {
            char buffer[64];
            if (format == HEX) {
                snprintf_P(buffer, sizeof(buffer), PSTR("%S%lX"), text, (unsigned long)value);
            } else if (format == BIN) {
                char bin[33];
                itoa((unsigned long)value, bin, 2);
                snprintf_P(buffer, sizeof(buffer), PSTR("%S%s"), text, bin);
            } else {
                snprintf_P(buffer, sizeof(buffer), PSTR("%S%ld"), text, (long)value);
            }
            appendDebugToLCD(buffer, true);
        }
    }
    void println(const char* text1, const char* text2) {
        if (settings.debug_lvl != DBG_LVL_ALL) return;
        if (settings.debug == DBG_SERIAL) {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "%s%s", text1, text2);
            _serial.println(buffer);
        } else if (settings.debug == DBG_LOGGER) {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "%s%s", text1, text2);
            logger.println(buffer);
            // sett.reload();
        } else if (settings.debug == DBG_LCD) {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "%s%s", text1, text2);
            appendDebugToLCD(buffer, true);
        }
    }
    void println(const __FlashStringHelper* text1, const __FlashStringHelper* text2) {
        if (settings.debug_lvl != DBG_LVL_ALL) return;
        if (settings.debug == DBG_SERIAL) {
            char buffer[64];
            snprintf_P(buffer, sizeof(buffer), PSTR("%S%S"), text1, text2);
            _serial.println(buffer);
        } else if (settings.debug == DBG_LOGGER) {
            char buffer[64];
            snprintf_P(buffer, sizeof(buffer), PSTR("%S%S"), text1, text2);
            logger.println(buffer);
            // sett.reload();
        } else if (settings.debug == DBG_LCD) {
            char buffer[64];
            snprintf_P(buffer, sizeof(buffer), PSTR("%S%S"), text1, text2);
            appendDebugToLCD(buffer, true);
        }
    }
    void println(const char* text1, const char* text2, const char* text3) {
        if (settings.debug_lvl != DBG_LVL_ALL) return;
        if (settings.debug == DBG_SERIAL) {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "%s%s%s", text1, text2, text3);
            _serial.println(buffer);
        } else if (settings.debug == DBG_LOGGER) {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "%s%s%s", text1, text2, text3);
            logger.println(buffer);
            // sett.reload();
        } else if (settings.debug == DBG_LCD) {
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "%s%s%s", text1, text2, text3);
            appendDebugToLCD(buffer, true);
        }
    }
    void println(const __FlashStringHelper* text1, const __FlashStringHelper* text2, const __FlashStringHelper* text3) {
        if (settings.debug_lvl != DBG_LVL_ALL) return;
        if (settings.debug == DBG_SERIAL) {
            char buffer[64];
            snprintf_P(buffer, sizeof(buffer), PSTR("%S%S%S"), text1, text2, text3);
            _serial.println(buffer);
        } else if (settings.debug == DBG_LOGGER) {
            char buffer[64];
            snprintf_P(buffer, sizeof(buffer), PSTR("%S%S%S"), text1, text2, text3);
            logger.println(buffer);
            // sett.reload();
        } else if (settings.debug == DBG_LCD) {
            char buffer[64];
            snprintf_P(buffer, sizeof(buffer), PSTR("%S%S%S"), text1, text2, text3);
            appendDebugToLCD(buffer, true);
        }
    }

    // ==============================================================
    // Вывод с символом + сообщение
    template <typename T>
    void println(SerialSymbol symbol, const T& msg) {
        // проверяем уровни дебага
        if (settings.debug_lvl == DBG_LVL_ALL)
            ;
        else if (settings.debug_lvl == DBG_LVL_INFO) {
            if (symbol < SIMBOL_INFO) return;
        } else if (settings.debug_lvl == DBG_LVL_WARNING) {
            if (symbol < SIMBOL_WAR || symbol == SIMBOL_CUSTOM) return;
        } else if (settings.debug_lvl == DBG_LVL_ERROR) {
            if (symbol < SIMBOL_ERR || symbol == SIMBOL_CUSTOM) return;
        }

        if (settings.debug == DBG_SERIAL) {
            if (settings.color_debug) {
                _serial.write((uint8_t)symbol);
            }
            _serial.println(msg);
        } else if (settings.debug == DBG_LOGGER) {
            if (settings.color_debug) {
                if (symbol == SIMBOL_INFO && settings.color_debug)
                    logger.print("info:");
                else if (symbol == SIMBOL_WAR && settings.color_debug)
                    logger.print("warn:");
                else if (symbol == SIMBOL_ERR && settings.color_debug) {
                    logger.print("err:");
                    // updt_time();
                    String _time = String(hour) + ":" + (minute > 9 ? String(minute) : "0" + String(minute));
                    logger.print("[" + _time + "] ");
                }
            }
            logger.println(msg);
            if (settings.color_debug && (symbol == SIMBOL_ERR || symbol == SIMBOL_WAR)) sett.reload();
            // sett.reload();
        } else if (settings.debug == DBG_LCD) {
            appendDebugToLCD(msg, true);
        }
    }
    template <typename T>
    void println(SerialSymbol symbol, const T& msg, int format) {
        // проверяем уровни дебага
        if (settings.debug_lvl == DBG_LVL_ALL)
            ;
        else if (settings.debug_lvl == DBG_LVL_INFO) {
            if (symbol < SIMBOL_INFO) return;
        } else if (settings.debug_lvl == DBG_LVL_WARNING) {
            if (symbol < SIMBOL_WAR || symbol == SIMBOL_CUSTOM) return;
        } else if (settings.debug_lvl == DBG_LVL_ERROR) {
            if (symbol < SIMBOL_ERR || symbol == SIMBOL_CUSTOM) return;
        }

        if (settings.debug == DBG_SERIAL) {
            if (settings.color_debug) {
                _serial.write((uint8_t)symbol);
            }
            _serial.println(msg, format);
        } else if (settings.debug == DBG_LOGGER) {
            if (symbol == SIMBOL_INFO && settings.color_debug)
                logger.print("info:");
            else if (symbol == SIMBOL_WAR && settings.color_debug)
                logger.print("warn:");
            else if (symbol == SIMBOL_ERR && settings.color_debug) {
                logger.print("err:");
                // updt_time();
                String _time = String(hour) + ":" + (minute > 9 ? String(minute) : "0" + String(minute));
                logger.print("[" + _time + "] ");
            }
            logger.println(msg, format);
            if (settings.color_debug && (symbol == SIMBOL_ERR || symbol == SIMBOL_WAR)) sett.reload();
            // sett.reload();
        } else if (settings.debug == DBG_LCD) {
            appendDebugToLCD(msg, true);
        }
    }
    void println(SerialSymbol symbol, const __FlashStringHelper* msg) {
        // проверяем уровни дебага
        if (settings.debug_lvl == DBG_LVL_ALL)
            ;
        else if (settings.debug_lvl == DBG_LVL_INFO) {
            if (symbol < SIMBOL_INFO) return;
        } else if (settings.debug_lvl == DBG_LVL_WARNING) {
            if (symbol < SIMBOL_WAR || symbol == SIMBOL_CUSTOM) return;
        } else if (settings.debug_lvl == DBG_LVL_ERROR) {
            if (symbol < SIMBOL_ERR || symbol == SIMBOL_CUSTOM) return;
        }

        if (settings.debug == DBG_SERIAL) {
            if (settings.color_debug) {
                _serial.write((uint8_t)symbol);
            }
            _serial.println(msg);
        } else if (settings.debug == DBG_LOGGER) {
            if (symbol == SIMBOL_INFO && settings.color_debug)
                logger.print("info:");
            else if (symbol == SIMBOL_WAR && settings.color_debug)
                logger.print("warn:");
            else if (symbol == SIMBOL_ERR && settings.color_debug) {
                logger.print("err:");
                // updt_time();
                String _time = String(hour) + ":" + (minute > 9 ? String(minute) : "0" + String(minute));
                logger.print("[" + _time + "] ");
            }
            logger.println(msg);
            if (settings.color_debug && (symbol == SIMBOL_ERR || symbol == SIMBOL_WAR)) sett.reload();
            // sett.reload();
        } else if (settings.debug == DBG_LCD) {
            appendDebugToLCD(msg, true);
        }
    }
    // Вывод с символом + сообщение + сообщение
    template <typename T1, typename T2>
    void println(SerialSymbol symbol, const T1& msg1, const T2& msg2) {
        // проверяем уровни дебага
        if (settings.debug_lvl == DBG_LVL_ALL)
            ;
        else if (settings.debug_lvl == DBG_LVL_INFO) {
            if (symbol < SIMBOL_INFO) return;
        } else if (settings.debug_lvl == DBG_LVL_WARNING) {
            if (symbol < SIMBOL_WAR || symbol == SIMBOL_CUSTOM) return;
        } else if (settings.debug_lvl == DBG_LVL_ERROR) {
            if (symbol < SIMBOL_ERR || symbol == SIMBOL_CUSTOM) return;
        }

        if (settings.debug == DBG_SERIAL) {
            if (settings.color_debug) {
                _serial.write((uint8_t)symbol);
            }
            _serial.print(msg1);
            _serial.println(msg2);
        } else if (settings.debug == DBG_LOGGER) {
            if (symbol == SIMBOL_INFO && settings.color_debug)
                logger.print("info:");
            else if (symbol == SIMBOL_WAR && settings.color_debug)
                logger.print("warn:");
            else if (symbol == SIMBOL_ERR && settings.color_debug) {
                logger.print("err:");
                // updt_time();
                String _time = String(hour) + ":" + (minute > 9 ? String(minute) : "0" + String(minute));
                logger.print("[" + _time + "] ");
            }
            logger.print(msg1);
            logger.println(msg2);
            if (settings.color_debug && (symbol == SIMBOL_ERR || symbol == SIMBOL_WAR)) sett.reload();
            // sett.reload();
        } else if (settings.debug == DBG_LCD) {
            appendDebugToLCD(msg1);
            appendDebugToLCD(msg2, true);
        }
    }
    void println(SerialSymbol symbol, const __FlashStringHelper* msg1, const __FlashStringHelper* msg2) {
        // проверяем уровни дебага
        if (settings.debug_lvl == DBG_LVL_ALL)
            ;
        else if (settings.debug_lvl == DBG_LVL_INFO) {
            if (symbol < SIMBOL_INFO) return;
        } else if (settings.debug_lvl == DBG_LVL_WARNING) {
            if (symbol < SIMBOL_WAR || symbol == SIMBOL_CUSTOM) return;
        } else if (settings.debug_lvl == DBG_LVL_ERROR) {
            if (symbol < SIMBOL_ERR || symbol == SIMBOL_CUSTOM) return;
        }

        if (settings.debug == DBG_SERIAL) {
            if (settings.color_debug) {
                _serial.write((uint8_t)symbol);
            }
            _serial.print(msg1);
            _serial.println(msg2);
        } else if (settings.debug == DBG_LOGGER) {
            if (symbol == SIMBOL_INFO && settings.color_debug)
                logger.print("info:");
            else if (symbol == SIMBOL_WAR && settings.color_debug)
                logger.print("warn:");
            else if (symbol == SIMBOL_ERR && settings.color_debug) {
                logger.print("err:");
                // updt_time();
                String _time = String(hour) + ":" + (minute > 9 ? String(minute) : "0" + String(minute));
                logger.print("[" + _time + "] ");
            }
            logger.print(msg1);
            logger.println(msg2);
            if (settings.color_debug && (symbol == SIMBOL_ERR || symbol == SIMBOL_WAR)) sett.reload();
            // sett.reload();
        } else if (settings.debug == DBG_LCD) {
            appendDebugToLCD(msg1);
            appendDebugToLCD(msg2, true);
        }
    }
    // Вывод с символом + сообщение + сообщение + сообщение
    template <typename T1, typename T2, typename T3>
    void println(SerialSymbol symbol, const T1& msg1, const T2& msg2, const T3& msg3) {
        // проверяем уровни дебага
        if (settings.debug_lvl == DBG_LVL_ALL)
            ;
        else if (settings.debug_lvl == DBG_LVL_INFO) {
            if (symbol < SIMBOL_INFO) return;
        } else if (settings.debug_lvl == DBG_LVL_WARNING) {
            if (symbol < SIMBOL_WAR || symbol == SIMBOL_CUSTOM) return;
        } else if (settings.debug_lvl == DBG_LVL_ERROR) {
            if (symbol < SIMBOL_ERR || symbol == SIMBOL_CUSTOM) return;
        }

        if (settings.debug == DBG_SERIAL) {
            if (settings.color_debug) _serial.write((uint8_t)symbol);

            _serial.print(msg1);
            _serial.print(msg2);
            _serial.println(msg3);
        } else if (settings.debug == DBG_LOGGER) {
            if (symbol == SIMBOL_INFO && settings.color_debug)
                logger.print("info:");
            else if (symbol == SIMBOL_WAR && settings.color_debug)
                logger.print("warn:");
            else if (symbol == SIMBOL_ERR && settings.color_debug) {
                logger.print("err:");
                // updt_time();
                String _time = String(hour) + ":" + (minute > 9 ? String(minute) : "0" + String(minute));
                logger.print("[" + _time + "] ");
            }
            logger.print(msg1);
            logger.print(msg2);
            logger.println(msg3);
            if (settings.color_debug && (symbol == SIMBOL_ERR || symbol == SIMBOL_WAR)) sett.reload();
            // sett.reload();
        } else if (settings.debug == DBG_LCD) {
            appendDebugToLCD(msg1);
            appendDebugToLCD(msg2);
            appendDebugToLCD(msg3, true);
        }
    }
    void println(SerialSymbol symbol, const __FlashStringHelper* msg1, const __FlashStringHelper* msg2, const __FlashStringHelper* msg3) {
        // проверяем уровни дебага
        if (settings.debug_lvl == DBG_LVL_ALL)
            ;
        else if (settings.debug_lvl == DBG_LVL_INFO) {
            if (symbol < SIMBOL_INFO) return;
        } else if (settings.debug_lvl == DBG_LVL_WARNING) {
            if (symbol < SIMBOL_WAR || symbol == SIMBOL_CUSTOM) return;
        } else if (settings.debug_lvl == DBG_LVL_ERROR) {
            if (symbol < SIMBOL_ERR || symbol == SIMBOL_CUSTOM) return;
        }

        if (settings.debug == DBG_SERIAL) {
            if (settings.color_debug) {
                _serial.write((uint8_t)symbol);
            }
            _serial.print(msg1);
            _serial.print(msg2);
            _serial.println(msg3);
        } else if (settings.debug == DBG_LOGGER) {  //&& settings.color_debug
            if (symbol == SIMBOL_INFO && settings.color_debug)
                logger.print("info:");
            else if (symbol == SIMBOL_WAR && settings.color_debug)
                logger.print("warn:");
            else if (symbol == SIMBOL_ERR && settings.color_debug) {
                logger.print("err:");
                // updt_time();
                String _time = String(hour) + ":" + (minute > 9 ? String(minute) : "0" + String(minute));
                logger.print("[" + _time + "] ");
            }
            logger.print(msg1);
            logger.print(msg2);
            logger.println(msg3);
            if (settings.color_debug && (symbol == SIMBOL_ERR || symbol == SIMBOL_WAR)) sett.reload();
            // sett.reload();
        } else if (settings.debug == DBG_LCD) {
            appendDebugToLCD(msg1);
            appendDebugToLCD(msg2);
            appendDebugToLCD(msg3, true);
        }
    }

    // Оператор для вывода с символом
    template <typename T>
    void operator()(SerialSymbol symbol, const T& msg) {
        println(symbol, msg);
    }
    template <typename T>
    void operator()(SerialSymbol symbol, const T& msg, int16_t format) {
        println(symbol, msg, format);
    }
    void operator()(SerialSymbol symbol, const __FlashStringHelper* msg) {
        println(symbol, msg);
    }
    template <typename T1, typename T2>
    void operator()(SerialSymbol symbol, const T1& msg1, const T2& msg2) {
        println(symbol, msg1, msg2);
    }
    void operator()(SerialSymbol symbol, const __FlashStringHelper* msg1, const __FlashStringHelper* msg2) {
        println(symbol, msg1, msg2);
    }
    template <typename T1, typename T2, typename T3>
    void operator()(SerialSymbol symbol, const T1& msg1, const T2& msg2, const T3& msg3) {
        println(symbol, msg1, msg2, msg3);
    }
    void operator()(SerialSymbol symbol, const __FlashStringHelper* msg1, const __FlashStringHelper* msg2, const __FlashStringHelper* msg3) {
        println(symbol, msg1, msg2, msg3);
    }

    // Оператор для вывода без символа
    template <typename T>
    void operator()(const T& msg) {
        println(msg);
    }
    template <typename T>
    void operator()(const T& msg, int16_t format) {
        println(msg, format);
    }
    void operator()(const __FlashStringHelper* msg) {
        println(msg);
    }
    void operator()(const char* text, int16_t value) {
        println(text, value);
    }
    template <typename T>
    void operator()(const char* text, const T& value, int16_t format) {
        println(text, value, format);
    }
    void operator()(const __FlashStringHelper* text, int16_t value) {
        println(text, value);
    }
    template <typename T>
    void operator()(const __FlashStringHelper* text, const T& value, int16_t format) {
        println(text, value, format);
    }
    void operator()(const char* text1, const char* text2) {
        println(text1, text2);
    }
    void operator()(const __FlashStringHelper* text1, const __FlashStringHelper* text2) {
        println(text1, text2);
    }
    void operator()(const char* text1, const char* text2, const char* text3) {
        println(text1, text2, text3);
    }
    void operator()(const __FlashStringHelper* text1, const __FlashStringHelper* text2, const __FlashStringHelper* text3) {
        println(text1, text2, text3);
    }
};

// Вывести в дебаг
//-
//- куда дебажим - settings.debug
// 0 - Debug::DBG_NO - никуда
// 1 - Debug::DBG_SERIAL - в сериал порт
// 2 - Debug::DBG_LOGGER - в логгер
// 3 - Debug::DBG_LCD -- на экран
//- уровень дебага - settings.debug_lvl
// 0 - DBG_LVL_ALL
// 1 - DBG_LVL_INFO
// 2 - DBG_LVL_WARNING
// 3 - DBG_LVL_ERROR
//- метки
//    SIMBOL_NONE = 0x00,
//    SIMBOL_INFO = 0x01,
//    SIMBOL_WAR = 0x02,
//    SIMBOL_ERR = 0x03,
//    SIMBOL_CUSTOM = 0x04
//---
//- Вывод:
// DEBUG(SIMBOL_INFO, "Значение переменной value: ", value, " кг");
// DEBUG(SIMBOL_ERR, "[ОШИБКА] Внимание!");
// DEBUG(SIMBOL_NONE, "[ДАТЧИК] значение: ", value);
// DEBUG("ДЕБАГ");
// DEBUG.print(F("собрать стро")); DEBUG.println(F("чку"));
Debug DEBUG(SERIAL_SPEED);

#include <common.h>

bool _setup_ = true;     // в сетап мы или нет
bool exit_flag = false;  // срочно тикаем // тут безнадобности

void updt_time() {
    char day_output[60];

    Datime _time = NTP.get();
    if (_time.year == 1970) _time = rtc.now();

    hour = _time.hour;
    minute = _time.minute;
    second = _time.second;
    day = _time.day;
    month = _time.month;
    year = _time.year;
    weekDay = _time.weekDay;
    // DEBUG(SIMBOL_CUSTOM, "weekDay: ",  weekDay);
    // DEBUG(SIMBOL_CUSTOM, "weekDay: ",  weekday_D[weekDay]);
    // DEBUG(SIMBOL_CUSTOM, "weekDay: ",  weekday_D_short[weekDay]);
    // DEBUG(SIMBOL_CUSTOM, "weekDay: ",  weekday_D_shorty[weekDay]);
    // DEBUG(SIMBOL_CUSTOM, "----------------------------");
    // DEBUG(SIMBOL_CUSTOM, "month: ",  month);
    // DEBUG(SIMBOL_CUSTOM, "month: ",  month_M[month]);
    // DEBUG(SIMBOL_CUSTOM, "month: ",  month_M_short[month]);
    // DEBUG(SIMBOL_CUSTOM, "----------------------------");

    unix = _time.getUnix();

    sprintf(day_output, "%s  %02u %s %04u", weekday_D[weekDay], day, month_M[month], year);

    date_str = day_output;
    time_str = NTP.timeToString();
    // DEBUG(SIMBOL_INFO, "Time source: ", (_time.year != 1970 ? "NTP" : "RTC"));
    // DEBUG(SIMBOL_INFO, "Time: ", _time.timeToString() + " " + _time.dateToString());

    CurrentHour = hour;   // status.timeinfo.tm_hour;
    CurrentMin = minute;  // status.timeinfo.tm_min;
    CurrentSec = second;  // status.timeinfo.tm_sec;

    // DEBUG(SIMBOL_CUSTOM, "date_str: ", date_str);
    // DEBUG(SIMBOL_CUSTOM, "time_str: ", time_str);
}

//=========================================================================================//
// в нижний регистр
void toLower_Case(char* str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower(str[i]);
    }
}

void toUpper_Case(char* data) {
    char temp = 0;                   // Переменная для временного хранения символов
    for (int i = 0; data[i]; i++) {  // Цикл по каждому символу строки data
        temp = data[i] & 0xFF;       // Извлечение младшего байта символа
        // определяем номер иконки
        if (temp == 0xD0) {         // Проверка на кириллический символ (первая часть) число 208
            i++;                    // Переход к следующему байту
            temp = data[i] & 0xFF;  // Извлечение  байта следующего
            if (temp >= 176) {
                data[i] -= 32;
            }

        } else if (temp == 0xD1) {  // Проверка на кириллический символ (вторая часть) число 209

            i++;                    // Переход к следующему байту
            temp = data[i] & 0xFF;  // Извлечение  байта следующего
            if (temp == 145) {
                data[i - 1] = 208;  // буква е если ё
                data[i] = 149;
            } else {
                // маленькие буквы начиная с р
                if (temp >= 128 && temp <= 143) {
                    data[i - 1] = 208;
                    data[i] += 32;
                }

                // temp = ((unsigned int)data[i] - 0x80) & 0xFF; // Корректировка значения для остальных символов
            }
        } else {                             // не русские символы
            if (temp >= 48 && temp <= 57) {  // это цифры

            } else {
                data[i] = tolower(data[i]);
            }
        }
    }
}

#define SPACE_TO_NONE 0        // удаляем все пробелы
#define SPACE_TO_MINUS 1       // удаляем в начале конце а в середине меняем на -
#define SPACE_TO_UNDERSCORE 2  // удаляем в начале конце а в середине меняем на _
#define SPACE_TO_DOT 3         // удаляем в начале конце а в середине меняем на .
#define SPACE_TO_SLASH 4       // удаляем в начале а в конце и середине меняем на /
#define SPACE_START_END 5      // удаляем в начале конце а в середине оставляем
// убрать пробелы и заменить их на нужные символы
// replaceSpaces("", SPACE_TO_DOT);
void replaceSpaces(char* str, uint8_t space) {
    int start = 0;
    int end = strlen(str) - 1;
    while (str[start] == ' ') start++;
    while (str[end] == ' ') end--;

    // Пробелы в середине строки
    int j = 0;
    for (int i = start; i <= end; i++) {
        if (str[i] == ' ') {
            switch (space) {
                case SPACE_TO_NONE:
                    // Не записываем пробелы в новую строку
                    break;
                case SPACE_TO_MINUS:
                    str[j++] = '-';
                    break;
                case SPACE_TO_UNDERSCORE:
                    str[j++] = '_';
                    break;
                case SPACE_TO_DOT:
                    str[j++] = '.';
                    break;
                case SPACE_TO_SLASH:
                    str[j++] = '/';
                    break;
                case SPACE_START_END:
                    str[j++] = ' ';
                    break;
            }
        } else {
            str[j++] = str[i];
        }
    }

    if (space == SPACE_TO_SLASH && str[j - 1] != '/') {
        str[j++] = '/';  // Добавляем / в конце строки, если его нет
    }

    str[j] = '\0';  // Завершаем строку нулевым символом
}
//=========================================================================================//

// делей с вызовом count()
void wait_disp(uint16_t _delay) {
    unsigned long previousDelay = millis();  // записываем текущее время
    while (1) {
        count();
        if (exit_flag) return;

        EXIT();                                 // если не надо дожидаться
        unsigned long currentDelay = millis();  // Текущее время в миллисекундах
        if (currentDelay - previousDelay >= (unsigned long)_delay) {
            previousDelay = currentDelay;  // Сохранить текущее время
            break;
        }
    }

    count();
}

//=========================================================================================//
class WiFiConnector {
   private:
    const int16_t ATTEMPT_DELAY = 500;                   // ms
    const unsigned long CHECK_INTERVAL = 5 * 60 * 1000;  // 5 минут в миллисекундах
    unsigned long lastCheckTime = 0;
    bool isAPMode = false;
    String current_ssid;

    int8_t wifiError = -2;        // -2 - небыыло действий | -1 - подключение | 0 - ок | 1 - нет такой сети | 2 - не тот пароль | 3 - режим станции всегда
    int8_t wifiReservError = -2;  // -2 - небыыло действий | -1 - подключение | 0 - ок | 1 - нет такой сети | 2 - не тот пароль | 3 - режим станции всегда

    bool wifiEnabled = true;                      // глобальный флаг — Wi-Fi включён?
    unsigned long wifiDisableTimeoutMinutes = 0;  // 0 = не отключать по таймеру
    unsigned long lastActivityTime = 0;           // время последней "активности"
    bool timeoutCounting = false;                 // таймер запущен?

    bool tryConnect(const char* ssid, const char* password, bool reserv = false) {
        DEBUG(SIMBOL_INFO, "[WIFI] Пытаемся подключиться...");  //, ssid
        reserv ? setReservError(WIFI_ERR_CONNECTION) : setError(WIFI_ERR_CONNECTION);
        // menu.refresh();
        // drawSettings();
        current_ssid = ssid;

        WiFi.begin(ssid, password);
        int16_t attempts = 0;

        while (WiFi.status() != WL_CONNECTED && attempts < MAX_ATTEMPTS) {
            wait_disp(ATTEMPT_DELAY);
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            DEBUG(SIMBOL_WAR, "[WIFI] Успешно подключились к сети: ", ssid);
            DEBUG(SIMBOL_INFO, "[WIFI] Выданный IP : http://", WiFi.localIP().toString());

            // menu.refresh();
            // drawSettings();
            sett.reload();
            if (reserv) {
                connectionAttempts = 0;
                setReservError(WIFI_ERR_OK);
                setError(WIFI_ERR_WAIT);
            } else {
                connectionAttempts = 0;
                setError(WIFI_ERR_OK);
                setReservError(WIFI_ERR_WAIT);
            }

            return true;
        }

        DEBUG(SIMBOL_ERR, "[WIFI] Не удалось подключиться к сети: ", ssid);
        DEBUG(SIMBOL_ERR, "[WIFI] Неверный пароль!");
        reserv ? setReservError(WIFI_ERR_PASSSWORD) : setError(WIFI_ERR_PASSSWORD);
        WiFi.disconnect();
        // menu.refresh();
        // drawSettings();
        sett.reload();
        return false;
    }

    bool scanForNetwork(const char* ssid, bool reserv = false) {
        DEBUG(SIMBOL_INFO, "[WIFI] Сканируем сети для поиска: ", ssid);
        // menu.refresh();
        // drawSettings();
        int16_t n = WiFi.scanNetworks();
        for (int16_t i = 0; i < n; i++) {
            if (strcmp(WiFi.SSID(i).c_str(), ssid) == 0) {
                DEBUG(SIMBOL_INFO, "[WIFI] Сеть найдена");  //, ssid

                return true;
            }
        }
        DEBUG(SIMBOL_ERR, "[WIFI] Сеть ", ssid, " не найдена!");

        reserv ? setReservError(WIFI_ERR_SSID) : setError(WIFI_ERR_SSID);

        return false;
    }

    String generateUniqueSSID(const char* baseSSID) {
        String newSSID = baseSSID;
        int16_t suffix = 1;
        int16_t n = WiFi.scanNetworks();

        while (true) {
            bool ssidExists = false;
            for (int16_t i = 0; i < n; i++) {
                if (WiFi.SSID(i) == newSSID) {
                    ssidExists = true;
                    break;
                }
            }
            if (!ssidExists) {
                break;
            }
            newSSID = String(baseSSID) + "_" + String(suffix);
            suffix++;
        }
        if (newSSID.length() > 32) {
            newSSID = newSSID.substring(0, 32);  // Обрезаем до 32 символов
            DEBUG(SIMBOL_ERR, "[WIFI] Имя SSID обрезано до 32 символов: ", newSSID);
        }
        if (newSSID != baseSSID) DEBUG(SIMBOL_ERR, "[WIFI] Конфликт имени точки доступа! Сгенерировано уникальное имя: ", newSSID);

        return newSSID;
    }

   public:
    // назначеное доменное имя
    String hostName;
    enum APMode {
        NEVER = 0,              // Никогда не запускать точку доступа
        AP_NO_PASS = 1,         // Точка доступа без пароля при неудачном подключении
        AP_PASS = 2,            // Точка доступа с паролем при неудачном подключении
        ALWAYS_AP_NO_PASS = 3,  // Всегда точка доступа без пароля
        ALWAYS_AP_PASS = 4      // Всегда точка доступа с паролем
    };
    enum WiFiErr {
        WIFI_ERR_OFF = -3,         // отключено
        WIFI_ERR_WAIT = -2,        // небыло дествий  ---
        WIFI_ERR_CONNECTION = -1,  // подключение
        WIFI_ERR_OK = 0,           // подключено
        WIFI_ERR_SSID = 1,         // нет такой сети
        WIFI_ERR_PASSSWORD = 2,    // не тот пароль
        WIFI_ERR_AP_ALWAYS = 3     //  режим станции всегда
    };

    WiFiConnector() {}  // Конструктор

    void begin() {
        setError(WIFI_ERR_CONNECTION);
        setReservError(WIFI_ERR_CONNECTION);
        // Если ALWAYS_AP_*, сразу запускаем точку доступа
        if (settings.ap == ALWAYS_AP_NO_PASS || settings.ap == ALWAYS_AP_PASS) {
            DEBUG(SIMBOL_WAR, "[WIFI] Всегда запускаем в режиме точки доступа");
            startAP();
            // menu.refresh();
            // drawSettings();
            if (settings.switchWifi) {
                setReservError(WIFI_ERR_AP_ALWAYS);
                setError(WIFI_ERR_WAIT);
            } else {
                setError(WIFI_ERR_AP_ALWAYS);
                setReservError(WIFI_ERR_WAIT);
            }

            return;
        }

        DEBUG("[WIFI] Устанавливаем режим станции");
        WiFi.mode(WIFI_STA);

        if (settings.switchWifi) {  // если поменяли сети местами
            // Проверяем резервную сеть
            if (scanForNetwork(settings.ssid_reserv, true) && tryConnect(settings.ssid_reserv, settings.password_reserv, true)) {
                return;
            }
            // Проверяем первую сеть
            if (scanForNetwork(settings.ssid) && tryConnect(settings.ssid, settings.password)) {
                return;
            }
        } else {  // классик, первая - первая, резервная - резервная
            // Проверяем первую сеть
            if (scanForNetwork(settings.ssid) && tryConnect(settings.ssid, settings.password)) {
                return;
            }

            // Проверяем резервную сеть
            if (scanForNetwork(settings.ssid_reserv, true) && tryConnect(settings.ssid_reserv, settings.password_reserv, true)) {
                return;
            }
        }

        // Если не подключились и AP не NEVER, запускаем точку доступа
        if (settings.ap != NEVER) {
            DEBUG(SIMBOL_ERR, "[WIFI] Не удалось подключиться к сетям, запускаем точку доступа");
            startAP();
        } else {
            DEBUG(SIMBOL_ERR, "[WIFI] Подключение не удалось, точка доступа запрещена в настройках (NEVER)");
        }
    }

    int16_t connectionAttempts = 0;
    const int16_t MAX_ATTEMPTS = 20;

    //- -1 - подключение
    //- 0 - ок |
    //- 1 - нет такой сети |
    //- 2 - не тот пароль |
    //- 3 - режим станции всегда |
    void setError(int8_t wifi_error) {
        wifiError = wifi_error;
        sett.reload();
    }
    //- -1 - подключение
    //- 0 - ок |
    //- 1 - нет такой сети |
    //- 2 - не тот пароль |
    //- 3 - режим станции всегда |
    int8_t getError() {
        return wifiError;
    }

    String getErrorStr() {
        switch (getError()) {
            case -3:
                return "выключено";
                break;
            case -2:
                return "---";
                break;
            case -1:
                return "подключение...";
                break;

            case 0:
                return "подключено";
                break;

            case 1:
                return "сеть не найдена";
                break;

            case 2:
                return "неверный пароль";
                break;

            case 3:
                return "всегда точка доступа";
                break;

            default:
                return "критическая ошибка";
                break;
        }
    }

    //- -1 - подключение
    //- 0 - ок |
    //- 1 - нет такой сети |
    //- 2 - не тот пароль |
    //- 3 - режим станции всегда |
    void setReservError(int8_t wifi_error) {
        wifiReservError = wifi_error;
        sett.reload();
    }
    //- -1 - подключение
    //- 0 - ок |
    //- 1 - нет такой сети |
    //- 2 - не тот пароль |
    //- 3 - режим станции всегда |
    int8_t getReservError() {
        return wifiReservError;
    }

    String getReservErrorStr() {
        switch (getReservError()) {
            case -3:
                return "выключено";
                break;
            case -2:
                return "---";
                break;

            case -1:
                return "подключение...";
                break;

            case 0:
                return "подключено";
                break;

            case 1:
                return "сеть не найдена";
                break;

            case 2:
                return "неверный пароль";
                break;

            case 3:
                return "всегда точка доступа";
                break;

            default:
                return "критическая ошибка";
                break;
        }
    }

    void tick() {
        if (!wifiEnabled) return;

        if (settings.sleep && timeoutCounting && wifiDisableTimeoutMinutes > 0) {
            unsigned long now = millis();
            unsigned long elapsed_ms = now - lastActivityTime;
            unsigned long timeout_ms = wifiDisableTimeoutMinutes * 60UL * 1000UL;

            if (elapsed_ms >= timeout_ms) {
                DEBUG(SIMBOL_WAR, "[WIFI] Таймаут ", wifiDisableTimeoutMinutes,
                      " мин истёк → Уходим в сон!");
                disableWiFi();
                BeginSleep();
                return;
            }
        }

        // Проверяем только если в режиме точки доступа и не ALWAYS_AP_*
        if (isAPMode && settings.ap != ALWAYS_AP_NO_PASS && settings.ap != ALWAYS_AP_PASS) {
            unsigned long currentTime = millis();
            if (currentTime - lastCheckTime >= CHECK_INTERVAL) {
                if (connectionAttempts >= MAX_ATTEMPTS) {
                    DEBUG(SIMBOL_ERR, "[WIFI] Закончились попытки подключения (", MAX_ATTEMPTS, "). Для сброса - нажмите кнопку \"подключиться\" ");
                    lastCheckTime = currentTime;
                    return;
                }

                DEBUG(SIMBOL_INFO, "[WIFI] Периодическая проверка доступности сетей");
                if (!settings.switchWifi) {
                    // Проверяем первую сеть
                    if (scanForNetwork(settings.ssid) && tryConnect(settings.ssid, settings.password)) {
                        isAPMode = false;
                        WiFi.mode(WIFI_STA);

                        // DEBUG(SIMBOL_NONE, "[WIFI] Переключились в режим станции");
                        return;
                    }

                    // Проверяем резервную сеть
                    if (scanForNetwork(settings.ssid_reserv, true) && tryConnect(settings.ssid_reserv, settings.password_reserv, true)) {
                        isAPMode = false;
                        WiFi.mode(WIFI_STA);

                        // DEBUG(SIMBOL_NONE, "[WIFI] Переключились в режим станции");
                        return;
                    }
                } else {
                    // Проверяем резервную сеть
                    if (scanForNetwork(settings.ssid_reserv) && tryConnect(settings.ssid_reserv, settings.password_reserv)) {
                        isAPMode = false;
                        WiFi.mode(WIFI_STA);

                        // DEBUG(SIMBOL_NONE, "[WIFI] Переключились в режим станции");
                        return;
                    }
                    // Проверяем первую сеть
                    if (scanForNetwork(settings.ssid, true) && tryConnect(settings.ssid, settings.password, true)) {
                        isAPMode = false;
                        WiFi.mode(WIFI_STA);

                        // DEBUG(SIMBOL_NONE, "[WIFI] Переключились в режим станции");
                        return;
                    }
                }

                connectionAttempts++;
                DEBUG(SIMBOL_INFO, "[WIFI] Попытка подключения: ", connectionAttempts);
                lastCheckTime = currentTime;
            }
        }
    }

    bool isConnected() {
        bool connected = WiFi.status() == WL_CONNECTED;
        if (connected) {
        } else {
        }
        return connected;
    }

    bool isAccessPoint() {
        if (isAPMode) {
            // DEBUG(SIMBOL_INFO, "[WIFI] Часы работают в режиме точки доступа");
        }
        return isAPMode;
    }

    bool isInAPMode() {
        wifi_mode_t mode = WiFi.getMode();
        if (mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA) {
            return WiFi.softAPIP() != IPAddress(0, 0, 0, 0);
        }
        return false;
    }

    bool findDeviceByMDNS(const char* hostname) {
        // Проверяем, работает ли устройство в режиме точки доступа
        if (isAPMode) return false;  // В режиме AP не проверяем mDNS

        // Проверяем, подключены ли к Wi-Fi в режиме станции
        if (WiFi.status() != WL_CONNECTED) {
            DEBUG(SIMBOL_ERR, "[mDNS] Не подключены к Wi-Fi, невозможно выполнить mDNS-запрос");
            return false;
        }

        // Инициализируем mDNS с временным именем
        if (!MDNS.begin("temp-check")) {
            DEBUG(SIMBOL_ERR, "[mDNS] Ошибка инициализации mDNS");
            return false;
        }

        // Отправляем mDNS-запрос для поиска hostname.local
        int n = MDNS.queryService(hostname, "local");
        MDNS.end();  // Завершаем mDNS после запроса

        if (n > 0) {
            DEBUG(SIMBOL_ERR, "[mDNS] Конфликт имени mDNS!");
            DEBUG(SIMBOL_ERR, "[mDNS] Устройство с именем ", String(hostname) + ".local уже существует");
            return true;  // Устройство с таким именем существует
        } else {
            // DEBUG(SIMBOL_INFO, "[mDNS] Устройство с именем ", String(hostname) + ".local не найдено");
            return false;  // Имя свободно
        }
    }

    String generateUniqueMDNSName(const char* baseName) {
        // Пропускаем генерацию mDNS имени в режиме точки доступа
        hostName = baseName;
        if (isAPMode) return String(baseName);  // Возвращаем имя без изменений

        String newName = baseName;
        int16_t suffix = 1;

        while (findDeviceByMDNS(newName.c_str())) {
            newName = String(baseName) + "-" + String(suffix);
            suffix++;
        }

        if (newName != baseName) {
            DEBUG(SIMBOL_ERR, "[mDNS] Сгенерировано уникальное имя: ", newName + ".local");
            DEBUG(SIMBOL_ERR, "[mDNS] Назначьте новое имя в настройках имени точки доступа!");
        }
        DEBUG(SIMBOL_INFO, "[mDNS] Часы доступны по адресу: http://", newName + ".local");

        hostName = newName;
        return newName;
    }

    String getSsid() {
        return current_ssid;
    }

    void startAP() {
        DEBUG(SIMBOL_WAR, "[WIFI] Запускаем точку доступа: ", settings.ssid_ap);

        WiFi.mode(WIFI_AP);
        String uniqueSSID = generateUniqueSSID(settings.ssid_ap);

        if (settings.ap == AP_NO_PASS || settings.ap == ALWAYS_AP_NO_PASS) {
            WiFi.softAP(uniqueSSID.c_str());
            DEBUG(SIMBOL_ERR, "[WIFI] Точка доступа запущена без пароля, ip: ", WiFi.softAPIP().toString().c_str());
        } else {
            // WiFi.softAP(uniqueSSID.c_str(), settings.pass_ap);
            WiFi.softAP(uniqueSSID.c_str(), settings.pass_ap, 1, 0, 2);  // канал 1, не скрытая, макс 2 клиента
            DEBUG(SIMBOL_INFO, "[WIFI] Точка доступа запущена с паролем, ip: ", WiFi.softAPIP().toString().c_str());
        }
        isAPMode = true;
        current_ssid = uniqueSSID;
    }

    void startAP(bool pass) {
        DEBUG(SIMBOL_WAR, "[WIFI] Запускаем точку доступа: ", settings.ssid_ap);

        WiFi.mode(WIFI_AP);
        String uniqueSSID = generateUniqueSSID(settings.ssid_ap);

        if (!pass) {
            WiFi.softAP(uniqueSSID.c_str());
            DEBUG(SIMBOL_ERR, "[WIFI] Точка доступа запущена без пароля, ip: ", WiFi.softAPIP().toString().c_str());
        } else {
            // WiFi.softAP(uniqueSSID.c_str(), settings.pass_ap);
            WiFi.softAP(uniqueSSID.c_str(), settings.pass_ap, 1, 0, 2);  // канал 1, не скрытая, макс 2 клиента
            DEBUG(SIMBOL_INFO, "[WIFI] Точка доступа запущена с паролем, ip: ", WiFi.softAPIP().toString().c_str());
        }
        isAPMode = true;
        current_ssid = uniqueSSID;
    }

    // Выключить Wi-Fi полностью (вызывается вручную)
    void disableWiFi() {
        if (!wifiEnabled) return;

        DEBUG(SIMBOL_WAR, "[WIFI] Принудительное отключение Wi-Fi");

        if (isAPMode) {
            WiFi.softAPdisconnect(true);
            isAPMode = false;
        }
        WiFi.disconnect(true);  // true = выключить WiFi полностью
        WiFi.mode(WIFI_OFF);

        wifiEnabled = false;
        timeoutCounting = false;

        setError(WIFI_ERR_OFF);
        setReservError(WIFI_ERR_OFF);

        // menu.refresh();  // если есть
        // drawSettings();
        sett.reload();
    }

    // Включить Wi-Fi заново (если был выключен)
    void enableWiFi() {
        if (wifiEnabled) return;

        DEBUG(SIMBOL_INFO, "[WIFI] Wi-Fi включается заново");
        wifiEnabled = true;
        lastActivityTime = millis();
        timeoutCounting = (wifiDisableTimeoutMinutes > 0);

        // Перезапускаем логику подключения
        begin();
    }

    // Установить таймаут автоотключения (в минутах)
    // 0 = отключить таймер автоотключения
    void setAutoDisableTimeout(uint32_t minutes) {
        wifiDisableTimeoutMinutes = minutes;

        if (minutes == 0) {
            DEBUG(SIMBOL_INFO, "[WIFI] Таймаут автоотключения отключён");
            timeoutCounting = false;
        } else {
            if (settings.sleep) DEBUG(SIMBOL_INFO, "[WIFI] Установлен таймаут автоотключения: ", minutes, " мин");
            lastActivityTime = millis();
            timeoutCounting = true;
        }

        sett.reload();  // если нужно сохранить в настройки
    }

    // Получить текущий таймаут в минутах
    uint32_t getAutoDisableTimeout() const {
        return wifiDisableTimeoutMinutes;
    }

    // Сбросить/продлить таймер активности (вызывать при действиях пользователя)
    void resetActivityTimer() {
        if (wifiDisableTimeoutMinutes > 0 && wifiEnabled) {
            lastActivityTime = millis();
            timeoutCounting = true;
        }
    }

} wifi;
//=========================================================================================//

class Status {
   public:
    bool i2c_avialable = false;
    bool available_scd40 = false;
    bool available_rtc = false;
    bool available_sht = false;
    bool available_bmep = false;
    bool available_aht = false;
    bool available_ina = false;

    // с датчика бме или бмп
    int8_t bmep_temp = -99;
    int8_t bmep_hum = 0;
    uint16_t bme_press = 0;
    // с датчика sht20
    int8_t sht_temp = -99;
    int8_t sht_hum = 0;
    // с датчика SCD40
    int8_t scd40_temp = -99;
    int8_t scd40_hum = 0;
    int16_t scd40_co2 = 0;

    // с датчика aht20
    int8_t aht_temp = -99;
    int8_t aht_hum = 0;

    uint16_t pm1_0 = 0;
    uint16_t pm2_5 = 0;
    uint16_t pm10 = 0;

    // MQTT
    String mqtt_messege = "time=21:22:33;temp=100;hum=100;co2=2000;pm2_5=100;press=999";
    // данные полученные по mqtt
    int8_t mqtt_count = 0;         // попуток соединения
    float mqtt_temperature = -99;  // Переменная для хранения температуры
    float mqtt_humidity = 0.0;     // Переменная для хранения температуры
    int16_t mqtt_pressure = 0;     // Переменная для хранения давления
    int16_t mqtt_co2 = 0;          // Переменная для хранения CO2
    int16_t mqtt_voc = 0;          // Переменная для хранения
    int16_t mqtt_pm1 = 0;          // Переменная для хранения
    int16_t mqtt_pm25 = 0;         // Переменная для хранения
    int16_t mqtt_pm10 = 0;         // Переменная для хранения
    bool mqtt_connected = false;   // подключились ли мы к mqtt серверу

    bool owm_weather_connected = false;
    int16_t owm_weather_error = 0;

    bool om_weather_connected = false;
    int16_t om_weather_error = 0;
    String om_weather_error_str = "";

    // struct tm timeinfo;

    // принудительный тихий режим
    //- -1 - принудительно выкл
    //- 0 - не вмешиваться
    //-  1 - принудительно вкл
    int8_t force_quiet_mode = 0;
    // вернет тру если тихий режим активен
    bool get_quiet_mode() {
        static bool previus_quiet_mode = false;
        // если расписание выклбчено тихого режима н ет
        if (!settings.quiet_mode) {
            // tele.mute = false;

            if (previus_quiet_mode != false) {
                force_quiet_mode = 0;
                previus_quiet_mode = false;

                if (!_setup_) sett.reload();
            }
            return false;
        }

        // когда старт и конец тихого режима равны тихого режима нет
        if (settings.start_mute == settings.end_mute) {
            if (force_quiet_mode == 1) {  // если принудительный тихий режим
                // tele.mute = true;
                return true;
            }

            if (previus_quiet_mode != false) force_quiet_mode = 0;
            previus_quiet_mode = false;

            // tele.mute = false;
            return false;
        }

        // когда старт тихого режима вечером а окончание утром
        if (settings.start_mute > settings.end_mute) {
            if (secondsSinceMidnight() < settings.end_mute || secondsSinceMidnight() > settings.start_mute) {
                if (previus_quiet_mode != true) {
                    if (!_setup_) sett.reload();
                }
                previus_quiet_mode = true;

                if (force_quiet_mode == -1) {
                    // tele.mute = false;
                    return false;
                }

                // tele.mute = true;
                return true;
            } else {
                if (previus_quiet_mode != false) {
                    force_quiet_mode = 0;
                    if (!_setup_) sett.reload();
                }
                previus_quiet_mode = false;

                if (force_quiet_mode == 1) {
                    // tele.mute = true;
                    return true;
                }

                // tele.mute = false;
                return false;
            }
        }
        // когда старт тихого режима утром а окончание вечером
        else {
            if (secondsSinceMidnight() < settings.end_mute && secondsSinceMidnight() > settings.start_mute) {
                if (previus_quiet_mode != true) {
                    force_quiet_mode = 0;
                    if (!_setup_) sett.reload();
                }
                previus_quiet_mode = true;

                if (force_quiet_mode == -1) {
                    // tele.mute = false;
                    return false;
                }

                // tele.mute = true;
                return true;
            } else {
                if (previus_quiet_mode != false) {
                    force_quiet_mode = 0;
                    if (!_setup_) sett.reload();
                }
                previus_quiet_mode = false;

                if (force_quiet_mode == 1) {
                    // tele.mute = true;
                    return true;
                }
                // tele.mute = false;
                return false;
            }
        }
    }

    //  не пора ли сохранить настройки
    //---------------------------------
    //- вызывать ~ раз в 10 секунд
    //- если настройки изменились то будет сохранено через (5 секунд)
    void check_settings() {
        static uint16_t prevCRC = 0;

        uint16_t currentCRC = calculateCRC16();
        if (currentCRC != prevCRC) {
            // DEBUG(SIMBOL_INFO, "[SETTINGS] Пора сохранить настройки");
            save_mem(true);

            prevCRC = currentCRC;
        }
    }
    // црц для отслеживания изменения в настройках
    uint16_t calculateCRC16() {
        const uint8_t* data = (const uint8_t*)&settings;
        size_t length = sizeof(SETTINGS);
        uint16_t crc = 0xFFFF;

        for (size_t i = 0; i < length; i++) {
            crc ^= (uint16_t)data[i];
            for (uint8_t j = 0; j < 8; j++) {
                if (crc & 0x0001) {
                    crc >>= 1;
                    crc ^= 0xA001;  // Полином CRC16
                } else {
                    crc >>= 1;
                }
            }
        }
        return crc;
    }
    // сохранить в пзу
    //---
    //- now - сохранить сразу
    //- !now - сохранит через таймаут   (5 секунд)
    void save_mem(bool now = false) {
        if (now) {
            base.updateNow();
            DEBUG(SIMBOL_INFO, "[BASE] Сохранили настройки");
        } else
            base.update();
    }

    bool CheckSensors() {
        uint8_t avialable = 0;

        if (ReadCD40()) avialable++;
        if (ReadAHT()) avialable++;
        if (ReadSHT()) avialable++;
        if (ReadBMP()) avialable++;
        //====================================================================//

        updateSensors();
        return (bool)avialable;
    }

    void updateSensors() {
        switch (settings.source_temp) {
            case 0:  // BME/BMP
                _temp = bmep_temp;
                break;
            case 1:  // SHT
                _temp = sht_temp;

                break;
            case 2:  // MQTT
                _temp = mqtt_temperature;

                break;
            case 3:  // SCD40
                _temp = scd40_temp;
                break;
            case 4:  // AHT
                _temp = aht_temp;
                break;

            default:
                break;
        }
        switch (settings.source_hum) {
            case 0:  // BME/BMP
                _hum = bmep_hum;

                break;
            case 1:  // SHT
                _hum = sht_hum;

                break;
            case 2:  // MQTT
                _hum = mqtt_humidity;

                break;
            case 3:  // SCD40
                _hum = scd40_hum;

                break;
            case 4:  // AHT
                _hum = aht_hum;
                break;

            default:
                break;
        }
        switch (settings.source_press) {
            case 0:  // BME/BMP

                _press = bme_press;

                break;
            case 1:  // MQTT
                _press = mqtt_pressure;

                break;
            case 2:  // OUTSIDE
                _press = WxConditions[0].Pressure * 0.75006;

                break;

            default:
                break;
        }
        switch (settings.source_co2) {
            case 0:  // MQTT
                _co2 = mqtt_co2;

                break;
            case 1:  // SCD40

                _co2 = scd40_co2;

            default:
                break;
        }
    }

    int8_t get_temp() { return _temp + settings.temp_calibration_coeff; }
    int8_t get_hum() { return _hum + settings.hum_calibration_coeff; }
    uint16_t get_press() { return _press; }

    uint16_t get_co2() { return _co2 + settings.co2_calibration_coeff; }
    uint16_t get_voc() { return mqtt_voc; }
    uint16_t get_pm1() { return pm1_0; }
    uint16_t get_pm2_5() { return pm2_5; }
    uint16_t get_pm10() { return pm2_5; }

    void I2C_Init() {
        i2c_avialable = Wire.begin(21, 22);
        // bool i2c_avialable = Wire.begin(42, 2, 50000);

        if (i2c_avialable)
            DEBUG("INIT I2C  ... ok");
        else
            DEBUG("INIT I2C  ... failed");
    }
    void I2C_sensors_init() {
        delay(100);

        if (i2c_avialable)
            DEBUG(SIMBOL_INFO, ("Инициализация i2c устройств..."));
        else {
            DEBUG(SIMBOL_ERR, ("I2C Шина недоступна, повторная попытка инициализации"));
            I2C_Init();
            delay(100);

            if (i2c_avialable) {
                DEBUG(SIMBOL_INFO, ("Инициализация i2c устройств..."));
            } else {
                DEBUG(SIMBOL_ERR, ("I2C Шина недоступна!"));
                available_rtc = false;
                available_sht = false;
                available_scd40 = false;
                available_bmep = false;
                available_aht = false;
                available_ina = false;
                available_scd40 = false;

                return;
            }
        }
        if (settings.debug) {
            DEBUG(F("scan..."));

            uint8_t error, address;
            uint16_t nDevices = 0;
            for (address = 1; address < 127; address++) {
                Wire.beginTransmission(address);
                error = Wire.endTransmission();

                if (error == 0) {
                    DEBUG.print(F("доступно устройство по адресу 0x"));
                    if (address < 16) DEBUG.print(F("0"));
                    DEBUG.print(address, HEX);
                    DEBUG.print(" (");
                    DEBUG.print(address);
                    DEBUG.println(")");

                    nDevices++;
                } else if (error != 2) {
                    DEBUG.print(F("error: ("));
                    DEBUG.print(error);
                    DEBUG.print(F(") "));
                    switch (error) {
                        case 1:
                            DEBUG.print(F("слишком большие данные не помещаются в буфер передачи"));
                            break;
                        case 2:
                            DEBUG.print(F("устройство не отвечает"));
                            break;
                        case 3:
                            DEBUG.print(F("ошибка при передаче данных"));
                            break;
                        case 4:
                            DEBUG.print(F("просто ошибка"));
                            break;
                        case 5:
                            DEBUG.print(F("вышел таймаут"));
                            break;

                        default:
                            DEBUG.print(F("хз, какая-то фигня"));
                            break;
                    }

                    DEBUG.print(F(",  по адресу 0x"));
                    DEBUG.print(F("0"));
                    DEBUG.println(address, HEX);
                }
            }
            if (nDevices == 0) {
                DEBUG(F("нет подключенных устройств"));
                i2c_avialable = false;
            }
            DEBUG("-------------------");
        }

        if (settings.rtc_snsr)
            initRTC();
        else
            available_rtc = false;

        ////////////////////////////////// инициализацие BME / BMP датчика
        if (settings.bmep_snsr) {
            // инициализация датчика бме280
            initBMP();
        } else {
            available_bmep = false;
        }

        ////////////////////////////////// инициализацие sht20 датчика
        if (settings.sht_snsr) {
            initSHT();

        } else
            available_sht = false;

        ////////////////////////////////// инициализацие aht датчика
        if (settings.aht_snsr) {
            initAHT();
        } else
            available_aht = false;

        /////////////////////////////////// инициализация ina219
        /*
        if (settings.ina_snsr) {
            uint8_t ina_conn = ina.begin(21, 22, settings.ADDR_INA);
            if (ina_conn == 0) {  // ina.begin(4, 5) // Для ESP32/ESP8266 можно указать пины I2C4, 5

                ina.setResolution(INA219_VBUS, INA219_RES_12BIT_X128);  // Напряжение в 12ти битном режиме + 4х кратное усреднение
                if (ina.getCalibration()) available_ina = true;

                DEBUG(SIMBOL_INFO, F("INIT INA219  ... ok"));

            } else {
                available_ina = false;
                DEBUG(SIMBOL_ERR, F("INIT INA219  ... failed"));
                // Wire.beginTransmission(settings.ADDR_INA);
                // uint8_t conn = Wire.endTransmission();
                DEBUG(SIMBOL_ERR, "err: ", ina_conn);
                switch (ina_conn) {
                    case 0:
                        DEBUG(SIMBOL_ERR, F("INIT INA219  ... ok"));
                        break;
                    case 1:
                        DEBUG(SIMBOL_ERR, F("INA219 Err:  DATA_TOO_LONG"));
                        break;
                    case 2:
                        DEBUG(SIMBOL_ERR, F("INIT Err:  RECEIVED_NACK_ON_ADDRESS"));
                        break;
                    case 3:
                        DEBUG(SIMBOL_ERR, F("INIT Err:  RECEIVED_NACK_ON_DATA"));
                        break;
                    case 4:
                        DEBUG(SIMBOL_ERR, F("INIT Err:  OTHER_ERROR"));
                        break;
                    case 5:
                        DEBUG(SIMBOL_ERR, F("INIT Err:  TIMEOUT"));
                        break;

                    default:
                        break;
                }
            }
        } else
            available_ina = false;
        */

        ////////////////////////////////// инициализация SCD40
        if (settings.scd40_snsr) {
            initSCD40();
        } else
            available_scd40 = false;

        delay(100);
    }
    bool initRTC() {
        if (!i2c_avialable) return false;
        if (rtc.begin(&Wire, settings.ADDR_RTC)) {
            available_rtc = true;
            DEBUG(SIMBOL_INFO, "INIT DS3231  ... ok");
            DEBUG(SIMBOL_NONE, "DS3231: " + rtc.dateToString() + " " + rtc.timeToString());
        } else {
            available_rtc = false;
            DEBUG(SIMBOL_ERR, "INIT DS3231  ... failed");
        }

        return available_rtc;
    }
    bool initSHT() {
        if (!i2c_avialable) return false;
        bool sht_bgn = sht.init();
        String errorStr;
        if (sht_bgn)
            errorStr = "INIT SHT   ... ok";
        else
            errorStr = "INIT SHT   ... failed ";

        sht_bgn ? DEBUG(SIMBOL_INFO, errorStr) : DEBUG(SIMBOL_ERR, errorStr);

        if (sht_bgn) {
            switch (sht.mSensorType) {
                case SHTSensor::SHT3X:
                    DEBUG(SIMBOL_NONE, F("Обнаружен: SHT3x (стандартный адрес)"));
                    break;
                case SHTSensor::SHT3X_ALT:
                    DEBUG(SIMBOL_NONE, F("Обнаружен: SHT3x (альтернативный адрес)"));
                    break;
                case SHTSensor::SHTC1:
                    DEBUG(SIMBOL_NONE, F("Обнаружен: SHTC1 / SHTC3 / SHTW1 / SHTW2"));
                    break;
                case SHTSensor::SHT85:
                    DEBUG(SIMBOL_NONE, F("Обнаружен: SHT85"));
                    break;
                case SHTSensor::SHT4X:
                    DEBUG(SIMBOL_NONE, F("Обнаружен: SHT4x"));
                    break;
                case SHTSensor::SHT2X:
                    DEBUG(SIMBOL_NONE, F("Обнаружен: SHT2x"));
                    break;
                default:
                    DEBUG(SIMBOL_NONE, F("Обнаружен неизвестный/неподдерживаемый тип"));
                    break;
            }
            String seensr_temp_str = "Temp = " + String(sht.getTemperature()) + " °C; ";
            String seensr_hum_str = "Hum = " + String(sht.getHumidity()) + " %";
            DEBUG(SIMBOL_NONE, seensr_temp_str + seensr_hum_str);
            available_sht = true;
        } else
            available_sht = false;

        return available_sht;
    }
    bool initAHT() {
        if (!i2c_avialable) return false;
        aht.setType(AHT2x_SENSOR);
        aht.setAddress(settings.ADDR_AHT);

        bool aht_bgn = aht.begin();
        if (aht_bgn) {
            DEBUG(SIMBOL_INFO, "INIT AHT   ... ok");
            available_aht = true;

            String seensr_temp_str = "Temp = " + String(aht.readTemperature()) + " °C; ";
            String seensr_hum_str = "Hum = " + String(aht.readHumidity()) + " %";
            DEBUG(SIMBOL_NONE, seensr_temp_str + seensr_hum_str);
        } else {
            DEBUG(SIMBOL_ERR, "INIT AHT   ... failed");
            available_aht = false;
        }
        return available_aht;
    }
    bool initBMP() {
        if (!i2c_avialable) return false;
        if (bme.begin(settings.ADDR_BME_P)) {
            available_bmep = true;
            DEBUG(SIMBOL_INFO, "INIT BME/P 280  ... ok");

            String seensr_temp_str = "Temp = " + String(bme.readTemperature()) + " °C; ";
            String seensr_hum_str = "Hum = " + String(bme.readHumidity()) + " %; ";
            String seensr_press_str = "Hum = " + String(bme.readPressure()) + " Pa";
            DEBUG(SIMBOL_NONE, seensr_temp_str + seensr_hum_str + seensr_press_str);
        } else {
            available_bmep = false;
            DEBUG(SIMBOL_ERR, "INIT BME/P 280  ... failed");
        }

        return available_bmep;
    }
    bool initSCD40() {
        if (!i2c_avialable) return false;
        if (co2.begin()) {
            available_scd40 = true;
            DEBUG(SIMBOL_INFO, "INIT SCD40  ... ok");
            delay(500);
            // DEBUG(SIMBOL_INFO, F("INIT SCD40  ... ok"));
        } else {
            available_scd40 = false;
            // DEBUG(SIMBOL_ERR, F("INIT SCD40  ... failed"));
            DEBUG(SIMBOL_ERR, "INIT SCD40  ... failed");
        }

        return available_scd40;
    }

    bool getWeather() {
        switch (settings.weather) {
            case 0:
                DEBUG(SIMBOL_ERR, "Получение погоды отключено!");

                updateSensors();
                return false;
                break;
            case 1:
                DEBUG(SIMBOL_ERR, "[В разработке] Получение погоды от openweathermap.org не работает!");

                updateSensors();
                return false;
                break;
            case 2: {
                bool ok = getOpenMeteoWeather();
                if (settings.internet_time == 0) {
                    NTP.end();
                    uint32_t _unix = WxConditions[0].Dt;
                    NTP.setGMT(0);
                    NTP.sync(_unix);
                }

                updateSensors();
                return ok;
            } break;

            default:
                break;
        }

        updateSensors();
        return false;
    }
    String urlEncode(String str) {
        String encoded = "";
        char c;
        for (unsigned int i = 0; i < str.length(); i++) {
            c = str.charAt(i);
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                encoded += c;
            } else if (c == ' ') {
                encoded += "+";
            } else {
                encoded += "%";
                encoded += String(c, HEX).substring(0, 2);
            }
        }
        return encoded;
    }
    bool getCityCoordinates(const char* cityName) {
        if (!IF_WIFI) {
            DEBUG(SIMBOL_ERR, "[GEO] Нет Wi-Fi");
            return false;
        }

        if (strlen(cityName) < 2) {
            DEBUG(SIMBOL_ERR, "[GEO] Слишком короткое название города");
            return false;
        }
        bool ok = false;

        HTTPClient http;
        String url = "https://geocoding-api.open-meteo.com/v1/search?";
        url += "name=" + urlEncode(String(cityName));  // экранировать русские буквы, пробелы и т.д.
        url += "&count=1";                             // берём только самый вероятный результат
        url += "&language=ru";                         // чтобы название города пришло на русском если возможно
        url += "&format=json";

        DEBUG(SIMBOL_INFO, "[GEO] Запрос координат для: ", cityName);
        DEBUG(SIMBOL_NONE, "[GEO] URL: ", url);

        http.begin(url);
        int16_t httpCode = http.GET();

        if (httpCode > 0) {
            String payload = http.getString();
            if (settings.extra_debug) {
                DEBUG(SIMBOL_INFO, "Ответ: ", payload);
            }

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);

            if (!error) {
                JsonArray results = doc["results"];

                if (results.size() > 0) {
                    JsonObject first = results[0];

                    float lat = first["latitude"] | 0.0;
                    float lon = first["longitude"] | 0.0;
                    String countryStr = first["country"].as<const char*>();  //
                    String adminStr = first["admin1"].as<const char*>();     //

                    // Проверка на валидные координаты
                    if (lat != 0.0 && lon != 0.0) {
                        settings.latitude = lat;
                        settings.longitude = lon;

                        // Опционально: можно сохранить полное название и страну для отображения
                        // const char* foundName   = first["name"]   | "Unknown";
                        // const char* countryCode = first["country_code"] | "";

                        DEBUG(SIMBOL_INFO, "[GEO] Координаты найдены!");

                        // === Сдвигаем старые города вниз ===
                        for (int i = 4; i > 0; i--) {
                            strlcpy(settings.last_city_owm[i], settings.last_city_owm[i - 1], sizeof(settings.last_city_owm[0]));
                        }

                        // === новый город ===
                        snprintf(settings.last_city_owm[0], sizeof(settings.last_city_owm[0]), "%s", first["name"].as<const char*>());
                        toUpper_Case(settings.last_city_owm[0]);

                        //  основной city_om
                        strlcpy(settings.city_om, settings.last_city_owm[0], sizeof(settings.city_om));

                        sett.reload();

                        DEBUG(SIMBOL_NONE, "[GEO] Страна: ", countryStr);
                        DEBUG(SIMBOL_NONE, "[GEO] Регион: ", adminStr);
                        DEBUG(SIMBOL_INFO, "[GEO] Город: ", settings.last_city_owm[0], " (" + String(first["country_code"].as<const char*>()) + ")");
                        DEBUG(SIMBOL_NONE, "[GEO] lat: ", String(lat, 6));
                        DEBUG(SIMBOL_NONE, "[GEO] lon: ", String(lon, 6));

                        ok = true;

                        owm_weather_connected = false;  // чтобы следующая итерация погоды обновилась
                    } else {
                        DEBUG(SIMBOL_ERR, "[GEO] Координаты нулевые → город не найден корректно");
                        ok = false;
                    }
                } else {
                    DEBUG(SIMBOL_ERR, "[GEO] Город не найден (results пустой)");
                    ok = false;
                }
            } else {
                DEBUG(SIMBOL_ERR, "[GEO] Ошибка парсинга JSON: ", error.c_str());
                DEBUG(SIMBOL_ERR, "[GEO] Ответ сервера: ", payload.substring(0, 200));  // первые 200 символов
                ok = false;
            }
        } else {
            DEBUG(SIMBOL_ERR, "[GEO] HTTP ошибка: ", httpCode);
            ok = false;
        }

        http.end();
        return ok;
    }

   private:
    int8_t _temp = 0;
    int8_t _hum = 0;
    uint16_t _press = 0;
    uint16_t _co2 = 0;

    // Функция для получения количества секунд с полуночи
    unsigned long secondsSinceMidnight() {
        return NTP.hour() * 3600 + NTP.minute() * 60 + NTP.second();
    }

    bool getOpenMeteoWeather() {
        DEBUG(SIMBOL_WAR, "[WEATHER] Получаем погоду от Open-Meteo ---");
        byte Attempts = 1;
        bool RxWeather = false;
        WiFiClient client;  // wifi client object
        while (RxWeather == false && Attempts <= 5) {
            DEBUG("[WEATHER] Попытка: " + String(Attempts));
            if (RxWeather == false) RxWeather = ReceiveOneCallWeather(client, settings.extra_debug);
            count();
            Attempts++;
        }

        if (RxWeather)
            ;
        else
            DEBUG(SIMBOL_ERR, "[WEATHER] Ошибка получения погоды ----");

        return RxWeather;
    }

    bool ReadSHT() {
        if (!available_sht || !settings.sht_snsr) {
            sht_temp = -99;
            sht_hum = 0;
            available_sht = false;
            return false;
        }

        uint8_t ok = sht.readSample();

        if (ok) {
            available_sht = true;
            sht_temp = sht.getTemperature();
            sht_hum = sht.getHumidity();
        } else {
            sht_temp = -99;
            sht_hum = 0;
            available_sht = false;
            DEBUG(SIMBOL_ERR, "[SENSOR] Error AHT - недоступен ");
        }

        return available_sht;
    }
    bool ReadAHT() {
        if (!available_aht || !settings.aht_snsr) {
            aht_temp = -99;
            aht_hum = 0;
            return false;
        }
        uint8_t err = aht.getStatus();

        if (err == 0) {
            available_aht = true;
            aht_temp = aht.readTemperature();
            aht_hum = aht.readHumidity();
        } else {
            aht_temp = -99;
            aht_hum = 0;

            available_aht = false;
            DEBUG(SIMBOL_ERR, "[SENSOR] Error AHT: ", err);
        }

        return available_aht;
    }
    bool ReadBMP() {
        if (!available_bmep || !settings.bmep_snsr) {
            bmep_temp = -99;
            bmep_hum = 0;
            bme_press - 0;
            return false;
        }

        int8_t tmp = bme.readTemperature();
        int8_t hum = bme.readHumidity();
        int16_t press = bme.readPressure();
        press = pressureToMmHg(press);

        if (hum != 0) {
            available_bmep = true;
            bmep_temp = tmp;
            bmep_hum = hum;
            bme_press = press;

        } else {
            available_bmep = false;
            DEBUG(SIMBOL_ERR, "[SENSOR] Error BME/P - недоступен ");
            bmep_temp = -99;
            bmep_hum = 0;
            bme_press - 0;
        }

        return available_bmep;
    }

    bool ReadCD40(bool once = true) {
        if (!available_scd40 || !settings.scd40_snsr) {
            scd40_co2 = 0;
            scd40_temp = -99;
            scd40_hum = 0;
            return false;
        }

        DEBUG(SIMBOL_WAR, "[SCD40] Получаем данные ==============");
        delay(50);

        // Останавливаем все
        co2.stopPeriodicMeasurement();
        delay(600);

        // Запускаем периодические измерения
        co2.startPeriodicMeasurement();

        // Ждём первое измерение (максимум ~6-7 секунд)
        DEBUG("[SCD40] Ожидание измерения...");

        unsigned long startTime = millis();
        bool dataReady = false;

        while (millis() - startTime < 6000) {  // таймаут секунд обычно за 4 с небольшим секунды
            if (co2.getDataReadyStatus()) {
                dataReady = true;
                break;
            }
            delay(100);  // проверяем каждые 100 мс
        }
        DEBUG(SIMBOL_INFO, "[SCD40] Измерение заняло: ", millis() - startTime, "мс");

        // Проверяем готовность
        if (dataReady) {
            DEBUG(SIMBOL_WAR, "[SCD40] Данные получены!");

            if (co2.readMeasurement()) {
                scd40_co2 = co2.getCO2();
                scd40_temp = round(co2.getTemperature());
                scd40_hum = round(co2.getHumidity());

                DEBUG("CO2: " + String(scd40_co2) + " ppm");
                DEBUG("Temp: " + String(scd40_temp) + " C");
                DEBUG("Hum: " + String(scd40_hum) + " %");

                // Останавливаем периодические измерения
                co2.stopPeriodicMeasurement();
                return true;
            }
        }

        DEBUG(SIMBOL_ERR, "[SENSOR] SCD40 - Ошибка получения данных");
        scd40_co2 = 0;
        scd40_temp = -99;
        scd40_hum = 0;

        return false;
    }

} status;

// OTA
void ota_init() {
    ArduinoOTA.setHostname(NETWORK_NAME);
    ArduinoOTA.onStart([]() {
        // led.on();
        DEBUG(SIMBOL_ERR, "==============================");
        DEBUG(SIMBOL_WAR, "==== OTA  UPDATE  STARTED ====");
        // printOta(0);
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        // led.tick();
        //  Serial.printf("progress: %u%%\r", (progress / (total / 100)));
        //  printOta(progress / (total / 100));
    });
    ArduinoOTA.onEnd([]() {
        // led.off();
        DEBUG(SIMBOL_INFO, "======== OTA FINISHED ========");
        DEBUG(SIMBOL_WAR, "========== RESTART  ==========");
        // printOta(225);
    });
    ArduinoOTA.onError([](ota_error_t error) {
        if (error == OTA_AUTH_ERROR)
            DEBUG(SIMBOL_ERR, "OTA update error: Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
            DEBUG(SIMBOL_ERR, "OTA update error: Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
            DEBUG(SIMBOL_ERR, "OTA update error: Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
            DEBUG(SIMBOL_ERR, "OTA update error: Receive Failed");
        else if (error == OTA_END_ERROR)
            DEBUG(SIMBOL_ERR, "OTA update error: End Failed");

        // printOta(224);
    });
    ArduinoOTA.begin();
}

// NTP
void ntp_init() {
    NTP.setHost(settings.ntp_host);

    switch (settings.internet_time) {
        case 0: {  // местное
            NTP.end();
            uint32_t _unix = WxConditions[0].Dt;
            NTP.setGMT(0);
            NTP.sync(_unix);
            DEBUG(SIMBOL_INFO, "[NTP] Используем местное время (полученное с сервера погоды)");
        } break;
        case 1:  // ntp сервер

            if (NTP.begin(settings.timeZone))
                DEBUG(SIMBOL_INFO, "[NTP] Cервер подключен: ", settings.ntp_host);
            else
                DEBUG(SIMBOL_ERR, "[NTP] Неудалось подключиться к: ", settings.ntp_host);

            NTP.updateNow();
            break;
        case 2:  // часы
            NTP.end();
            if (status.available_rtc) {
                NTP.attachRTC(rtc);
                Datime dt = rtc.getTime();
                uint32_t _unix = dt.getUnix();
                rtc.setUnix(_unix);
                NTP.sync(_unix);
                DEBUG(SIMBOL_INFO, "[NTP] Используем время с модуля RTC");
            }
            break;

        default:
            break;
    }

    NTP.tick();
}

// MQTT
#include "ha_mqtt.h"
WiFiClient esp;
HA_MQTT ha(esp);
bool mqtt_init(bool sleep = false) {
    DEBUG(SIMBOL_WAR, F("------------ MQTT -------------"));

    ha.setMaxAttempts(settings.count_mqtt);
    ha.setReconnectInterval(8000);
    ha.setSocketTimeout(3000);
    if (sleep) {
        int16_t sleep_time = status.get_quiet_mode() ? settings.sleep_duration_quiet_mode : settings.sleep_duration;
        ha.setKeepAlive(sleep_time * 60);
    } else
        ha.setKeepAlive(20);
    // ha.setWiFiTimeout(2500);

    ha.setServer(settings.mqtt_server, settings.mqtt_port);
    DEBUG(SIMBOL_INFO, "[MQTT] Сервер:" + String(settings.mqtt_server), " Порт: " + String(settings.mqtt_port));

    ha.setAuth(settings.mqtt_user, settings.mqtt_pass);
    DEBUG(SIMBOL_NONE, "[MQTT] Логин:" + String(settings.mqtt_user), " Пароль: " + String(settings.mqtt_pass));

    ha.setClient(settings.mqtt_client_name);
    DEBUG(SIMBOL_NONE, "[MQTT] Имя:" + String(settings.mqtt_client_name));

    ha.setTopics(settings.mqtt_topic_pub, settings.mqtt_topic_sub);
    DEBUG(SIMBOL_NONE, "[MQTT] Топик отправки:" + String(settings.mqtt_topic_pub));
    DEBUG(SIMBOL_NONE, "[MQTT] Топик подкиски:" + String(settings.mqtt_topic_sub));

    ha.addSensor("Температура", "temp", "°C", "temperature", 1);
    ha.addSensor("Влажность", "hum", "%", "humidity");
    ha.addSensor("Давление", "press", "mmHg", "pressure");

    ha.addSensor("CO₂", "co2", "ppm", "carbon_dioxide", 0);
    ha.addSensor("PM1", "pm1", "µg/m³", "pm1", 0);
    ha.addSensor("PM2.5", "pm2_5", "µg/m³", "pm25", 0);
    ha.addSensor("PM10", "pm10", "µg/m³", "pm10", 0);

    ha.addSensor("VOC", "voc", "ppb", "volatile_organic_compounds_parts", 0);
    ha.addSensor("Random", "random", "", "", 0);
    // ha.addSensor("Последнее обновление", "last_seen", "", "timestamp", 0);

    ha.onMessage([](String topic, String msg) {
        DEBUG(SIMBOL_WAR, "[MQTT][", NTP.timeToString(), "] Сообщение ==========");
        DEBUG(SIMBOL_INFO, "Топик: ", topic);
        DEBUG(SIMBOL_INFO, "Сообщение: ", msg);

        if (msg == "ping") DEBUG("pong");

        if (topic == String(settings.mqtt_topic_sub)) {
            DEBUG(SIMBOL_INFO, "Пришла подписка");

            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, msg);
            int8_t mqtt_temperature = -99;
            int8_t mqtt_humidity;
            int16_t mqtt_pressure;
            int16_t mqtt_co2;
            int16_t mqtt_pm1;
            int16_t mqtt_pm25;
            int16_t mqtt_pm10;
            int16_t mqtt_voc;

            if (!err) {
                mqtt_temperature = doc["temp"] | -99;
                mqtt_humidity = doc["hum"] | 0;
                mqtt_pressure = doc["press"] | 0;
                mqtt_co2 = doc["co2"] | 0;
                mqtt_pm1 = doc["pm1"] | 0;
                mqtt_pm25 = doc["pm2_5"] | 0;
                mqtt_pm10 = doc["pm10"] | 0;
                mqtt_voc = doc["voc"] | 0;

            } else if (err) {
                DEBUG(SIMBOL_ERR, "[MQTT] Ошибка парсинга JSON: ", err.c_str());
                DEBUG(SIMBOL_ERR, "[MQTT] Ответ сервера: ", msg.substring(0, 200));  // первые 200 символов
            }

            status.mqtt_temperature = mqtt_temperature;
            status.mqtt_humidity = mqtt_humidity;
            status.mqtt_pressure = mqtt_pressure;
            status.mqtt_co2 = mqtt_co2;
            status.mqtt_voc = mqtt_voc;
            status.mqtt_pm1 = mqtt_pm1;
            status.mqtt_pm25 = mqtt_pm25;
            status.mqtt_pm10 = mqtt_pm10;
        }

        DEBUG(SIMBOL_WAR, "[MQTT] ==============================");
    });

    bool ok = ha.begin();

    if (ok) {
        DEBUG(SIMBOL_INFO, "[MQTT] Успешное подключение");
        uint8_t device_count = ha.getDevicesCount();

        if (device_count > 0) {
            DEBUG(SIMBOL_INFO, "[MQTT] Доступных устройств:", device_count);
            for (uint8_t i = 0; i < device_count; i++) {
                DEBUG(SIMBOL_NONE, "[MQTT] Топик устройства " + String(i + 1) + ": ", ha.getTopic(i));
            }
            // DEBUG(SIMBOL_NONE, "[MQTT] Список всех топиков: ", ha.getTopicsString());
        }

    } else
        DEBUG(SIMBOL_ERR, "[MQTT] Ошибка подключения!");

    return ok;
}
bool sendMqttData() {
    if (!settings.mqtt || !(WIFI)) return false;
    DEBUG(SIMBOL_INFO, "[MQTT] Отправка данных ---");
    return ha.sendState([](JsonDocument& doc) {
        doc["temp"] = status.get_temp();
        doc["hum"] = status.get_hum();

        doc["press"] = status.get_press();
        doc["co2"] = status.get_co2();
        doc["random"] = random(2000);
        // doc["last_seen"] = NTP.getUnix();
    });
}

//=========================================================================================//
// #include <pcf8574_esp.h>
// PCF857x pcf(settings.ADDR_PCF, &Wire);
// Прерывания для кнопок
// const uint8_t PIN_pcf_int = 19;  // пин прерывания для кнопок
volatile bool PCF_InterruptFlag = false;
// void IRAM_ATTR PCFInterrupt() { PCF_InterruptFlag = true; }

// Типы кнопок
#define PIN_btn 0
#define PCF_btn 1
#define PCF3_btn 2
#define PIN_btn_HIGH 3
#define PIN_btn_PULLUP 4
class BUTTON {
   public:
    // Состояния кнопки для release
    enum ButtonState {
        NO_ACTION = 0,
        PUSH = 1,
        CLICK = 2,
        HOLD = 3
    };

    // временные пороги
    BUTTON(uint8_t type,
           uint8_t pin,
           uint16_t debounceTime = DEBOUNCE_TIME,
           uint16_t holdTime = HOLD_TIME,
           uint16_t holdedTime = HOLDED_TIME) {
        _type = type;
        _pin = pin;
        _debounceTime = debounceTime;
        _holdTime = holdTime;
        _holdedTime = holdedTime;

        // Инициализация пинов в зависимости от типа
        if (_type == PIN_btn || _type == PIN_btn_HIGH) pinMode(_pin, INPUT);
        if (_type == PIN_btn_PULLUP) pinMode(_pin, INPUT_PULLUP);
    }

    // клик кнопокой
    bool click() {
        bool btnState = getButtonState();

        if (!btnState && !_flag && millis() - _tmr >= _debounceTime) {
            _flag = true;
            _tmr = millis();
            return true;
        }
        if (btnState && _flag) {
            resetInterruptFlags();
            _flag = false;
            _tmr = millis();
        }
        return false;
    }

    // Проверка удержания (1 секунда по умолчанию)
    // - возвращает тру каждые 1 секунду удержания
    bool hold() {
        bool btnState = getButtonState();

        if (!btnState && !_flag_hold && millis() - _tmr_hold >= _debounceTime) {
            _flag_hold = true;
            _tmr_hold = millis();
        }
        if (!btnState && _flag_hold && millis() - _tmr_hold >= _holdTime) {
            _flag_hold = false;
            return true;
        }
        if (btnState && _flag_hold) {
            resetInterruptFlags();
            _flag_hold = false;
        }
        return false;
    }

    // Проверка долгого удержания (15 секунд по умолчанию)
    // - возвращает тру каждые 15 секунд удержания
    bool holded() {
        bool btnState = getButtonState();

        if (!btnState && !_flag_holded && millis() - _tmr_holded >= _debounceTime) {
            _flag_holded = true;
            _tmr_holded = millis();
        }
        if (!btnState && _flag_holded && millis() - _tmr_holded >= _holdedTime) {
            _flag_holded = false;
            resetInterruptFlags();
            return true;
        }
        if (btnState && _flag_holded) {
            resetInterruptFlags();
            _flag_holded = false;
        }
        return false;
    }

    // Проверка, удерживается ли кнопка
    // - всегда возвращает тру после 15 секунд удержания (по умолчанию)
    // - длительность удержания равна длительности holded
    bool isHolded2() {
        bool btnState = getButtonState();

        // Если кнопка нажата и флаг не установлен, начинаем отсчет
        if (!btnState && !_flag_held_after && millis() - _tmr_held_after >= _debounceTime) {
            _flag_held_after = true;
            _tmr_held_after = millis();
        }
        // Если кнопка удерживается и прошло 10 секунд, возвращаем true
        if (!btnState && _flag_held_after && millis() - _tmr_held_after >= _holdedTime) {
            return true;
        }
        // Если кнопка отпущена, сбрасываем флаг
        if (btnState && _flag_held_after) {
            resetInterruptFlags();
            _flag_held_after = false;
            _tmr_held_after = millis();
        }
        return false;
    }

    // Проверка, удерживается ли кнопка
    //
    //---
    //- forcePcf - прочитать состояние кнопки с пцф напрямую
    //- - не вызывать в лупе, зависнит i2c
    //- - -
    //- всегда возвращает тру после 15 секунд удержания (по умолчанию)
    //- - длительность удержания равна длительности holdedTime
    bool isHolded(bool forcePcf = false) {
        bool btnState = getButtonState();

        // При первом запуске или forcePcf == true принудительно читаем состояние пина
        // так как читаем пин только по прерыванию (когда есть изменение статуса) то при запуске, если кнопка удерживается все равно вернет фелс
        // static bool firstRun = true;
        // if (firstRun && _type == PCF3_btn || forcePcf && _type == PCF3_btn) {
        //     btnState = pcf3.read(_pin);
        //     firstRun = false;
        // }

        // Если кнопка нажата и флаг не установлен, начинаем отсчет
        if (!btnState && !_flag_held_after && millis() - _tmr_held_after >= _debounceTime) {
            _flag_held_after = true;
            _tmr_held_after = millis();
        }
        // Если кнопка удерживается и прошло заданное время (_holdedTime), возвращаем true
        if (!btnState && _flag_held_after && millis() - _tmr_held_after >= _holdedTime) {
            return true;
        }
        // Если кнопка отпущена, сбрасываем флаг
        if (btnState && _flag_held_after) {
            resetInterruptFlags();
            _flag_held_after = false;
            _tmr_held_after = millis();
        }
        return false;
    }
    // Определение типа отпускания кнопки
    uint8_t release() {
        bool btnState = getButtonState();

        if (!btnState && !_flag_release && millis() - _tmr_release >= _debounceTime) {
            _flag_release = true;
            _tmr_release = millis();
        }
        if (btnState && _flag_release) {
            resetInterruptFlags();
            _flag_release = false;
            unsigned long duration = millis() - _tmr_release;
            if (duration >= _holdTime) return HOLD;
            if (duration >= _debounceTime) return CLICK;
            return PUSH;
        }
        return NO_ACTION;
    }

    // Проверка текущего состояния кнопки (нажата или нет)
    bool isPressed() {
        return !getButtonState();  // Инверсия, т.к. LOW = нажата
    }

   private:
    // Временные константы
    static const uint16_t DEBOUNCE_TIME = 50;       // Время антидребезга (мс)
    static const uint16_t HOLD_TIME = 1000;         // Время удержания (мс)
    static const uint16_t HOLDED_TIME = 15000;      // Время долгого удержания (мс)
    static const uint16_t HELD_AFTER_TIME = 10000;  // Время задержки (10 секунд) isHolded // неиспользуется

    uint8_t _pin;            // Номер пина
    uint8_t _type;           // Тип кнопки
    uint16_t _debounceTime;  // Настраиваемое время антидребезга
    uint16_t _holdTime;      // Настраиваемое время удержания
    uint16_t _holdedTime;    // Настраиваемое время долгого удержания

    unsigned long _tmr = 0;             // Таймер для click()
    bool _flag = false;                 // Флаг для click()
    unsigned long _tmr_hold = 0;        // Таймер для hold()
    bool _flag_hold = false;            // Флаг для hold()
    unsigned long _tmr_holded = 0;      // Таймер для holded()
    bool _flag_holded = false;          // Флаг для holded()
    unsigned long _tmr_held_after = 0;  // Таймер для isHolded()
    bool _flag_held_after = false;      // Флаг для isHolded()
    unsigned long _tmr_release = 0;     // Таймер для release()
    bool _flag_release = false;         // Флаг для release()

    // Получение состояния кнопки
    bool getButtonState() {
        bool btnState;
        switch (_type) {
            case PIN_btn:
            case PIN_btn_PULLUP:
                btnState = digitalRead(_pin);
                break;
            case PIN_btn_HIGH:
                btnState = !digitalRead(_pin);
                break;
            case PCF_btn:
                // btnState = PCF_InterruptFlag ? pcf.read(_pin) : HIGH;
                break;
                // case PCF3_btn:
                //     btnState = PCF3_btn_InterruptFlag ? pcf3.read(_pin) : HIGH;
                //     break;
            default:
                btnState = HIGH;  // По умолчанию не нажата
        }
        return btnState;
    }

    // Сброс флагов прерываний
    void resetInterruptFlags() {
        if (_type == PCF_btn) PCF_InterruptFlag = false;
        // if (_type == PCF3_btn) PCF3_btn_InterruptFlag = false;
    }
};

BUTTON knp_boot(PIN_btn_PULLUP, BUTTON_PIN);  //

class Gluonica {
   public:
    void InitialiseDisplay() {
        display.init(115200, true, 50, false);
        // display.init(); for older Waveshare HAT's
        DEBUG(SIMBOL_WAR, F("--------- INIT DISPLAY ---------"));

        SPI.end();
        SPI.begin();
        u8g2Fonts.begin(display);                   // connect u8g2 procedures to Adafruit GFX
        u8g2Fonts.setFontMode(1);                   // use u8g2 transparent mode (this is default)
        u8g2Fonts.setFontDirection(0);              // left to right (this is default)
        u8g2Fonts.setForegroundColor(GxEPD_BLACK);  // apply Adafruit GFX color
        u8g2Fonts.setBackgroundColor(GxEPD_WHITE);  // apply Adafruit GFX color
        display.fillScreen(GxEPD_WHITE);
        display.setFullWindow();
    }

    void Display() {
        DEBUG(SIMBOL_INFO, "[DISPLAY] Вывод на экран");

        display.fillScreen(GxEPD_WHITE);
        display.setFullWindow();

        updt_time();
        drawHeadingSection();             // Top line of the display
        drawMainWeatherSection(180, 70);  // Centre section of display for Location, temperature, Weather report, current Wx Symbol and wind direction
        drawForecastSection(233, 15);     // 3hr forecast boxes

        drawHomeSection();

        display.display(false);
    }

    void displayLogo() {
        u8g2Fonts.setFont(medium_gluon_font);
        display.drawBitmap(65, 90, logo, logo_w, logo_h, GxEPD_BLACK);
        drawString(65, 90 + 38, "ПОГОДНАЯ СТАНЦИЯ", LEFT);

        display.display(false);
    }
    void displayHost() {
        u8g2Fonts.setFont(big_gluon_font_20);

        display.drawBitmap(65 - 15, 90 + 38 + 44, wifi_bmp, wifi_w, wifi_h, GxEPD_BLACK);

        if (wifi.isAccessPoint()) {
            u8g2Fonts.setFont(big_gluon_font_20);
            drawString(65 + 38 - 15, 90 + 38 + 54, wifi.getSsid(), LEFT);

            u8g2Fonts.setFont(medium_gluon_font);
            drawString(65 + 38 - 15, 90 + 38 + 54 - 24, "ЗАПУЩЕНА ТОЧКА ДОСТУПА", LEFT);
            drawString(65 + 38 - 15, 90 + 38 + 54 + 22, "ПАРОЛЬ: " + String(settings.pass_ap), LEFT);

            display.display(true);
            delay(3000);
        } else {
            drawString(65 + 38 - 15, 90 + 38 + 54, "\x2A//:" + WiFi.localIP().toString(), LEFT);
            display.display(true);
            delay(1000);
        }
    }

    // #########################################################################################
    String windDegToDirection(float winddirection) {
        int dir = int((winddirection / 22.5) + 0.5);
        String Ord_direction[16] = {TXT_N, TXT_NNE, TXT_NE, TXT_ENE, TXT_E, TXT_ESE, TXT_SE, TXT_SSE, TXT_S, TXT_SSW, TXT_SW, TXT_WSW, TXT_W, TXT_WNW, TXT_NW, TXT_NNW};
        return Ord_direction[(dir % 16)];
    }

   private:
    // #########################################################################################
    void drawHeadingSection() {
        u8g2Fonts.setFont(medium_gluon_font);
        drawString(4, 0, time_str + "  " + settings.city_om, LEFT);
        drawString(SCREEN_WIDTH, 0, date_str, RIGHT);
    }
    // #########################################################################################
    void drawMainWeatherSection(int x, int y) {
        y += 4;
        displayDisplayWindSection(348, y - 6, WxConditions[0].Winddir, WxConditions[0].Windspeed, 40);
        displayWXicon(75, y + 4, WxConditions[0].Icon, LargeIcon);

        // u8g2Fonts.setFont(u8g2_font_7Segments_26x42_mn);//big_gluon_font_42
        u8g2Fonts.setFont(big_gluon_font_52);
        int8_t outside_temp = WxConditions[0].Temperature;
        drawTemperatureOutside(SCREEN_WIDTH / 2 + 8, y + 2, outside_temp);

        status.updateSensors();
        u8g2Fonts.setFont(medium_gluon_font);
        uint16_t pressure_mmHg = status.get_press();  // round(WxConditions[0].Pressure * 0.75006);

        drawPressureAndTrend(SCREEN_WIDTH / 2, y + 22, pressure_mmHg, WxConditions[0].Trend);  // тренд один фиг из погоды
    }
    // #########################################################################################
    void drawForecastSection(int x, int y) {
        // почасовой прогноз
        for (int r = 0; r < max_readings; r++) {
            hourly_rain_readings[r] = WxForecast[r].Rainfall;
            hourly_temperature_readings[r] = WxForecast[r].Temperature;
            //  hourly_snow_readings[r] = WxForecast[r].Snowfall;
            //  hourly_pressure_readings[r] = WxForecast[r].Pressure;
        }
        // подневной прогноз
        for (int r = 0; r < max_dayly_readings; r++) {
            daily_rain_readings[r] = Daily[r].Precipitation;
            // daily_snow_readings[r] = Daily[r].Snowfall;
            daily_max_temperature_readings[r] = round(Daily[r].High);
            daily_min_temperature_readings[r] = round(Daily[r].Low);
        }

        // display.drawLine(0, y + 172, SCREEN_WIDTH, y + 172, GxEPD_BLACK);
        //  u8g2Fonts.setFont(u8g2_font_cu12_t_cyrillic);
        //  drawString(SCREEN_WIDTH / 2, y + 180, TXT_FORECAST_VALUES, CENTER);
        // u8g2Fonts.setFont(u8g2_font_tom_thumb_4x6_mf );
        // u8g2Fonts.setFont(u8g2_font_6x13_t_cyrillic);

        drawTempChart(20, 134, 178, 90, 10, 30, TXT_TEMPERATURE_C, daily_max_temperature_readings, daily_min_temperature_readings, max_dayly_readings, autoscale_on, false);

        // display.drawFastVLine(199, 134, 90, GxEPD_BLACK);
        display.drawFastVLine(200, 134, 90, GxEPD_BLACK);
        display.drawFastVLine(201, 134, 90, GxEPD_BLACK);

        drawRainChart(200, 134, 175, 90, 0, 0, TXT_RAINFALL_MM, daily_rain_readings, max_dayly_readings, autoscale_on, true);
    }

    String getRainLevelStr(float mm) {
        if (mm < 1.0) return "";            // почти нет
        if (mm < 5.0) return "СЛАБЫЙ";      // 1–5 мм
        if (mm < 15.0) return "УМЕРЕННЫЙ";  // 5–15 мм
        if (mm < 30.0) return "СИЛЬНЫЙ";    // 15–30 мм
        return "ЛИВЕНЬ";                    // 30+ мм
    }
    //- 0 - почти нет
    //- 1 - СЛАБЫЙ
    //- 2 - УМЕРЕННЫЙ
    //- 3 - СИЛЬНЫЙ
    //- 4 - ЛИВЕНЬ
    uint8_t getRainLevel(float mm) {
        if (mm < 1.0) return 0;   // почти нет
        if (mm < 5.0) return 1;   // 1–5 мм
        if (mm < 15.0) return 2;  // 5–15 мм
        if (mm < 30.0) return 3;  // 15–30 мм
        return 4;                 // 30+ мм
    }

    String getPrecipDescription(float precip_mm, float snow_cm) {
        if (precip_mm < 0.5f && snow_cm < 0.5f) return "БЕЗ ОСАДКОВ";

        if (snow_cm >= 2.0f) {
            if (snow_cm >= 15.0f) return "СИЛЬНЫЙ СНЕГОПАД";
            if (snow_cm >= 8.0f) return "СНЕГОПАД";
            if (snow_cm >= 3.0f) return "УМЕРЕННЫЙ СНЕГ";
            return "СЛАБЫЙ СНЕГ";
        } else {
            // дождь или смешанные осадки
            if (precip_mm >= 30.0f) return "ЛИВЕНЬ";
            if (precip_mm >= 15.0f) return "СИЛЬНЫЙ ДОЖДЬ";
            if (precip_mm >= 5.0f) return "УМЕРЕННЫЙ ДОЖДЬ";
            if (precip_mm >= 1.0f) return "СЛАБЫЙ ДОЖДЬ";
            return "ДОЖДИК";
        }
    }

    // #########################################################################################
    void drawHomeSection() {
        drawHome(7, 295);
        // drawString(x, y, "\x27", LEFT, true);

        u8g2Fonts.setFont(big_gluon_font_42);
        int16_t pos_x = 67;

        int8_t inside_temp = status.get_temp();
        drawTemperatureInside(pos_x, 245 + 42, inside_temp);

        String tmp_str = String(inside_temp) + " \x24";
        int16_t w_x = u8g2Fonts.getUTF8Width(tmp_str.c_str());

        int8_t inside_hum = status.get_hum();
        drawHumunditiInside(w_x + pos_x, 245 + 42, inside_hum);

        u8g2Fonts.setFont(big_gluon_font_20);
        int16_t inside_co2 = status.get_co2();
        drawCo2Inside(400 - 6, 245 + 20, inside_co2);

        String co2str = "\x27 " + String(inside_co2) + " \x23";

        uint8_t w = u8g2Fonts.getUTF8Width(co2str.c_str());

        display.drawFastHLine(400 - 6 - w, 245 + 32, w + 1, GxEPD_RED);
        display.drawFastHLine(400 - 6 - w, 245 + 32 + 1, w + 1, GxEPD_RED);

        /////////////////////// ===================
        drawAstronomySection(400 - 11 - w, 274);
    }
    // x/y - координаты нижнего левого угла
    void drawHome(int x, int y) {
        y -= 42;
        // Время в секундах с начала эпохи (локальное, уже с учётом часового пояса)
        time_t now = NTP.getUnix();                // + settings.timeZone * 3600;
                                                   // time_t now = time(NULL);
        time_t sunrise = WxConditions[0].Sunrise;  // + WxConditions[0].Timezone;
        time_t sunset = WxConditions[0].Sunset;    // + WxConditions[0].Timezone;
                                                   //
                                                   // // Получаем сегодняшнюю полночь (начало текущих суток)
                                                   // struct tm* tm_now = localtime(&now);
                                                   // tm_now->tm_hour = 0;
                                                   // tm_now->tm_min = 0;
                                                   // tm_now->tm_sec = 0;
                                                   // time_t today_midnight = mktime(tm_now);
                                                   //
                                                   // now += WxConditions[0].Timezone;

        // Определяем состояние
        uint8_t night_state;
        DEBUG(SIMBOL_CUSTOM, "------ Время ------");
        DEBUG(SIMBOL_INFO, "NTP: ", ConvertUnixTime(now));
        DEBUG(SIMBOL_INFO, "weather: ", ConvertUnixTime(WxConditions[0].Dt));
        DEBUG(SIMBOL_INFO, "sunrise: ", ConvertUnixTime(sunrise));
        DEBUG(SIMBOL_INFO, "sunset: ", ConvertUnixTime(sunset));
        DEBUG(SIMBOL_CUSTOM, "-------------------");

        if (now >= sunrise && now < sunset) {
            night_state = 0;  // День
            DEBUG("[День]");
        } else if (now >= sunset) {
            night_state = 1;  // После заката (вечер + ночь до полуночи)
            DEBUG("[Вечер]");
        } else {
            // now < sunrise  →  это уже после полуночи сегодняшнего дня
            night_state = 2;  // Раннее утро / до восхода
            DEBUG("[Ночь]");
        }

        bool icy_cold = (WxConditions[0].Temperature < 0);
        bool heat = (WxConditions[0].Temperature > 25);
        bool stuffy = (status.get_co2() > 1000);
        bool is_new_year = false;

        icy_cold ? y -= 10 : y += 0;

        if (icy_cold) {
            switch (night_state) {
                    // день
                case 0: {
                    if (stuffy)
                        display.drawBitmap(x, y, home_cold_day_stuffy, home_cold_w, home_cold_h, GxEPD_BLACK);
                    else
                        display.drawBitmap(x, y, home_cold_day, home_cold_w, home_cold_h, GxEPD_BLACK);
                } break;
                // вечер
                case 1: {
                    if (stuffy)
                        display.drawBitmap(x, y, home_cold_evening_stuffy, home_cold_w, home_cold_h, GxEPD_BLACK);
                    else
                        display.drawBitmap(x, y, home_cold_evening, home_cold_w, home_cold_h, GxEPD_BLACK);
                } break;
                // ночь
                case 2: {
                    if (stuffy)
                        display.drawBitmap(x, y, home_cold_night_stuffy, home_cold_w, home_cold_h, GxEPD_BLACK);
                    else
                        display.drawBitmap(x, y, home_cold_night, home_cold_w, home_cold_h, GxEPD_BLACK);
                } break;
                default:
                    break;
            }

            if (is_new_year) display.drawBitmap(x, y, new_year, new_year_w, new_year_h, GxEPD_RED);
        } else {
            switch (night_state) {
                    // день
                case 0: {
                    if (heat) {
                        display.drawBitmap(x, y - 10, beach_heat, beach_heat_w, beach_heat_h, GxEPD_BLACK);
                        break;
                    }
                    if (stuffy)
                        display.drawBitmap(x, y, home_day_stuffy, home_w, home_h, GxEPD_BLACK);
                    else
                        display.drawBitmap(x, y, home_day, home_w, home_h, GxEPD_BLACK);

                } break;
                // вечер
                case 1: {
                    if (stuffy)
                        display.drawBitmap(x, y, home_evening_stuffy, home_w, home_h, GxEPD_BLACK);
                    else
                        display.drawBitmap(x, y, home_evening, home_w, home_h, GxEPD_BLACK);
                } break;
                // ночь
                case 2: {
                    if (stuffy)
                        display.drawBitmap(x, y, home_night_stuffy, home_w, home_h, GxEPD_BLACK);
                    else
                        display.drawBitmap(x, y, home_night, home_w, home_h, GxEPD_BLACK);
                } break;
                default:
                    break;
            }

            if (is_new_year) display.drawBitmap(x, y - 10, new_year, new_year_w, new_year_h, GxEPD_RED);
        }
    }
    // #########################################################################################

    // #########################################################################################
    void displayDisplayWindSection(int x, int y, float angle, float windspeed, int Cradius) {
        arrow(x, y, Cradius - 24, angle, 10, 12, true);  // Show wind direction on outer circle of width and length
        u8g2Fonts.setFont(gluon_font);
        int dxo, dyo, dxi, dyi;

        // display.fillCircle(x, y, Cradius + 1, GxEPD_BLACK); // Draw compass circle
        // display.fillCircle(x, y, Cradius - 2, GxEPD_WHITE); // Draw compass circle

        // display.drawCircle(x, y, Cradius - 1, GxEPD_BLACK); // Draw compass circle
        // display.drawCircle(x, y, Cradius, GxEPD_BLACK);     // Draw compass circle
        // display.drawCircle(x, y, Cradius + 1, GxEPD_BLACK); // Draw compass circle

        display.fillCircle(x, y, Cradius * 0.7, GxEPD_BLACK);      // Draw compass circle
        display.fillCircle(x, y, Cradius * 0.7 - 2, GxEPD_WHITE);  // Draw compass circle

        display.drawCircle(x, y, Cradius * 0.7 - 6, GxEPD_BLACK);  // Draw compass inner circle
        // display.drawCircle(x, y, Cradius * 0.7, GxEPD_BLACK); // Draw compass inner circle
        // display.drawCircle(x, y, Cradius * 0.7 - 2, GxEPD_BLACK); // Draw compass inner circle

        for (float a = 0; a < 360; a = a + 22.5) {
            dxo = Cradius * cos((a - 90) * PI / 180);
            dyo = Cradius * sin((a - 90) * PI / 180);
            dxi = dxo * 0.9;
            dyi = dyo * 0.9;
            display.drawLine(dxo + x, dyo + y, dxi + x, dyi + y, GxEPD_BLACK);
            switch ((uint16_t)a) {
                case 0:
                    display.drawLine(dxo + x + 1, dyo + y + 2, dxi + x + 1, dyi + y + 2, GxEPD_BLACK);
                    break;
                case 90:
                    display.drawLine(dxo + x - 2, dyo + y - 1, dxi + x - 2, dyi + y - 1, GxEPD_BLACK);
                    break;
                case 180:
                    display.drawLine(dxo + x - 1, dyo + y - 2, dxi + x - 1, dyi + y - 2, GxEPD_BLACK);
                    break;
                case 270:
                    display.drawLine(dxo + x + 2, dyo + y + 1, dxi + x + 2, dyi + y + 1, GxEPD_BLACK);
                    break;

                default:
                    break;
            }
            dxo = dxo * 0.7;
            dyo = dyo * 0.7;
            dxi = dxo * 0.9;
            dyi = dyo * 0.9;
            display.drawLine(dxo + x, dyo + y, dxi + x, dyi + y, GxEPD_BLACK);
        }
        drawString(x, y - Cradius - 11, TXT_N, CENTER);
        drawString(x, y + Cradius + 2, TXT_S, CENTER);
        drawString(x - Cradius - 6, y - 5, TXT_W, CENTER);
        drawString(x + Cradius + 6, y - 6, TXT_E, CENTER);
        // drawString(x - 2, y - 20, windDegToDirection(angle), CENTER);
        // drawString(x + 3, y + 12, String(angle, 0) + "°", CENTER);
        u8g2Fonts.setFont(medium_gluon_font);
        drawString(x, y - 4, String(round(windspeed), 0) + " м/с", CENTER, true);
    }

    // #########################################################################################
    void drawTemperatureOutside(int x, int y, int8_t temp) {
        String tmpStr = temp > 0 ? "+" + String(temp) : temp;
        drawString(x, y, tmpStr + "\x24", CENTER, true);
    }
    // #########################################################################################
    void drawPressureAndTrend(int x, int y, uint16_t pressure, String slope) {
        String text = String(pressure) + " мм";
        drawString(x, y, text, CENTER);

        uint16_t w = u8g2Fonts.getUTF8Width(text.c_str());

        // x = x + 44;
        x += w / 2 + 8;
        y += 4;

        if (slope == "+") {
            display.drawLine(x, y, x + 4, y - 4, GxEPD_BLACK);
            display.drawLine(x + 1, y, x + 4 + 1, y - 4, GxEPD_BLACK);

            display.drawLine(x + 4, y - 4, x + 8, y, GxEPD_BLACK);
            display.drawLine(x + 4 + 1, y - 4, x + 8 + 1, y, GxEPD_BLACK);
        } else if (slope == "0") {
            display.drawLine(x + 4, y - 4, x + 8, y, GxEPD_BLACK);
            display.drawLine(x + 4 + 1, y - 4, x + 8 + 1, y, GxEPD_BLACK);

            display.drawLine(x + 4, y + 4, x + 8, y, GxEPD_BLACK);
            display.drawLine(x + 4 + 1, y + 4, x + 8 + 1, y, GxEPD_BLACK);
        } else if (slope == "-") {
            display.drawLine(x, y, x + 4, y + 4, GxEPD_BLACK);
            display.drawLine(x + 1, y, x + 4 + 1, y + 4, GxEPD_BLACK);

            display.drawLine(x + 4, y + 4, x + 8, y, GxEPD_BLACK);
            display.drawLine(x + 4 + 1, y + 4, x + 8 + 1, y, GxEPD_BLACK);
        }
    }
    // #########################################################################################
    void displayPrecipitationSection(int x, int y) {
        display.drawRect(x, y - 1, 167, 56, GxEPD_BLACK);  // precipitation outline
        // u8g2Fonts.setFont(u8g2_font_6x13_t_cyrillic);
        if (WxForecast[1].Rainfall > 0.005) {                                           // Ignore small amounts
            drawString(x + 5, y + 15, String(WxForecast[1].Rainfall, 2) + "mm", LEFT);  // Only display rainfall total today if > 0
            addraindrop(x + 65, y + 16, 7);
        }
        if (WxForecast[1].Snowfall > 0.005)                                                      // Ignore small amounts
            drawString(x + 5, y + 35, String(WxForecast[1].Snowfall, 2) + "mm" + " * *", LEFT);  // Only display snowfall total today if > 0
    }
    // #########################################################################################
    void drawAstronomySection(int x, int y) {
        u8g2Fonts.setFont(gluon_font);
        //+ WxConditions[0].Timezone
        drawString(400 - 6, y + 4, ConvertUnixTime(WxConditions[0].Sunrise).substring(0, 5) + " " + TXT_SUNRISE, RIGHT);
        drawString(400 - 6, y + 11, ConvertUnixTime(WxConditions[0].Sunset).substring(0, 5) + " " + TXT_SUNSET, RIGHT);
        // time_t now = time(NULL);
        //
        // struct tm* now_utc = gmtime(&now);
        // const int day_utc = now_utc->tm_mday;
        // const int month_utc = now_utc->tm_mon + 1;
        // const int year_utc = now_utc->tm_year + 1900;

        const int day_utc = day;
        const int month_utc = month;
        const int year_utc = year;

        drawString(400 - 6, y + 18, moonPhase(day_utc, month_utc, year_utc), RIGHT);

        int8_t _hemisphere = settings.latitude > 0 ? NORTH : SOUTH;  //

        drawMoon(x, y, day_utc, month_utc, year_utc, settings.internet_time == 0 ? _hemisphere : settings.hemisphere);
    }
    // #########################################################################################
    void drawMoon(int x, int y, int dd, int mm, int yy, int8_t hemisphere) {
        const int diameter = 16;
        double Phase = NormalizedMoonPhase(dd, mm, yy);

        if (hemisphere == SOUTH) Phase = 1 - Phase;
        // Draw dark part of moon
        display.fillCircle(x + diameter - 1, y + diameter, diameter / 2 + 1, GxEPD_RED);
        const int number_of_lines = 90;
        for (double Ypos = 0; Ypos <= 45; Ypos++) {
            double Xpos = sqrt(45 * 45 - Ypos * Ypos);
            // Determine the edges of the lighted part of the moon
            double Rpos = 2 * Xpos;
            double Xpos1, Xpos2;
            if (Phase < 0.5) {
                Xpos1 = -Xpos;
                Xpos2 = (Rpos - 2 * Phase * Rpos - Xpos);
            } else {
                Xpos1 = Xpos;
                Xpos2 = (Xpos - 2 * Phase * Rpos + Rpos);
            }
            // Draw light part of moon
            double pW1x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
            double pW1y = (number_of_lines - Ypos) / number_of_lines * diameter + y;
            double pW2x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
            double pW2y = (number_of_lines - Ypos) / number_of_lines * diameter + y;
            double pW3x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
            double pW3y = (Ypos + number_of_lines) / number_of_lines * diameter + y;
            double pW4x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
            double pW4y = (Ypos + number_of_lines) / number_of_lines * diameter + y;
            display.drawLine(pW1x, pW1y, pW2x, pW2y, GxEPD_WHITE);
            display.drawLine(pW3x, pW3y, pW4x, pW4y, GxEPD_WHITE);
        }
        display.drawCircle(x + diameter - 1, y + diameter, diameter / 2 + 1, GxEPD_RED);
    }
    // #########################################################################################
    String moonPhase(int d, int m, int y) {
        int c, e;
        double jd;
        int b;
        if (m < 3) {
            y--;
            m += 12;
        }
        ++m;
        c = 365.25 * y;
        e = 30.6 * m;
        jd = c + e + d - 694039.09; /* jd is total days elapsed */
        jd /= 29.53059;             /* divide by the moon cycle (29.53 days) */
        b = jd;                     /* int(jd) -> b, take integer part of jd */
        jd -= b;                    /* subtract integer part to leave fractional part of original jd */
        b = jd * 8 + 0.5;           /* scale fraction from 0-8 and round by adding 0.5 */
        b = b & 7;                  /* 0 and 8 are the same phase so modulo 8 for 0 */

        int8_t _hemisphere = settings.latitude > 0 ? NORTH : SOUTH;  //
        if (settings.internet_time != 0) _hemisphere = settings.hemisphere;

        if (_hemisphere == SOUTH) b = 7 - b;
        if (b == 0) return TXT_MOON_NEW;              // New;              0%  illuminated
        if (b == 1) return TXT_MOON_WAXING_CRESCENT;  // Waxing crescent; 25%  illuminated
        if (b == 2) return TXT_MOON_FIRST_QUARTER;    // First quarter;   50%  illuminated
        if (b == 3) return TXT_MOON_WAXING_GIBBOUS;   // Waxing gibbous;  75%  illuminated
        if (b == 4) return TXT_MOON_FULL;             // Full;            100% illuminated
        if (b == 5) return TXT_MOON_WANING_GIBBOUS;   // Waning gibbous;  75%  illuminated
        if (b == 6) return TXT_MOON_THIRD_QUARTER;    // Third quarter;   50%  illuminated
        if (b == 7) return TXT_MOON_WANING_CRESCENT;  // Waning crescent; 25%  illuminated
        return "";
    }
    // #########################################################################################
    void arrow(int x, int y, int asize, float aangle, int pwidth, int plength, bool red) {
        float dx = (asize + 28) * cos((aangle - 90) * PI / 180) + x;  // calculate X position
        float dy = (asize + 28) * sin((aangle - 90) * PI / 180) + y;  // calculate Y position
        float x1 = 0;
        float y1 = plength;
        float x2 = pwidth / 2;
        float y2 = pwidth / 2;
        float x3 = -pwidth / 2;
        float y3 = pwidth / 2;
        float angle = aangle * PI / 180;
        float xx1 = x1 * cos(angle) - y1 * sin(angle) + dx;
        float yy1 = y1 * cos(angle) + x1 * sin(angle) + dy;
        float xx2 = x2 * cos(angle) - y2 * sin(angle) + dx;
        float yy2 = y2 * cos(angle) + x2 * sin(angle) + dy;
        float xx3 = x3 * cos(angle) - y3 * sin(angle) + dx;
        float yy3 = y3 * cos(angle) + x3 * sin(angle) + dy;
        display.fillTriangle(xx1, yy1, xx3, yy3, xx2, yy2, red ? GxEPD_RED : GxEPD_BLACK);
    }
    // #########################################################################################
    void displayWXicon(int x, int y, String IconName, bool IconSize) {
        DEBUG(IconName);
        if (IconName == "01d" || IconName == "01n")
            Sunny(x, y, IconSize, IconName);
        else if (IconName == "02d" || IconName == "02n")
            MostlySunny(x, y, IconSize, IconName);
        else if (IconName == "03d" || IconName == "03n")
            Cloudy(x, y, IconSize, IconName);
        else if (IconName == "04d" || IconName == "04n")
            MostlyCloudy(x, y, IconSize, IconName);
        else if (IconName == "09d" || IconName == "09n")
            ChanceRain(x, y, IconSize, IconName);
        else if (IconName == "10d" || IconName == "10n")
            Rain(x, y, IconSize, IconName);
        else if (IconName == "11d" || IconName == "11n")
            Tstorms(x, y, IconSize, IconName);
        else if (IconName == "13d" || IconName == "13n")
            Snow(x, y, IconSize, IconName);
        else if (IconName == "50d")
            Haze(x, y, IconSize, IconName);
        else if (IconName == "50n")
            Fog(x, y, IconSize, IconName);
        else
            Nodata(x, y, IconSize, IconName);
    }

    // #########################################################################################
    void drawTemperatureInside(int x, int y, int8_t temp) {
        drawString(x, y, String(temp) + "\x24", LEFT, true);
    }
    void drawHumunditiInside(int x, int y, int8_t hum) {
        drawString(x, y, String(hum) + "\x25", LEFT, false);
    }
    void drawCo2Inside(int x, int y, int16_t co_2) {
        drawString(x, y, "\x27 " + String(co_2) + " \x23", RIGHT, false);
    }

    // #########################################################################################
    void drawBattery(int x, int y) {
        uint8_t percentage = 100;
        float voltage = analogRead(35) / 4096.0 * 7.46;
        if (voltage > 1) {  // Only display if there is a valid reading
            DEBUG("Voltage = " + String(voltage));
            percentage = 2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) + 255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303;
            if (voltage >= 4.20) percentage = 100;
            if (voltage <= 3.50) percentage = 0;
            display.drawRect(x + 15, y - 12, 19, 10, GxEPD_BLACK);
            display.fillRect(x + 34, y - 10, 2, 5, GxEPD_BLACK);
            display.fillRect(x + 17, y - 10, 15 * percentage / 100.0, 6, GxEPD_BLACK);
            drawString(x + 65, y - 11, String(percentage) + "%", RIGHT);
            // drawString(x + 13, y + 5,  String(voltage, 2) + "v", CENTER);
        }
    }
    // #########################################################################################

    /* (C) D L BIRD
        Эта функция рисует график на бумажном/TFT/LCD дисплее, используя данные из массива, содержащего данные для отображения в виде графика.
        Переменная 'max_readings' определяет максимальное количество элементов данных для каждого массива. Вызовите его, используя следующие параметрические данные:
        x_pos - положение оси x в верхнем левом углу графика
        y_pos - положение графика в верхнем левом углу по оси y, например, 100, 200, при этом график будет отображаться на 100 пикселей вдоль и на 200 пикселей вниз от верхнего левого края экрана
        ширина - ширина графика в пикселях
        height - высота графика в пикселях
        Y1_Max - устанавливает масштаб отображаемых данных, например, значение 5000 означает масштабирование всех данных по оси Y не более чем на 5000
        data_array1 анализируется по значению, внешне они могут называться как угодно иначе, например, внутри процедуры это называется data_array1, но внешне может быть temperature_readings
        auto_scale - логическое значение (TRUE или FALSE), которое включает или выключает автоматическое масштабирование по оси Y
        barchart_on - логическое значение (TRUE или FALSE), которое переключает режим рисования между столбчатыми и линейчатыми графиками
        barchart_colour - задает цвет заголовка и построения графика
        Если вызывается со значением Y!_Max равным 500 и данные никогда не превышают 500, то автоматическое масштабирование сохранит масштаб 0-500 Y, если включено, масштаб увеличивается / уменьшается в соответствии с данными.
        auto_scale_margin, например, если установлено значение 1000, то автоматическое масштабирование увеличивает масштаб на 1000 шагов.
    */
    void drawRainChart(int x_pos, int y_pos, int gwidth, int gheight, float Y1Min, float Y1Max, String title, float DataArray[], int readings, boolean auto_scale, bool right) {
        float current_percip = Daily[0].Precipitation;
        float current_snow = Daily[0].Snowfall;
        String percip_str = getPrecipDescription(current_percip, current_snow);
        u8g2Fonts.setFont(medium_gluon_font);
        drawString(x_pos + 4, y_pos - 10, percip_str, LEFT);

#define auto_scale_margin 0  // Sets the autoscale increment, so axis steps up in units of e.g. 3
#define y_minor_axis 5       // 5 y-axis division markers
        float maxYscale = -10000;
        float minYscale = 10000;
        int last_x, last_y;
        float x1, y1, x2, y2;
        if (auto_scale == true) {
            for (int i = 0; i < readings; i++) {
                if (DataArray[i] >= maxYscale) maxYscale = DataArray[i];
                if (DataArray[i] <= minYscale) minYscale = DataArray[i];
            }
            maxYscale = round(maxYscale + auto_scale_margin);
            Y1Max = round(maxYscale + 0.5);
            if (minYscale != 0) minYscale = round(minYscale - auto_scale_margin);
            Y1Min = round(minYscale);
        }
        // Draw the graph
        last_x = x_pos + 1;
        last_y = y_pos + (Y1Max - constrain(DataArray[1], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight;

        u8g2Fonts.setFont(gluon_font);  //

        // Draw the data
        for (int gx = 0; gx < readings; gx++) {
            y2 = y_pos + (Y1Max - constrain(DataArray[gx], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight + 1;

            x2 = x_pos + gx * (gwidth / readings) + 4;
            // gx==0 ? x2 + 2 : x2
            if (DataArray[gx] >= 30.0) {  //  ливень y_pos + gheight

                uint16_t height_bar_rect = y_pos + gheight - y2 + 2;
                uint16_t height_bar_fill = height_bar_rect - 2;

                display.fillRect(x2 + 1, y2 + 1, (gwidth / readings) - 6, height_bar_fill, GxEPD_RED);
                display.drawRect(x2, y2, (gwidth / readings) - 4, height_bar_rect, GxEPD_BLACK);

                DEBUG(SIMBOL_INFO, "[CHART][" + String(gx) + "] Precipitation (mm): ", DataArray[gx]);
            } else if (DataArray[gx] >= 15.0) {  // сильный дождь
                uint16_t height_bar_rect = y_pos + gheight - y2 + 2;
                uint16_t height_bar_rect2 = height_bar_rect - 1;
                uint16_t height_bar_fill = height_bar_rect2 - 2;

                display.drawRect(x2, y2, (gwidth / readings) - 4, height_bar_rect, GxEPD_RED);
                display.drawRect(x2 + 1, y2 + 1, (gwidth / readings) - 6, height_bar_rect2, GxEPD_RED);
                display.fillRect(x2 + 2, y2 + 2, (gwidth / readings) - 8, height_bar_fill, GxEPD_BLACK);

                // display.drawRect(x2, y2+1, (gwidth / readings) - 4, y_pos + gheight - y2 + 2-1, GxEPD_RED);

                DEBUG(SIMBOL_INFO, "[CHART][" + String(gx) + "] Precipitation (mm): ", DataArray[gx]);
            } else if (DataArray[gx] >= 5.0) {  // умеренный дождь
                // display.fillRect(x2, y2, (gwidth / readings) - 4, y_pos + gheight - y2 + 2, GxEPD_BLACK);
                //  === ТВОЙ ПРЯМОУГОЛЬНИК (оставляем как было) ===

                uint16_t height_bar_rect = y_pos + gheight - y2 + 2;
                uint16_t height_bar_fill = height_bar_rect - 2;

                if (y2 > y_pos + gheight) {
                    y2 = (y_pos + gheight) - 3;
                    if (height_bar_rect > 3) height_bar_rect = 3;
                }

                display.drawRect(x2, y2, (gwidth / readings) - 4, height_bar_rect, GxEPD_BLACK);
                drawGridBar(x2, y2 + 1, (gwidth / readings) - 4 - 1, height_bar_fill, GxEPD_RED, 3);

                DEBUG(SIMBOL_INFO, "[CHART][" + String(gx) + "] Precipitation (mm): ", DataArray[gx]);

            } else if (DataArray[gx] >= 1.0) {  // слабый дождь
                // display.fillRect(x2, y2, (gwidth / readings) - 4, y_pos + gheight - y2 + 2, GxEPD_BLACK);
                //  === ТВОЙ ПРЯМОУГОЛЬНИК (оставляем как было) ===
                uint16_t height_bar_rect = y_pos + gheight - y2 + 2;
                uint16_t height_bar_fill = height_bar_rect - 2;

                if (y2 > y_pos + gheight) {
                    y2 = (y_pos + gheight) - 2;
                    if (height_bar_rect > 3) height_bar_rect = 3;
                }

                display.drawRect(x2, y2, (gwidth / readings) - 4, height_bar_rect, GxEPD_BLACK);
                drawGridBar(x2, y2 + 1, (gwidth / readings) - 4 - 1, height_bar_fill, GxEPD_BLACK, 8);

                DEBUG(SIMBOL_INFO, "[CHART][" + String(gx) + "] Precipitation (mm): ", DataArray[gx]);

            } else if (DataArray[gx] >= 0.1) {  // почти нет дождя

                uint16_t height_bar_rect = y_pos + gheight - y2 + 2;

                if (y2 > y_pos + gheight) {
                    y2 = (y_pos + gheight) - 2;
                    if (height_bar_rect > 3) height_bar_rect = 3;
                }

                display.drawRect(x2, y2, (gwidth / readings) - 4, height_bar_rect, GxEPD_BLACK);

                DEBUG(SIMBOL_INFO, "[CHART][" + String(gx) + "] Precipitation (mm): ", DataArray[gx]);
            }
            // display.drawRect(x2, y2, (gwidth / readings) - 4, y_pos + gheight - y2 + 2, GxEPD_BLACK);

            last_x = x2;
            last_y = y2;
        }
// Draw the Y-axis scale
#define number_of_dashes 30
        int16_t right_offset = 0;
        if (right)
            right_offset += (gwidth + 20);
        else
            right_offset = -4;

        for (int spacing = 0; spacing <= y_minor_axis; spacing++) {
            for (int j = 0; j < number_of_dashes; j++) {
                if (spacing < y_minor_axis) display.drawFastHLine((x_pos + 8 + j * gwidth / number_of_dashes), y_pos + (gheight * spacing / y_minor_axis), gwidth / (2 * number_of_dashes), GxEPD_BLACK);
            }
            if ((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing) < 5 || title == TXT_PRESSURE_IN) {
                drawString(x_pos + right_offset, y_pos + gheight * spacing / y_minor_axis - 5, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), 1), RIGHT);
                // DEBUG(SIMBOL_CUSTOM, "------------------------------------------------ 1 ",    String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01) ));
            } else {
                if (Y1Min < 1 && Y1Max < 10) {
                    drawString(x_pos + right_offset, y_pos + gheight * spacing / y_minor_axis - 5, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), 1), RIGHT);
                    // DEBUG(SIMBOL_CUSTOM, "------------------------------------------------ 2 ", String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01) ));
                } else {
                    drawString(x_pos + right_offset, y_pos + gheight * spacing / y_minor_axis - 5, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), 0), RIGHT);
                    // DEBUG(SIMBOL_CUSTOM, "------------------------------------------------ 3 ", String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01) ));
                }
            }
        }

        display.drawFastHLine(x_pos, y_pos + gheight, gwidth + 3, GxEPD_BLACK);
        display.drawFastHLine(x_pos, y_pos + gheight + 1, gwidth + 3, GxEPD_BLACK);

        uint8_t day_for_print = weekDay;
        for (int i = 1; i < 8; i++) {
            int step = gwidth / 6 - 4;
            int x_label = (x_pos + 14) + ((i - 1) * step);

            if (i > 1 && ++day_for_print > 7) {
                day_for_print = 1;
            }

            drawString(x_label, y_pos + gheight + 3, weekday_D_shorty[day_for_print], CENTER);
        }
    }
    void drawGridBar(int rectX, int rectY, int rectW, int rectH, uint16_t color, int step = 3) {
        // границы прямоугольника
        int xStart = rectX;
        int yStart = rectY;
        int xEnd = rectX + rectW;
        int yEnd = rectY + rectH;

        // 1. линии, начинающиеся сверху
        for (int x = xStart; x <= xEnd; x += step) {
            int x1 = x;
            int y1 = yStart;

            int x2 = x;
            int y2 = yStart;

            // ведём линию вниз-вправо пока не выйдем за границы
            while (x2 < xEnd && y2 < yEnd) {
                x2++;
                y2++;
            }

            display.drawLine(x1, y1, x2, y2, color);
        }

        // 2. линии, начинающиеся слева
        for (int y = yStart; y <= yEnd; y += step) {
            int x1 = xStart;
            int y1 = y;

            int x2 = xStart;
            int y2 = y;

            while (x2 < xEnd && y2 < yEnd) {
                x2++;
                y2++;
            }

            display.drawLine(x1, y1, x2, y2, color);
        }
    }

    void drawTempChartMax(int x_pos, int y_pos, int gwidth, int gheight, float Y1Min, float Y1Max, String title, int16_t DataArray[], int readings, boolean auto_scale, boolean barchart_mode, bool right = false) {
#define auto_scale_margin 0
#define y_minor_axis 5

        float maxYscale = -10000;
        float minYscale = 10000;

        // === Автоматическое масштабирование ===
        if (auto_scale == true) {
            for (int i = 0; i < readings; i++) {
                if (DataArray[i] > maxYscale) maxYscale = DataArray[i];
                if (DataArray[i] < minYscale) minYscale = DataArray[i];
            }
            Y1Max = round(maxYscale + auto_scale_margin + 0.5);
            if (minYscale != 0) minYscale = round(minYscale - auto_scale_margin);
            Y1Min = round(minYscale) - 2.0;
        }

        u8g2Fonts.setFont(gluon_font);

        // === Линейный режим — теперь просто линия по дням ===
        float last_x = x_pos + 1;
        float last_y = y_pos + (Y1Max - constrain(DataArray[0], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight + 1;

        for (int i = 1; i < readings; i++) {
            float x2 = x_pos + (float)i * gwidth / (readings - 1) + 1;
            float y2 = y_pos + (Y1Max - constrain(DataArray[i], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight + 1;

            // Красная линия (максимумы/значения)
            display.drawLine(last_x, last_y, x2, y2, GxEPD_RED);
            display.drawLine(last_x + 1, last_y, x2 + 1, y2, GxEPD_RED);

            last_x = x2;
            last_y = y2;
        }

        // === Шкала Y (ось ординат) ===
#define number_of_dashes 30
        int16_t right_offset = -4;

        for (int spacing = 0; spacing <= y_minor_axis; spacing++) {
            // Горизонтальные штрихи
            for (int j = 0; j < number_of_dashes; j++) {
                if (spacing < y_minor_axis) {
                    display.drawFastHLine(
                        x_pos + 8 + j * gwidth / number_of_dashes,
                        y_pos + (gheight * spacing / y_minor_axis),
                        gwidth / (2 * number_of_dashes),
                        GxEPD_BLACK);
                }
            }

            // Значение на шкале
            float raw_value = Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing;

            String label;
            int int_value = round(raw_value);
            label = String(int_value);

            drawString(x_pos + right_offset, y_pos + gheight * spacing / y_minor_axis - 5, label, RIGHT);
        }

        // === Нижняя линия графика ===
        display.drawFastHLine(x_pos, y_pos + gheight, gwidth + 3, GxEPD_BLACK);
        display.drawFastHLine(x_pos, y_pos + gheight + 1, gwidth + 3, GxEPD_BLACK);

        // === Подписи дней недели снизу ===
        uint8_t day_for_print = weekDay;

        for (int i = 1; i < 8; i++) {
            int step = gwidth / 6 - 4;
            int x_label = (x_pos + 14) + ((i - 1) * step);

            if (i > 1 && ++day_for_print > 7) {
                day_for_print = 1;
            }

            drawString(x_label, y_pos + gheight + 3, weekday_D_shorty[day_for_print], CENTER);
        }
    }

    void drawTempChart(int x_pos, int y_pos, int gwidth, int gheight, float Y1Min, float Y1Max, String title, int16_t DataArray[], int16_t DataMinArray[], int readings, boolean auto_scale, bool right = false) {
#define auto_scale_margin 0
#define y_minor_axis 5

        float maxYscale = -10000;
        float minYscale = 10000;

        // === Автоматическое масштабирование (по обоим массивам) ===
        if (auto_scale == true) {
            for (int i = 0; i < readings; i++) {
                if (DataArray[i] > maxYscale) maxYscale = DataArray[i];
                if (DataMinArray[i] > maxYscale) maxYscale = DataMinArray[i];
                if (DataArray[i] < minYscale) minYscale = DataArray[i];
                if (DataMinArray[i] < minYscale) minYscale = DataMinArray[i];
            }
            Y1Max = round(maxYscale + auto_scale_margin + 0.5);
            if (minYscale != 0) minYscale = round(minYscale - auto_scale_margin);
            Y1Min = round(minYscale) - 2.0;
        }

        u8g2Fonts.setFont(gluon_font);

        // === Рисуем две линии по дням ===

        float last_max_x = x_pos + 1;
        float last_max_y = y_pos + (Y1Max - constrain(DataArray[0], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight + 1;

        float last_min_x = x_pos + 1;
        float last_min_y = y_pos + (Y1Max - constrain(DataMinArray[0], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight + 1;

        for (int i = 1; i < readings; i++) {
            float x2 = x_pos + (float)i * gwidth / (readings - 1) + 1;

            float y_max = y_pos + (Y1Max - constrain(DataArray[i], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight + 1;
            float y_min = y_pos + (Y1Max - constrain(DataMinArray[i], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight + 1;

            // Красная линия — максимальная температура
            display.drawLine(last_max_x, last_max_y, x2, y_max, GxEPD_RED);
            display.drawLine(last_max_x + 1, last_max_y, x2 + 1, y_max, GxEPD_RED);

            // Чёрная линия — минимальная температура
            display.drawLine(last_min_x, last_min_y, x2, y_min, GxEPD_BLACK);
            display.drawLine(last_min_x + 1, last_min_y, x2 + 1, y_min, GxEPD_BLACK);

            last_max_x = x2;
            last_max_y = y_max;
            last_min_x = x2;
            last_min_y = y_min;
        }

        // === Шкала Y (ось ординат) ===
#define number_of_dashes 30
        int16_t right_offset = right ? (gwidth + 20) : -4;

        for (int spacing = 0; spacing <= y_minor_axis; spacing++) {
            // Горизонтальные штрихи сетки
            for (int j = 0; j < number_of_dashes; j++) {
                if (spacing < y_minor_axis) {
                    display.drawFastHLine(
                        x_pos + 8 + j * gwidth / number_of_dashes,
                        y_pos + (gheight * spacing / y_minor_axis),
                        gwidth / (2 * number_of_dashes),
                        GxEPD_BLACK);
                }
            }

            // Значение на шкале (всегда целое число для температуры)
            float raw_value = Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing;
            int16_t int_value = round(raw_value);
            String label = String(int_value);

            drawString(x_pos + right_offset,
                       y_pos + gheight * spacing / y_minor_axis - 5,
                       label, RIGHT);
        }

        // === Нижняя жирная линия графика ===
        display.drawFastHLine(x_pos, y_pos + gheight, gwidth + 3, GxEPD_BLACK);
        display.drawFastHLine(x_pos, y_pos + gheight + 1, gwidth + 3, GxEPD_BLACK);

        // === Подписи дней недели снизу ===
        uint8_t day_for_print = weekDay;

        for (int i = 1; i < 8; i++) {
            int step = gwidth / 6 - 4;
            int x_label = (x_pos + 14) + ((i - 1) * step);

            if (i > 1 && ++day_for_print > 7) {
                day_for_print = 1;
            }

            drawString(x_label, y_pos + gheight + 3, weekday_D_shorty[day_for_print], CENTER);
        }
    }

    // #########################################################################################
    void drawString(int x, int y, String text, alignment align, bool RED = false) {
        int16_t x1, y1;  // the bounds of x,y and w and h of the variable 'text' in pixels.
        uint16_t w, h;
        display.setTextWrap(false);
        display.getTextBounds(text, x, y, &x1, &y1, &w, &h);

        w = u8g2Fonts.getUTF8Width(text.c_str());
        if (align == RIGHT) x = x - w;
        if (align == CENTER) x = x - w / 2;
        u8g2Fonts.setCursor(x, y + h);

        if (RED) {
            u8g2Fonts.setForegroundColor(GxEPD_RED);
        }
        u8g2Fonts.print(text);
        u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    }
    // #########################################################################################
    void drawStringMaxWidth(int x, int y, unsigned int text_width, String text, alignment align) {
        int16_t x1, y1;  // the bounds of x,y and w and h of the variable 'text' in pixels.
        uint16_t w, h;
        display.getTextBounds(text, x, y, &x1, &y1, &w, &h);
        w = u8g2Fonts.getUTF8Width(text.c_str());

        if (align == RIGHT) x = x - w;
        if (align == CENTER) x = x - w / 2;
        u8g2Fonts.setCursor(x, y);
        if (text.length() > text_width * 2) {
            // u8g2Fonts.setFont(u8g2_font_6x13_t_cyrillic);
            text_width = 42;
            y = y - 3;
        }
        u8g2Fonts.println(text.substring(0, text_width));
        if (text.length() > text_width) {
            u8g2Fonts.setCursor(x, y + h + 15);
            String secondLine = text.substring(text_width);
            secondLine.trim();  // Remove any leading spaces
            u8g2Fonts.println(secondLine);
        }
    }

    // #########################################################################################
    //  Symbols are drawn on a relative 10x10grid and 1 scale unit = 1 drawing unit
    void addcloud(int x, int y, int scale, int linesize) {
        // Draw cloud outer
        display.fillCircle(x - scale * 3, y, scale, GxEPD_BLACK);                               // Left most circle
        display.fillCircle(x + scale * 3, y, scale, GxEPD_BLACK);                               // Right most circle
        display.fillCircle(x - scale, y - scale, scale * 1.4, GxEPD_BLACK);                     // left middle upper circle
        display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75, GxEPD_BLACK);        // Right middle upper circle
        display.fillRect(x - scale * 3 - 1, y - scale, scale * 6, scale * 2 + 1, GxEPD_BLACK);  // Upper and lower lines
        // Clear cloud inner
        display.fillCircle(x - scale * 3, y, scale - linesize, GxEPD_WHITE);                                                    // Clear left most circle
        display.fillCircle(x + scale * 3, y, scale - linesize, GxEPD_WHITE);                                                    // Clear right most circle
        display.fillCircle(x - scale, y - scale, scale * 1.4 - linesize, GxEPD_WHITE);                                          // left middle upper circle
        display.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75 - linesize, GxEPD_WHITE);                             // Right middle upper circle
        display.fillRect(x - scale * 3 + 2, y - scale + linesize - 1, scale * 5.9, scale * 2 - linesize * 2 + 2, GxEPD_WHITE);  // Upper and lower lines
    }
    // #########################################################################################
    void addraindrop(int x, int y, int scale) {
        display.fillCircle(x, y, scale / 2, GxEPD_BLACK);
        display.fillTriangle(x - scale / 2, y, x, y - scale * 1.2, x + scale / 2, y, GxEPD_BLACK);
        x = x + scale * 1.6;
        y = y + scale / 3;
        display.fillCircle(x, y, scale / 2, GxEPD_BLACK);
        display.fillTriangle(x - scale / 2, y, x, y - scale * 1.2, x + scale / 2, y, GxEPD_BLACK);
    }
    // #########################################################################################
    void addrain(int x, int y, int scale, bool IconSize) {
        if (IconSize == SmallIcon) scale *= 1.34;
        for (int d = 0; d < 4; d++) {
            addraindrop(x + scale * (7.8 - d * 1.95) - scale * 5.2, y + scale * 2.1 - scale / 6, scale / 1.6);
        }
    }
    // #########################################################################################
    void addsnow(int x, int y, int scale, bool IconSize) {
        int dxo, dyo, dxi, dyi;
        for (int flakes = 0; flakes < 5; flakes++) {
            for (int i = 0; i < 360; i = i + 45) {
                dxo = 0.5 * scale * cos((i - 90) * 3.14 / 180);
                dxi = dxo * 0.1;
                dyo = 0.5 * scale * sin((i - 90) * 3.14 / 180);
                dyi = dyo * 0.1;
                display.drawLine(dxo + x + flakes * 1.5 * scale - scale * 3, dyo + y + scale * 2, dxi + x + 0 + flakes * 1.5 * scale - scale * 3, dyi + y + scale * 2, GxEPD_BLACK);
            }
        }
    }
    // #########################################################################################
    void addtstorm(int x, int y, int scale) {
        y = y + scale / 2;
        for (int i = 0; i < 5; i++) {
            display.drawLine(x - scale * 4 + scale * i * 1.5 + 0, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 0, y + scale, GxEPD_BLACK);
            if (scale != Small) {
                display.drawLine(x - scale * 4 + scale * i * 1.5 + 1, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 1, y + scale, GxEPD_BLACK);
                display.drawLine(x - scale * 4 + scale * i * 1.5 + 2, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 2, y + scale, GxEPD_BLACK);
            }
            display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 0, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 0, GxEPD_BLACK);
            if (scale != Small) {
                display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 1, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 1, GxEPD_BLACK);
                display.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 2, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 2, GxEPD_BLACK);
            }
            display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 0, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5, GxEPD_BLACK);
            if (scale != Small) {
                display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 1, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 1, y + scale * 1.5, GxEPD_BLACK);
                display.drawLine(x - scale * 3.5 + scale * i * 1.4 + 2, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 2, y + scale * 1.5, GxEPD_BLACK);
            }
        }
    }
    // #########################################################################################
    void addsun(int x, int y, int scale, bool IconSize) {
        int linesize = 3;
        if (IconSize == SmallIcon) linesize = 1;
        display.fillRect(x - scale * 2, y, scale * 4, linesize, GxEPD_RED);
        display.fillRect(x, y - scale * 2, linesize, scale * 4, GxEPD_RED);
        display.drawLine(x - scale * 1.3, y - scale * 1.3, x + scale * 1.3, y + scale * 1.3, GxEPD_RED);
        display.drawLine(x - scale * 1.3, y + scale * 1.3, x + scale * 1.3, y - scale * 1.3, GxEPD_RED);
        if (IconSize == LargeIcon) {
            display.drawLine(1 + x - scale * 1.3, y - scale * 1.3, 1 + x + scale * 1.3, y + scale * 1.3, GxEPD_RED);
            display.drawLine(2 + x - scale * 1.3, y - scale * 1.3, 2 + x + scale * 1.3, y + scale * 1.3, GxEPD_RED);
            display.drawLine(3 + x - scale * 1.3, y - scale * 1.3, 3 + x + scale * 1.3, y + scale * 1.3, GxEPD_RED);
            display.drawLine(1 + x - scale * 1.3, y + scale * 1.3, 1 + x + scale * 1.3, y - scale * 1.3, GxEPD_RED);
            display.drawLine(2 + x - scale * 1.3, y + scale * 1.3, 2 + x + scale * 1.3, y - scale * 1.3, GxEPD_RED);
            display.drawLine(3 + x - scale * 1.3, y + scale * 1.3, 3 + x + scale * 1.3, y - scale * 1.3, GxEPD_RED);
        }
        display.fillCircle(x, y, scale * 1.3, GxEPD_WHITE);
        display.fillCircle(x, y, scale, GxEPD_RED);
        display.fillCircle(x, y, scale - linesize, GxEPD_WHITE);
    }
    // #########################################################################################
    void addfog(int x, int y, int scale, int linesize, bool IconSize) {
        if (IconSize == SmallIcon) {
            y -= 10;
            linesize = 1;
        }
        for (int i = 0; i < 6; i++) {
            display.fillRect(x - scale * 3, y + scale * 1.5, scale * 6, linesize, GxEPD_BLACK);
            display.fillRect(x - scale * 3, y + scale * 2.0, scale * 6, linesize, GxEPD_BLACK);
            display.fillRect(x - scale * 3, y + scale * 2.5, scale * 6, linesize, GxEPD_BLACK);
        }
    }
    // #########################################################################################
    void Sunny(int x, int y, bool IconSize, String IconName) {
        int scale = Small, offset = 3;
        if (IconSize == LargeIcon) {
            scale = Large;
            y = y - 8;
            offset = 18;
        } else
            y = y - 3;  // Shift up small sun icon
        if (IconName.endsWith("n")) addmoon(x, y + offset, scale, IconSize);
        scale = scale * 1.6;
        addsun(x, y, scale, IconSize);
    }
    // #########################################################################################
    void MostlySunny(int x, int y, bool IconSize, String IconName) {
        int scale = Small, linesize = 3, offset = 3;
        if (IconSize == LargeIcon) {
            scale = Large;
            offset = 10;
        } else
            linesize = 1;

        if (IconName.endsWith("n")) addmoon(x, y + offset, scale, IconSize);
        addcloud(x, y + offset, scale, linesize);
        addsun(x - scale * 1.8, y - scale * 1.8 + offset, scale + 3, IconSize);
    }
    // #########################################################################################
    void MostlyCloudy(int x, int y, bool IconSize, String IconName) {
        int scale = Large, linesize = 3;
        if (IconSize == SmallIcon) {
            scale = Small;
            linesize = 1;
        }
        if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
        addcloud(x, y, scale, linesize);
        addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
        addcloud(x, y, scale, linesize);
    }
    // #########################################################################################
    void Cloudy(int x, int y, bool IconSize, String IconName) {
        int scale = Large, linesize = 3;
        if (IconSize == SmallIcon) {
            scale = Small;
            if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
            linesize = 1;
            addcloud(x, y, scale, linesize);
        } else {
            y += 10;
            if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
            addcloud(x + 30, y - 35, 5, linesize);  // Cloud top right
            addcloud(x - 20, y - 25, 7, linesize);  // Cloud top left
            addcloud(x, y, scale, linesize);        // Main cloud
        }
    }
    // #########################################################################################
    void Rain(int x, int y, bool IconSize, String IconName) {
        int scale = Large, linesize = 3;
        if (IconSize == SmallIcon) {
            scale = Small;
            linesize = 1;
        }
        if (IconName.endsWith("n")) addmoon(x, y + 10, scale, IconSize);
        addcloud(x, y, scale, linesize);
        addrain(x, y, scale, IconSize);
    }
    // #########################################################################################
    void ExpectRain(int x, int y, bool IconSize, String IconName) {
        int scale = Large, linesize = 3;
        if (IconSize == SmallIcon) {
            scale = Small;
            linesize = 1;
        }
        if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
        addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
        addcloud(x, y, scale, linesize);
        addrain(x, y, scale, IconSize);
    }
    // #########################################################################################
    void ChanceRain(int x, int y, bool IconSize, String IconName) {
        int scale = Large, linesize = 3;
        if (IconSize == SmallIcon) {
            scale = Small;
            linesize = 1;
        }
        if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
        addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
        addcloud(x, y, scale, linesize);
        addrain(x, y, scale, IconSize);
    }
    // #########################################################################################
    void Tstorms(int x, int y, bool IconSize, String IconName) {
        int scale = Large, linesize = 3;
        if (IconSize == SmallIcon) {
            scale = Small;
            linesize = 1;
        }
        if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
        addcloud(x, y, scale, linesize);
        addtstorm(x, y, scale);
    }
    // #########################################################################################
    void Snow(int x, int y, bool IconSize, String IconName) {
        int scale = Large, linesize = 3;
        if (IconSize == SmallIcon) {
            scale = Small;
            linesize = 1;
        }
        if (IconName.endsWith("n")) addmoon(x, y + 15, scale, IconSize);
        addcloud(x, y, scale, linesize);
        addsnow(x, y, scale, IconSize);
    }
    // #########################################################################################
    void Fog(int x, int y, bool IconSize, String IconName) {
        int linesize = 3, scale = Large;
        if (IconSize == SmallIcon) {
            scale = Small;
            linesize = 1;
        }
        if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
        addcloud(x, y - 5, scale, linesize);
        addfog(x, y - 5, scale, linesize, IconSize);
    }
    // #########################################################################################
    void Haze(int x, int y, bool IconSize, String IconName) {
        int linesize = 3, scale = Large;
        if (IconSize == SmallIcon) {
            scale = Small;
            linesize = 1;
        }
        if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
        addsun(x, y - 5, scale * 1.4, IconSize);
        addfog(x, y - 5, scale * 1.4, linesize, IconSize);
    }
    // #########################################################################################
    void CloudCover(int x, int y, int CCover) {
        addcloud(x - 9, y - 3, Small * 0.5, 2);  // Cloud top left
        addcloud(x + 3, y - 3, Small * 0.5, 2);  // Cloud top right
        addcloud(x, y, Small * 0.5, 2);          // Main cloud
                                                 // u8g2Fonts.setFont(u8g2_font_6x13_t_cyrillic);
        drawString(x + 15, y - 5, String(CCover) + "%", LEFT);
    }
    // #########################################################################################
    void Visibility(int x, int y, String Visi) {
        y = y - 3;  //
        float start_angle = 0.52, end_angle = 2.61;
        int r = 10;
        for (float i = start_angle; i < end_angle; i = i + 0.05) {
            display.drawPixel(x + r * cos(i), y - r / 2 + r * sin(i), GxEPD_BLACK);
            display.drawPixel(x + r * cos(i), 1 + y - r / 2 + r * sin(i), GxEPD_BLACK);
        }
        start_angle = 3.61;
        end_angle = 5.78;
        for (float i = start_angle; i < end_angle; i = i + 0.05) {
            display.drawPixel(x + r * cos(i), y + r / 2 + r * sin(i), GxEPD_BLACK);
            display.drawPixel(x + r * cos(i), 1 + y + r / 2 + r * sin(i), GxEPD_BLACK);
        }
        display.fillCircle(x, y, r / 4, GxEPD_BLACK);
        // u8g2Fonts.setFont(u8g2_font_6x13_t_cyrillic);
        drawString(x + 12, y - 3, Visi, LEFT);
    }
    // #########################################################################################
    void addmoon(int x, int y, int scale, bool IconSize) {
        if (IconSize == LargeIcon) {
            x = x + 12;
            y = y + 12;
            display.fillCircle(x - 50, y - 55, scale, GxEPD_BLACK);
            display.fillCircle(x - 35, y - 55, scale * 1.6, GxEPD_WHITE);
        } else {
            display.fillCircle(x - 20, y - 12, scale, GxEPD_BLACK);
            display.fillCircle(x - 15, y - 12, scale * 1.6, GxEPD_WHITE);
        }
    }
    // #########################################################################################
    void Nodata(int x, int y, bool IconSize, String IconName) {
        if (IconSize == LargeIcon)
            u8g2Fonts.setFont(big_gluon_font_42);
        else
            u8g2Fonts.setFont(medium_gluon_font);
        drawString(x - 3, y - 8, "?", CENTER);
        // u8g2Fonts.setFont(u8g2_font_6x13_t_cyrillic);
    }

} gluon;
void appendDebugToLCD(const __FlashStringHelper* msg, bool new_line) {
    //  gluon.appendDebug(msg, new_line);
}
void appendDebugToLCD(const char* msg, bool new_line) {
    //  gluon.appendDebug(msg, new_line);
}
void appendDebugToLCD(int value, bool new_line) {
    // char buffer[16];
    // snprintf(buffer, sizeof(buffer), "%d", value);
    // gluon.appendDebug(buffer, new_line);
}
void appendDebugToLCD(long value, bool new_line) {
    // char buffer[16];
    // snprintf(buffer, sizeof(buffer), "%ld", value);
    // gluon.appendDebug(buffer, new_line);
}
void appendDebugToLCD(float value, bool new_line) {
    // char buffer[16];
    //  snprintf(buffer, sizeof(buffer), "%f", value);
    // gluon.appendDebug(buffer, new_line);
}
void appendDebugToLCD(const String& msg, bool new_line) {
    // gluon.appendDebug(msg.c_str(), new_line);
}
void appendDebugToLCD(const su::Text& msg, bool new_line) {
    // gluon.appendDebug(msg.toString(), new_line);
}
void appendDebugToLCD(unsigned int value, bool new_line) {
    // char buffer[16];
    // snprintf(buffer, sizeof(buffer), "%u", value);
    //  gluon.appendDebug(buffer, new_line);
    //  if (new_line) gluon.appendDebug("\n", new_line);
}
void appendDebugToLCD(unsigned long value, bool new_line) {
    // char buffer[16];
    // snprintf(buffer, sizeof(buffer), "%u", value);
    //  gluon.appendDebug(buffer, new_line);
    //  if (new_line) gluon.appendDebug("\n", new_line);
}

const char welcome_message[] PROGMEM = R"rawliteral(
    <div class="hello" >
        <span class="hello-message">
            Я погодная станиция GLUONiCA!<br>
            Для продолжения требуется первоначальная настройка.<br>
            Тут есть инструкция: 🌐 <a href="https://github.com/TonTon-Macout/GLUONiCA-WS/blob/main/help/first_start.md" target="_blank" rel="noopener noreferrer">github.com</a>
        </span>
        <br>
    </div>
    )rawliteral";
const char weatherHtmlTemplate[] PROGMEM = R"rawliteral(
  <div class="weather-container">
    <div class="header-bar">
      <div>%CITY%</div>
      <div>%DATE% • %TIME%</div>
    </div>
    <div class="top-row">
      <div class="current-main">
        <div class="weather-icon">%CURRENT_ICON%</div>
      </div>
      <div class="current-temp-big">
        <div class="temp-main">
          <div><div class="big-temp">%CURRENT_TEMP%</div></div>
        </div>
        <div class="day-night" style="margin-top: auto;">
          <strong>%DAY_TEMP%</strong><strong>%NIGHT_TEMP%</strong>
        </div>
      </div>
      <div class="info-column">
        <div class="info-box">Давление: %PRESSURE%</div>
        <div class="info-box">Ветер: %WIND%</div>
        <div class="info-box">УФ-индекс: %UV%</div>
        <div class="info-box">Восход: %SUNRISE%</div>
        <div class="info-box">Закат: %SUNSET%</div>
      </div>
    </div>
    <div class="forecast-grid">
      %FORECAST_DAYS%
    </div>
  </div>
)rawliteral";  // CO₂ 1200 ppm
const char insideHtmlTemplate[] PROGMEM = R"rawliteral( 
  <div class="inside-container">
    <div class="top-row">

        <div class="inside-temp-container">
          <div class="inside-temp-big">
            <div class="inside-home">%CINSIDE_ICON%</div>
          </div>
          <div class="inside-temp-big">
            <div class="temp-main">%INSIDE_TEMP%</div>
          </div>
          <div class="inside-temp-big">
            <div class="temp-main">%INSIDE_HUM%</div>
          </div>
        </div>
        <div class="inside-extra_container">
          <div class="extra-one">
            <div><div class="inside-extra">%INSIDE_EXTRA_ONE%</div></div>
          </div>
          <div class="extra-two">
            <div><div class="inside-extra">%INSIDE_EXTRA_TWO%</div></div>
          </div>
        </div>
    </div>
  </div>
)rawliteral";

String createForecastDay(const String& day, const String& icon, const String& temp) {
    return "<div class=\"forecast-day\">"
           "<div class=\"forecast-day-name\">" +
           day +
           "</div>"
           "<div class=\"forecast-icon\">" +
           icon +
           "</div>"
           "<div class=\"forecast-temp\">" +
           temp +
           "</div>"
           "</div>";
}
String generateWeatherHTML() {
    String html = FPSTR(weatherHtmlTemplate);

    html.replace("%CITY%", settings.city_om);
    html.replace("%DATE%", date_str);

    html.replace("%TIME%", NTP.timeToString());

    html.replace("%CURRENT_ICON%", IconToEmoji(WxConditions[0].Icon));

    // w_text.reserve(50);
    int8_t outside_temp = round(WxConditions[0].Temperature);
    String w_text = outside_temp > 0 ? "+" : "";
    w_text += outside_temp;
    w_text += "°";
    // DEBUG(SIMBOL_CUSTOM, "CURRENT_TEMP w_text: ", w_text);
    html.replace("%CURRENT_TEMP%", w_text);

    outside_temp = round(Daily[0].High);
    w_text = outside_temp > 0 ? "+" : "";
    w_text += outside_temp;
    w_text += "°";
    // DEBUG(SIMBOL_CUSTOM, "DAY_TEMP w_text: ", w_text);
    html.replace("%DAY_TEMP%", w_text);

    outside_temp = round(Daily[0].Low);
    w_text = outside_temp > 0 ? "+" : "";
    w_text += outside_temp;
    w_text += "°";
    // DEBUG(SIMBOL_CUSTOM, "NIGHT_TEMP w_text: ", w_text);
    html.replace("%NIGHT_TEMP%", w_text);

    w_text = (int)round(WxConditions[0].Windspeed);
    w_text += " м/с, ";
    w_text += gluon.windDegToDirection(WxConditions[0].Winddir);
    // DEBUG(SIMBOL_CUSTOM, "WIND w_text: ", w_text);
    html.replace("%WIND%", w_text);

    w_text = status.get_press();
    w_text += "мм.рт.ст.";
    // DEBUG(SIMBOL_CUSTOM, "PRESSURE w_text: ", w_text);
    html.replace("%PRESSURE%", w_text);

    w_text = (int)round(WxConditions[0].UVI);
    // DEBUG(SIMBOL_CUSTOM, "UVI w_text: ", w_text);
    html.replace("%UV%", w_text);

    html.replace("%SUNRISE%", ConvertUnixTime(WxConditions[0].Sunrise).substring(0, 5));
    html.replace("%SUNSET%", ConvertUnixTime(WxConditions[0].Sunset).substring(0, 5));

    String forecast = "";

    uint8_t day_for_print = weekDay;

    for (int r = 0; r < 7; r++) {
        if (r > 0 && ++day_for_print > 7) {
            day_for_print = 1;
        }

        String dayName = weekday_D_shorty[day_for_print];
        outside_temp = round(Daily[r].High);
        String temp = outside_temp > 0 ? "+" : "";
        temp += String(outside_temp) + "°";

        String icon = IconToEmoji(Daily[r].Icon);

        forecast += createForecastDay(dayName, icon, temp);
    }

    html.replace("%FORECAST_DAYS%", forecast);

    return html;
}

String generateInsideHTML() {
    String html = FPSTR(insideHtmlTemplate);

    html.replace("%CINSIDE_ICON%", "🏡");

    int8_t inside_t = status.get_temp();
    String w_text = inside_t > 0 ? "+" : "";
    w_text += inside_t;
    w_text += "°";
    html.replace("%INSIDE_TEMP%", w_text);

    inside_t = status.get_hum();
    w_text = inside_t;
    w_text += "%";
    html.replace("%INSIDE_HUM%", w_text);

    int16_t extra_d_1 = status.get_co2();
    w_text = "co₂ ";
    w_text += extra_d_1;
    w_text += " ppm";
    html.replace("%INSIDE_EXTRA_ONE%", w_text);

    html.replace("%INSIDE_EXTRA_TWO%", "&nbsp");

    return html;
}

class BUILD_UI {
   public:
    bool fine_tuning = false;  // показать скрыть тонкие настройки

    void alert(String text) {
        alertStr = text;
        show_alert = true;
    }
    void notif(String text) {
        notifStr = text;
        show_notif = true;
    }
    void confReset() {
    }

    void printDebugInfo() {
        static uint8_t count_ = 0;
        count_++;

        DEBUG(SIMBOL_ERR, "------------- STATUS " + (String)count_ + " ------------");

        if (settings.debug_lvl > Debug::DBG_LVL_INFO) {
            DEBUG(SIMBOL_ERR, "необходим уровень дебага - INFO");
            return;
        }

        DEBUG(SIMBOL_WAR, "--- процессор ---");
        // процессор
        DEBUG(SIMBOL_INFO, "Частота процессора: ", ESP.getCpuFreqMHz(), " МГц");
        DEBUG(SIMBOL_INFO, "Количество ядер: ", ESP.getChipCores());
        DEBUG(SIMBOL_INFO, "Модель чипа: ", ESP.getChipModel());
        DEBUG(SIMBOL_INFO, "Ревизия чипа: ", ESP.getChipRevision());
        DEBUG(SIMBOL_INFO, "Версия SDK: ", ESP.getSdkVersion());
        float temp = temperatureRead();
        DEBUG(SIMBOL_INFO, "Температура: ", temp, " °C");
        uint8_t raw = temprature_sens_read();
        float temp2 = (raw - 32) / 1.8;  // конвертация из °F в °C
        DEBUG(SIMBOL_INFO, "Температура 2: ", temp2, " °C");
        DEBUG(SIMBOL_INFO, "          raw: ", raw);

        // память
        DEBUG(SIMBOL_WAR, "--- оперативная память ---");
        DEBUG(SIMBOL_INFO, "Свободная куча: ", ESP.getFreeHeap(), " байт");
        DEBUG(SIMBOL_INFO, "Общий размер кучи: ", ESP.getHeapSize(), " байт");
        DEBUG(SIMBOL_INFO, "Минимальная свободная куча: ", ESP.getMinFreeHeap(), " байт");
        //  DEBUG(SIMBOL_INFO, "Максимальный блок кучи (Max Alloc Heap): ", ESP.getMaxAllocHeap(), " байт");
        DEBUG(SIMBOL_WAR, "--- флеш память ---");
        DEBUG(SIMBOL_INFO, "Общий размер флэш-памяти : ", ESP.getFlashChipSize(), " байт");
        DEBUG(SIMBOL_INFO, "Размер скетча : ", ESP.getSketchSize(), " байт");
        DEBUG(SIMBOL_INFO, "Свободное место для скетча: ", ESP.getFreeSketchSpace(), " байт");
        DEBUG(SIMBOL_INFO, "Частота флэш-памяти (Flash Chip Speed): ", ESP.getFlashChipSpeed(), " Гц");
        DEBUG(SIMBOL_INFO, "Режим флэш-памяти (Flash Chip Mode): ", ESP.getFlashChipMode() == FM_QIO ? "QIO" : ESP.getFlashChipMode() == FM_QOUT ? "QOUT"
                                                                                                           : ESP.getFlashChipMode() == FM_DIO    ? "DIO"
                                                                                                           : ESP.getFlashChipMode() == FM_DOUT   ? "DOUT"
                                                                                                                                                 : "UNKNOWN");
        DEBUG(SIMBOL_WAR, "--- внешняя память ---");
// Вывод информации о PSRAM (если поддерживается)
#ifdef CONFIG_SPIRAM_SUPPORT
        DEBUG(SIMBOL_INFO, "Общий размер PSRAM (PSRAM Size): ", ESP.getPsramSize(), " байт");
        DEBUG(SIMBOL_INFO, "Свободный PSRAM (Free PSRAM): ", ESP.getFreePsram(), " байт");
#else
        DEBUG(SIMBOL_INFO, "PSRAM: ", "Не поддерживается на этом модуле");
#endif

        // Вывод информации о LittleFS
        DEBUG(SIMBOL_WAR, "--- файловая система LittleFS ---");

        // if (LittleFS.begin()) {
        DEBUG(SIMBOL_INFO, "Общий размер: ", LittleFS.totalBytes(), " байт");
        DEBUG(SIMBOL_INFO, "Использовано: ", LittleFS.usedBytes(), " байт");
        DEBUG(SIMBOL_INFO, "Свободно: ", LittleFS.totalBytes() - LittleFS.usedBytes(), " байт");
        DEBUG(SIMBOL_INFO, F("[LittleFS] Список файлов:"));
        listFiles("/");
        //} else {
        //    DEBUG(SIMBOL_INFO, "LittleFS: ", "Не удалось инициализировать");
        //}
        DEBUG(SIMBOL_WAR, "--- Время ---");
        DEBUG(SIMBOL_CUSTOM, "NTP Date: " + NTP.dateToString());
        DEBUG(SIMBOL_CUSTOM, "NTP Time: " + NTP.timeToString());
        DEBUG(SIMBOL_CUSTOM, "RTC Date: " + rtc.dateToString());
        DEBUG(SIMBOL_CUSTOM, "RTC Time: " + rtc.timeToString());
        DEBUG(SIMBOL_CUSTOM, "Время системы:" + String(hour) + ":" + String(minute) + ":" + String(second));

        DEBUG(SIMBOL_INFO, "RTC Temp: ", rtc.getTempInt(), "°C");

        // Датчики
        DEBUG(SIMBOL_WAR, "--- Датчики ---");
        if (settings.sht_snsr && status.available_sht) {
            sht.readSample();
            DEBUG(SIMBOL_INFO, "SHT Temp: ", sht.getTemperature(), "°C");
            DEBUG(SIMBOL_INFO, "SHT Hum: ", sht.getHumidity(), "%");
        }
        if (settings.aht_snsr && status.available_aht) {
            DEBUG(SIMBOL_INFO, "AHT Temp: ", aht.readTemperature(), "°C");
            DEBUG(SIMBOL_INFO, "AHT Hum: ", aht.readHumidity(), "%");
        }
        if (settings.bmep_snsr && status.available_bmep) {
            DEBUG(SIMBOL_INFO, "BME/BMP Temp: ", bme.readTemperature(), "°C");
            DEBUG(SIMBOL_INFO, "BME/BMP Hum: ", bme.readHumidity(), "%");
            DEBUG(SIMBOL_INFO, "BME Press: ", round(bme.readPressure() * 0.00750061683f), "мм рт.ст.");
        }
        if (settings.scd40_snsr && status.available_scd40) {
            co2.readMeasurement();
            DEBUG(SIMBOL_INFO, "SCD40 Temp: ", round(co2.getTemperature()), "°C");
            DEBUG(SIMBOL_INFO, "SCD40 Hum: ", round(co2.getHumidity()), "%");
            DEBUG(SIMBOL_INFO, "SCD40 CO2: ", co2.getCO2(), "ppm");
        }
        // if (settings.ina_snsr && status.available_ina) {  //
        //     DEBUG(SIMBOL_INFO, "INA Current: ", int(round(ina.getCurrent() * 1000)), "mA");
        //     DEBUG(SIMBOL_INFO, "INA Voltage: ", ina.getVoltage(), "v");
        // }

        if (settings.weather) {
            // status.getWeather();
            DEBUG(SIMBOL_WAR, "--- Погода ---");

            if (settings.weather == 1) {
                DEBUG(SIMBOL_INFO, "Поставщик погоды: openweathermap.org");
                DEBUG(SIMBOL_INFO, "Город: ", settings.city_owm);

            } else if (settings.weather == 2) {
                DEBUG(SIMBOL_INFO, "Поставщик погоды: open-meteo.com");
                DEBUG(SIMBOL_INFO, "Город: ", settings.city_om);
                DEBUG(SIMBOL_INFO, "Широта: ", settings.latitude);
                DEBUG(SIMBOL_INFO, "Долгота: ", settings.longitude);
            } else
                DEBUG(SIMBOL_ERR, "Неизвестный поставщик погоды!");

            DEBUG(SIMBOL_NONE, "Temp: ", WxConditions[0].Temperature, "°C");                         // status.get_outside_temp()
            DEBUG(SIMBOL_NONE, "Hum: ", WxConditions[0].Humidity, "%");                              //
            DEBUG(SIMBOL_NONE, "Press: ", String(WxConditions[0].Pressure * 0.75006), "мм рт.ст.");  //
            DEBUG(SIMBOL_NONE, "Wind: ", WxConditions[0].Windspeed, "м/с");                          //
            DEBUG(SIMBOL_NONE, "Облачность: ", WxConditions[0].Cloudcover, "%");                     //
            // DEBUG(SIMBOL_NONE, "Погода: ", WxConditions[0]);//status.get_outside_weather()
        }

        if (settings.mqtt) {
            DEBUG(SIMBOL_WAR, "--- MQTT ---");
            DEBUG(SIMBOL_INFO, "MQTT Сonnected: ", ha.isConnected() ? "Ok" : "No");
            DEBUG(SIMBOL_INFO, "MQTT Temp: ", status.mqtt_temperature, "°C");
            DEBUG(SIMBOL_INFO, "MQTT Hum: ", status.mqtt_humidity, "%");
            DEBUG(SIMBOL_INFO, "MQTT Press: ", status.mqtt_pressure, "мм рт.ст.");
            DEBUG(SIMBOL_INFO, "MQTT CO2: ", status.mqtt_co2, "ppm");
            DEBUG(SIMBOL_INFO, "MQTT PM1: ", status.mqtt_pm1, "ppm");
            DEBUG(SIMBOL_INFO, "MQTT PM2.5: ", status.mqtt_pm25, "ppm");
            DEBUG(SIMBOL_INFO, "MQTT PM10: ", status.mqtt_pm10, "ppm");
        }

        DEBUG(SIMBOL_ERR, "^^^^^^^^^^^^^^^^^^^^^^^^^^");

        sett.reload();
    }

    bool show_notif = false;         // показать уведомление
    String notifStr = "";            // текст уведомления
    bool show_alert = false;         // показать уведомление с ошибкой
    String alertStr = "";            // текст уведомления с ошибкой
    bool show_cnfrm_reset = false;   // сброс
    bool show_cnfrm_delete = false;  // удалить
    String cnfrmStr = "";            // текст в окне подтверждения

    bool show_display_button = false;
    void wifi_group() {
    }

   private:
} ui;
void build(sets::Builder& b) {
    if (settings.welcome_message) {
        sets::Group g(b, "🖖 ПРИВЕТ!");  //<span class="hello-h">Привет!</span><br>

        b.HTML("", welcome_message);
        //

        if (b.Button("больше не показывать", sets::Colors::Orange)) {
            settings.welcome_message = false;
            b.reload();
        }
    }

    b.HTML("", generateWeatherHTML());

    b.HTML("", generateInsideHTML());  //

    auto wifi_group = [&]() {
        {
            {
                b.Label("WIFI:", "", "", sets::Colors::Gray);
                if (wifi.connectionAttempts >= wifi.MAX_ATTEMPTS) {
                    b.Label("Осталось попыток подключения", "", "0", sets::Colors::Red);
                    b.Label("Для сброса нажмите кнопку \"подключиться\"");
                }
                if (b.Input("имя", "", AnyPtr(settings.ssid, 25))) {
                    status.save_mem(true);
                    b.reload();
                }
                if (b.Pass("пароль", "минимум 8 латинских символа", AnyPtr(settings.password, 20))) {
                    if (strcmp(settings.pass_ap, "appassword") == 0) {
                        if (settings.ap == wifi.AP_NO_PASS) {
                            if (strlen(settings.pass_ap) >= 8) {
                                strncpy(settings.pass_ap, settings.password, sizeof(settings.pass_ap) - 1);
                                settings.pass_ap[sizeof(settings.pass_ap) - 1] = '\0';

                                settings.ap = wifi.AP_PASS;
                            }
                        }
                    }
                    status.save_mem(true);
                    b.reload();
                }

                if (wifi.getError() != -2) b.Label("WiFi", "", wifi.getErrorStr(), wifi.getError() == 0 ? sets::Colors::Green : sets::Colors::Red);
                if (!settings.switchWifi && b.Button("подключиться")) {
                    wifi.connectionAttempts = 0;
                    wifi.begin();
                }
            }
            b.HTML("", "<span class=\"HR\"></span>");
            {
                b.Label("WIFI Резервный:", "", "", sets::Colors::Gray);
                if (b.Switch("сначала искать резервный", "", &settings.switchWifi)) b.reload();
                if (b.Input("имя", "", AnyPtr(settings.ssid_reserv, 25))) {
                    status.save_mem(true);
                    b.reload();
                }
                if (b.Pass("пароль", "минимум 8 латинских символа", AnyPtr(settings.password_reserv, 20))) {
                    status.save_mem(true);
                    b.reload();
                }
                if (wifi.getReservError() != -2) b.Label("WiFi", "", wifi.getReservErrorStr(), wifi.getReservError() == 0 ? sets::Colors::Green : sets::Colors::Red);
                if (settings.switchWifi && b.Button("подключиться")) {
                    wifi.connectionAttempts = 0;
                    wifi.begin();
                }
            }
            b.HTML("", "<span class=\"HR\"></span>");
            {
                b.Label("Точка доступа:", "", "", sets::Colors::Gray);
                if (b.Select("Запуск", "Когда будет запущена точка доступа", "НИКОДА;При отсутсвии сетей, без пароля;При отсутсвии сетей, с паролем;Всегда, без пароля;Всегда, с паролем;", &settings.ap)) {
                    b.reload();
                }
                if (settings.ap == wifi.ALWAYS_AP_NO_PASS || settings.ap == wifi.ALWAYS_AP_PASS) {
                    b.Label(" ", "", "Внимание!!!", sets::Colors::Orange);
                    b.Paragraph("", "", "wifi всегда будет работать в режиме точки доступа!");
                }

                if (settings.ap == wifi.ALWAYS_AP_NO_PASS || settings.ap == wifi.AP_NO_PASS) {
                    b.Label(" ", "", "Внимание!!!", sets::Colors::Red);
                    b.Paragraph("", "", "Кто угодно может подключиться к точке доступа и управлять часами!");
                }

                if (b.Input("сетевое имя", "WiFi SSID, Имя точки доступа, Имя MQTT", AnyPtr(settings.ssid_ap, 25))) {
                    status.save_mem(true);
                    b.reload();
                }
                if (strlen(settings.pass_ap) < 8) b.Label(" ", "", "Пароль короче 8 символов!!!", sets::Colors::Red);
                if (b.Pass("пароль", "Пароль минимум 8 латинских символов или цифр", AnyPtr(settings.pass_ap, 20))) {
                    status.save_mem(true);
                    b.reload();
                }
            }
        }
    };

    //  WIFI
    if (wifi.isInAPMode()) {
        sets::Group g(b, "WIFI");
        wifi_group();
    }

    {
        sets::Group g(b, "НАСТРОЙКИ");
        if (b.Switch("ТОНКИЕ НАСТРОЙКИ", "Покажет дополнительные настройки", &ui.fine_tuning)) b.reload();
        if (b.Switch("Сон", "Засыпать между обновлениями экрана", &settings.sleep)) b.reload();

        b.Switch("Экран загрузки", "Показывать экран загрузки", &settings.show_logo_screen);
        if (settings.show_logo_screen) b.Switch("Выводить IP", "Показывать выданный IP на экране загрузки", &settings.show_ip_in_logo_screen);
        String txt_sleep_label = settings.sleep ? "Время сна, минут" : "Интервал обновления экрана, минут";
        b.Number(txt_sleep_label, "Время между обновлениями экрана и получением погоды", &settings.sleep_duration);
    }
    ///////////////////////////////////////////////////////
    // ====================================================
    // расписание
    {
        sets::Group g(b, "НОЧНОЙ РЕЖИМ");  //
        String txt_sleep_label = settings.sleep ? "Увеличеный интервал сна" : "Увеличеный интервал обновления экрана и получения погоды";
        if (b.Switch("Включить", txt_sleep_label, &settings.quiet_mode)) {
            b.reload();
        }
        if (settings.quiet_mode) {
            {
                sets::Row g(b);

                if (b.Time("начало", "", &settings.start_mute)) {
                }
                if (b.Time("конец", "", &settings.end_mute)) {
                }
            }
            String txt_sleep_label_dur = settings.sleep ? "Интервал сна, минут" : "Интервал обновления, минут";
            b.Number(txt_sleep_label_dur, "Интевал обновления экрана и получения погоды в ночном режиме", &settings.sleep_duration_quiet_mode);
        }
    }
    ///////////////////////////////////////////////////////
    // ====================================================
    // время
    {
        sets::Group gr(b, "ВРЕМЯ");
        {
            if (b.Select("Источник времени", "Местное - время в указанной для погоды точке. Из интернета - из указанного ntp сервера. Часы - из встроенного модуля", "Местное;Из интернета;Часы", &settings.internet_time)) {
                ntp_init();

                // status.refresh_date = true;
                //  gluon.clock_tik ();
                b.reload();
            }
            if (ui.fine_tuning && settings.internet_time == 1)
                if (b.Input("host", "", AnyPtr(settings.ntp_host, 30))) {
                    status.save_mem(true);
                    b.reload();
                }
            // установить часовой пояс
            if (settings.internet_time != 0) {
                if (b.Number("time zone", "Часовой пояс в часах, например 3 - Москва", &settings.timeZone)) {
                    NTP.setGMT(settings.timeZone);
                    b.reload();
                }
            } else {
                // b.LabelNum("Часовой пояс", "Установленный часовой пояс в часах", NTP.getGMT()/60);
                b.LabelNum("Часовой пояс", "Часовой пояс выбранной точки в часах, например 3 - Москва", WxConditions[0].Timezone / 3600);
            }
            // информер времени
            if (settings.internet_time == 1 || settings.internet_time == 0) {
                b.Label("время", "", NTP.timeToString() + " " + NTP.dateToString());
            }
            if (settings.internet_time == 2) {
                Datime dt = rtc.getTime();
                uint32_t _unix = dt.getUnix();
                if (b.DateTime("установить", "", &_unix)) {
                    rtc.setUnix(_unix);
                    NTP.sync(_unix);
                    b.reload();
                }
            }
            {
                sets::Buttons r(b);

                switch (settings.internet_time) {
                    case 0:  // местное
                        if (b.Button("обновить")) {
                            uint32_t _unix = WxConditions[0].Dt;
                            NTP.setGMT(0);
                            NTP.sync(_unix);
                            b.reload();
                        }
                        break;
                    case 1:  // ntp сервер
                        if (b.Button(F("получить"))) {
                            DEBUG(SIMBOL_INFO, "[NTP] Получаем время с сервера");
                            NTP.updateNow();
                            b.reload();
                        }
                        break;
                    case 2:  // часы
                        if (b.Button("обновить")) {
                            Datime dt = rtc.getTime();
                            uint32_t _unix = dt.getUnix();
                            rtc.setUnix(_unix);
                            NTP.sync(_unix);
                            b.reload();
                        }
                        break;

                    default:
                        break;
                }
            }

        }  // группа ВРЕМЯ
    }
    ///////////////////////////////////////////////////////
    // ====================================================
    // погода
    {
        sets::Group g(b, "ПОГОДА");
        // if (b.Switch(F("openweathermap.org"), &settings.weather)) {
        //
        //     // status.refresh_date = true;
        //     b.reload();
        // }

        if (b.Select("Источник", "", "НЕТ;openweathermap.org;open-meteo.com;", &settings.weather)) b.reload();

        if (settings.weather) {
            if (settings.weather == 1) {
                // b.Link("Сайт", "", "https://openweathermap.org/");
                b.HTML("", R"(<a href="https://openweathermap.org/">openweathermap.org</a>)");
                b.Label("В разработке!!!");
                if (b.Input("Город", "", AnyPtr(settings.city_owm, 31))) {
                    replaceSpaces(settings.city_owm, SPACE_START_END);

                    b.reload();
                }
                if (b.Input("ключ API", "", AnyPtr(settings.owm_api_key, 36))) {
                    replaceSpaces(settings.owm_api_key, SPACE_TO_NONE);

                    b.reload();
                }
            } else if (settings.weather == 2) {
                // b.Link("Сайт", "", "https://open-meteo.com/");
                b.HTML("", R"(<a href="https://open-meteo.com/">open-meteo.com</a>)");
                b.Label("Данные для погоды", "Погода получается для указанных координат. Можно ввести вручную или искать по названию города. Ввести название или часть названия, нажать - искать. Если город нашелся - нажать получить, и обновить экран.");
                {
                    sets::Row r(b);
                    if (b.Input("город", "Имя города отображаемое в строке даты. Также используется для поиска координат. Можно писать кирилицей, иногда нужно между словами писать тире, например \"Санкт-Петербург\". Что нашлось - в логах ", AnyPtr(settings.city_om, 61))) {
                        b.reload();
                    }

                    if (b.Button("искать")) {
                        bool ok = status.getCityCoordinates(settings.city_om);
                        if (ok)
                            ui.notif("Отлично! Город нашелся.");
                        else
                            ui.alert("Ошибка получения координат");
                        b.reload();
                    }
                }
                String city_str = String(settings.last_city_owm[0]) + ";" +
                                  settings.last_city_owm[1] + ";" +
                                  settings.last_city_owm[2] + ";" +
                                  settings.last_city_owm[3] + ";" +
                                  settings.last_city_owm[4] + ";";

                static uint8_t last_sity;
                if (b.Select("Города", "Последние найденные города.", city_str, &last_sity)) {
                    strncpy(settings.city_om, settings.last_city_owm[last_sity], 60);
                    settings.city_om[60] = '\0';  // гарантируем завершающий ноль
                    b.reload();
                }
                b.Number("широта", "", &settings.latitude);
                b.Number("долгота", "", &settings.longitude);

                if (settings.internet_time != 0)
                    if (b.Select("Полушарие", "Для фазы луны", "Северное;Южное", &settings.hemisphere)) b.reload();
            }
        }

        if (settings.weather)
            if (status.owm_weather_error != 200 && status.owm_weather_error > 0) {
                String _err = "";

                if (settings.weather == 1) switch (status.owm_weather_error) {
                        case 401:
                            _err = "Error 401: Invalid API key";
                            break;
                        case 403:
                            _err = "Error 403: Forbidden access";
                            break;
                        case 404:
                            _err = "Error 404: City not found";
                            break;
                        case 429:
                            _err = "Error 429: Too many requests";
                            break;
                        case 500:
                            _err = "Error 500: Internal server error";
                            break;
                        case 502:
                            _err = "Error 502: Bad gateway";
                            break;
                        case 503:
                            _err = "Error 503: Service unavailable";
                            break;

                        default:
                            _err = "Error unknown: ";
                            _err += status.owm_weather_error;
                            break;
                    }

                if (settings.weather == 2) _err = status.om_weather_error_str;

                b.Label("error", "", _err, sets::Colors::Red);
                b.reload();
            }

        if (settings.weather) {
            sets::Buttons g(b);
            if (b.Button("получить погоду")) {
                b.reload();
                count();
                if (status.getWeather()) {
                    ui.show_display_button = true;
                    ui.notif("Погода получена!");
                    DEBUG(SIMBOL_INFO, "Погода получена");
                }  // погода
                else {
                    ui.alert("Ошибка получения погоды!");
                    DEBUG(SIMBOL_ERR, "Ошибка получения погоды");
                    ui.show_display_button = false;
                }

                b.reload();
            }

            if (ui.show_display_button && b.Button("на экран")) {
                ui.show_display_button = false;
                ui.notif("OK");
                b.reload();
                count();
                gluon.Display();
                b.reload();
            }
        }
    }
    ///////////////////////////////////////////////////////
    // ====================================================
    //  MQTT
    {
        sets::Group gr(b, "MQTT");
        if (b.Switch(("MQTT"), "Получение и отправка данных на MQTT сервер Home Assistant", &settings.mqtt)) {
            if (settings.mqtt) {
                if (mqtt_init())
                    ui.notif("MQTT Подключено!");
                else
                    ui.alert("Ошибка подключения MQTT!");
            } else {
                ha.end();
                ui.notif("ОК");
            }

            b.reload();
            // status.refresh_roomEnvironment = true;
        }
        if (settings.mqtt) {
            if (b.Input("сервер", "", AnyPtr(settings.mqtt_server, sizeof(settings.mqtt_server)))) {
                replaceSpaces(settings.mqtt_server, SPACE_TO_DOT);
                // status.mqtt_connected = false;
                if (mqtt_init())
                    ui.notif("MQTT Подключено!");
                else
                    ui.alert("Ошибка подключения MQTT!");

                // set_mqtt_server();
                // reconnect_mqtt(true);
                b.reload();
            }
            if (ui.fine_tuning)
                if (b.Input("логин", "", AnyPtr(settings.mqtt_user, sizeof(settings.mqtt_user)))) {
                    replaceSpaces(settings.mqtt_user, SPACE_TO_UNDERSCORE);
                    if (mqtt_init())
                        ui.notif("MQTT Подключено!");
                    else
                        ui.alert("Ошибка подключения MQTT!");

                    // status.mqtt_connected = false;
                    //  reconnect_mqtt(true);
                    b.reload();
                }
            if (ui.fine_tuning)
                if (b.Pass("пароль", "", AnyPtr(settings.mqtt_pass, sizeof(settings.mqtt_pass)))) {
                    replaceSpaces(settings.mqtt_pass, SPACE_TO_NONE);
                    if (mqtt_init())
                        ui.notif("MQTT Подключено!");
                    else
                        ui.alert("Ошибка подключения MQTT!");
                    // status.mqtt_connected = false;

                    // reconnect_mqtt(true);
                    b.reload();
                }
            {
                if (b.Number("порт", "", &settings.mqtt_port)) {
                    // status.mqtt_connected = false;
                    if (mqtt_init())
                        ui.notif("MQTT Подключено!");
                    else
                        ui.alert("Ошибка подключения MQTT!");

                    //  reconnect_mqtt(true);

                    b.reload();
                }
            }
            if (ui.fine_tuning) {
                if (b.Input("имя клиента", "", AnyPtr(settings.mqtt_client_name, sizeof(settings.mqtt_client_name)))) {
                    // replaceSpaces(settings.mqtt_client_name, SPACE_TO_UNDERSCORE);
                    //  status.mqtt_connected = false;
                    if (mqtt_init())
                        ui.notif("MQTT Подключено!");
                    else
                        ui.alert("Ошибка подключения MQTT!");

                    //  reconnect_mqtt(true);
                    b.reload();
                }
            }
            if (ui.fine_tuning) {
                if (b.Input("подписаться", "Топик на который подписываемся", AnyPtr(settings.mqtt_topic_sub, sizeof(settings.mqtt_topic_sub)))) {
                    // replaceSpaces(settings.mqtt_topic_sub, SPACE_TO_SLASH);
                    // toLower_Case(settings.mqtt_topic_sub);
                    //  status.mqtt_connected = false;
                    if (mqtt_init())
                        ui.notif("MQTT Подключено!");
                    else
                        ui.alert("Ошибка подключения MQTT!");

                    //  reconnect_mqtt(true);
                    b.reload();
                }
                static uint8_t _devise = 0;
                if (ha.getDevicesCount() > 0)
                    if (b.Select("Доступные устройства", "Выберите устройство, для подписки", ha.getDevicesString(), &_devise)) {
                        DEBUG(SIMBOL_INFO, "Топик этого устройства: ", settings.mqtt_topic_pub);
                        DEBUG(SIMBOL_INFO, "Текущий топик подписки : ", settings.mqtt_topic_sub);

                        if (ha.getTopic(_devise) == settings.mqtt_topic_pub) {
                            DEBUG(SIMBOL_INFO, "Топик выбранного устройства: ", ha.getTopic(_devise));
                            DEBUG(SIMBOL_ERR, "Нельзя выбирать самого себя");
                            ui.alert("Нельзя выбирать самого себя");
                        } else {
                            strncpy(settings.mqtt_topic_sub, ha.getTopic(_devise).c_str(), 60);
                            ui.notif("ОК");
                            DEBUG(SIMBOL_INFO, "Новый топик для подписки: ", settings.mqtt_topic_sub);
                            mqtt_init();
                        }

                        b.reload();
                    }
            }

            if (ui.fine_tuning) {
                if (b.Input("отправлять", "Топик для отправки сообщения", AnyPtr(settings.mqtt_topic_pub, sizeof(settings.mqtt_topic_pub)))) {
                    // replaceSpaces(settings.mqtt_topic_pub, SPACE_TO_SLASH);
                    // toLower_Case(settings.mqtt_topic_pub);
                    //  status.mqtt_connected = false;
                    if (mqtt_init())
                        ui.notif("MQTT Подключено!");
                    else
                        ui.alert("Ошибка подключения MQTT!");

                    //  reconnect_mqtt(true);
                    b.reload();
                }
            }

            if (b.Switch(("отправлять данные"), "Отправлять данные с датчиков. Температура, влажность, co2 и т.д.", &settings.send_mqtt)) {
                b.reload();
            }
            if (settings.send_mqtt && b.Switch("отправлять после обновления", "Отправлять данные после получения погоды и данных с датчиков", &settings.mqtt_send_after_weather)) {
                b.reload();
            }
            if (settings.send_mqtt && !settings.mqtt_send_after_weather)
                if (b.Number("Интервал отправки, мин.", "", &settings.period_mqtt)) {
                    b.reload();
                }
            if (ui.fine_tuning) {
                if (b.Number("Попыток подключения", "Максимум неудачных. Сбрасывается раз в час или - включить выключить мктт. 0 - не ограничивать", &settings.count_mqtt)) {
                    ha.setMaxAttempts(settings.count_mqtt);
                    b.reload();
                }
                b.LabelNum("Неудачных попыток подключения", "количество текущих неудачных попыток подключения", ha.getReconnectAttempts());
            }

            // if (b.Input("сообщение", "Сообщение о подключении", AnyPtr(settings.mqtt_topic_msg, sizeof(settings.mqtt_topic_msg)))) {
            //     // replaceSpaces(settings.mqtt_topic_msg, SPACE_TO_UNDERSCORE);
            //     status.mqtt_connected = false;
            //
            //    reconnect_mqtt(true);
            //    b.reload();
            //}

            // кнопки
            if (ha.isConnected()) {  // status.mqtt_connected
                if (settings.send_mqtt) {
                    if (b.Button("опубликовать ")) {
                        // send_ha_discovery();
                        if (sendMqttData())
                            ui.notif("OK");
                        else
                            ui.alert("Ошибка");

                        b.reload();
                    }
                }
            } else {
                if (b.Button("подключиться", status.mqtt_count >= settings.count_mqtt ? sets::Colors::Red : sets::Colors::Default)) {
                    //  set_mqtt_server();
                    if (mqtt_init())
                        ui.notif("MQTT Подключено!");
                    else
                        ui.alert("Ошибка подключения MQTT!");

                    // status.mqtt_connected = false;
                    //  reconnect_mqtt(true);
                    b.reload();
                }
            }
        }
    }
    ///////////////////////////////////////////////////////
    // ====================================================
    ///////////////////////////////////////////////////////
    // ====================================================
    // ДАТЧИКИ
    if (ui.fine_tuning) {
        sets::Group gr(b, "ДАТЧИКИ");

        if (b.Select("Источник температуры в помещении", "", "BME/BMP;SHT;MQTT;SCD40;AHT", &settings.source_temp)) {
            status.updateSensors();
            b.reload();
        }
        if (b.Select("Источник влажности в помещении", "", "BME/BMP;SHT;MQTT;SCD40;AHT", &settings.source_hum)) {
            status.updateSensors();
            b.reload();
        }
        if (b.Select("Источник давления", "", "BME/BMP;MQTT;OUTSIDE", &settings.source_press)) {
            status.updateSensors();
            b.reload();
        }
        if (b.Select("Источник CO2", "", "MQTT;SCD40", &settings.source_co2)) {
            status.updateSensors();
            b.reload();
        }
        // if (b.Select("Источник PM", "", "MQTT;PMS", &settings.source_pm)) {
        //     if (settings.source_co2 == 1) status.pms_tick();
        //     b.reload();
        // }

        if (b.Button("опросить датчики")) {
            if (status.CheckSensors())
                ui.notif("ОК");
            else
                ui.alert("Нет доступных датчиков");
            b.reload();
        }
        b.HTML("", "<span class=\"HR\"></span>");
        b.Paragraph("", "", "• адрес в десятичном формате\n");

        {
            sets::Row r(b);
            b.LED("", "", status.available_bmep, sets::Colors::Gray, sets::Colors::Green);
            if (b.Switch("BME/P280", "", &settings.bmep_snsr)) {
                status.initBMP();

                status.updateSensors();
                base.updateNow();
                b.reload();
            }
        }

        if (b.Number("адрес bmp/bme*", "", &settings.ADDR_BME_P)) {
            status.initBMP();

            status.updateSensors();
            base.updateNow();
            b.reload();
        }
        {
            sets::Row r(b);
            b.LED("", "", status.available_sht, sets::Colors::Gray, sets::Colors::Green);
            if (b.Switch(("SHT | GXHT"), "", &settings.sht_snsr)) {
                status.initSHT();

                status.updateSensors();
                base.updateNow();
                b.reload();
            }
        }

        b.Label("адрес sht", "", "зашит в датчик, смотри лог");

        {
            sets::Row r(b);
            b.LED("", "", status.available_aht, sets::Colors::Gray, sets::Colors::Green);
            if (b.Switch(("AHT"), "", &settings.aht_snsr)) {
                status.initAHT();

                status.updateSensors();
                base.updateNow();
                b.reload();
            }
        }

        if (b.Number("адрес aht", "", &settings.ADDR_AHT)) {
            status.initAHT();

            status.updateSensors();
            base.updateNow();
            b.reload();
        }

        {
            sets::Row r(b);
            b.LED("", "", status.available_rtc, sets::Colors::Gray, sets::Colors::Green);
            if (b.Switch(("DS3231 (часы)"), "", &settings.rtc_snsr)) {
                status.initRTC();

                status.updateSensors();
                base.updateNow();
                b.reload();
            }
        }

        if (b.Number("адрес rtc", "", &settings.ADDR_RTC)) {
            status.initRTC();

            status.updateSensors();
            base.updateNow();
            b.reload();
        }

        {
            sets::Row r(b);
            b.LED("", "", status.available_scd40, sets::Colors::Gray, sets::Colors::Green);
            if (b.Switch(("SCD40"), "", &settings.scd40_snsr)) {
                status.initSCD40();

                status.updateSensors();
                base.updateNow();
                b.reload();
            }
        }

        if (b.Number("адрес SCD40", "", &settings.ADDR_SCD40)) {
            status.initSCD40();

            status.updateSensors();
            base.updateNow();
            b.reload();
        }

        if (b.Button("инициализация i2c")) {
            if (settings.debug_lvl > Debug::DBG_LVL_ALL) DEBUG(SIMBOL_ERR, "Полный вывод в дебаг требует уровень - ALL");

            status.I2C_Init();
            b.reload();
        }

        {
            b.HTML("", "<span class=\"HR\"></span>");
            if (b.Number("коэффициент температуры", "", &settings.temp_calibration_coeff)) {
                b.reload();
            }
            if (b.Number("коэффициент влажности", "", &settings.hum_calibration_coeff)) {
                b.reload();
            }
            if (b.Number("коэффициент co2", "", &settings.co2_calibration_coeff)) {
                b.reload();
            }
        }
/*
        b.HTML("", "<span class=\"HR\"></span>");
        if (b.Number("макс. температура процессора", "Порог температуры для уведомления", &settings.alert_cpu_temp, 0, 255)) {
        }
        if (b.Button("Тест перегрева")) {
            // tele.send("🌡️ Внимание!\nПерегрев процессора часов!\n" + (String)status.esp_cpu_temp + "°C");
            // if (settings.mode < MODE_SPACE) settings.previusMode = settings.mode;
            // settings.mode = MODE_OVERHEATING_ANIM;
            b.reload();
        }
*/
    }
    ///////////////////////////////////////////////////////

    ///////////////////////////////////////////////////////
    // ====================================================

    {
        {
            sets::Group r(b, "Веб интерфейс и WiFi");
            if (b.Select("цветовая схема", "", "Black;Gray;White;Red;Orange;Yellow;Green;Mint;Aqua;Blue;Violet;Pink;", &settings.color_theme)) {
                switch (settings.color_theme) {
                    case 0:
                        sett.config.theme = sets::Colors::Black;
                        break;
                    case 1:
                        sett.config.theme = sets::Colors::Gray;
                        break;
                    case 2:
                        sett.config.theme = sets::Colors::White;
                        break;
                    case 3:
                        sett.config.theme = sets::Colors::Red;
                        break;
                    case 4:
                        sett.config.theme = sets::Colors::Orange;
                        break;
                    case 5:
                        sett.config.theme = sets::Colors::Yellow;
                        break;
                    case 6:
                        sett.config.theme = sets::Colors::Green;
                        break;
                    case 7:
                        sett.config.theme = sets::Colors::Mint;
                        break;
                    case 8:
                        sett.config.theme = sets::Colors::Aqua;
                        break;
                    case 9:
                        sett.config.theme = sets::Colors::Blue;
                        break;
                    case 10:
                        sett.config.theme = sets::Colors::Violet;
                        break;
                    case 11:
                        sett.config.theme = sets::Colors::Pink;
                        break;
                    default:
                        sett.config.theme = sets::Colors::Default;
                        break;
                }

                b.reload();
            }

            b.HTML("", "<span class=\"HR\"></span>");
            wifi_group();
        }

        {
            sets::Group r(b, "Дебаг");

            if (b.Select("Debug", "", "Нет;Serial;Logger;LCD;", &settings.debug)) {
                if (settings.debug != Debug::DBG_LCD)
                    ;
                else {
                    // settings.previusMode = MODE_DEBUG;
                    // settings.mode = MODE_DEBUG;
                    settings.debug_lvl = Debug::DBG_LVL_ALL;
                }

                /////
                if (settings.debug == Debug::DBG_NO) {  //&& settings.mode == MODE_DEBUG
                    // settings.mode = MODE_BIG_SECONDS;
                    // settings.previusMode = MODE_BIG_SECONDS;
                }

                b.reload();
            }
            if (settings.debug == Debug::DBG_SERIAL) b.LabelNum("Скорость порта", "", SERIAL_SPEED);

            if (ui.fine_tuning && settings.debug && settings.debug != Debug::DBG_LCD)
                if (b.Switch("Метки", "", &settings.color_debug)) b.reload();
            if (settings.debug)
                if (b.Select("Debug Level", "", "All;Info;Warning;Error;", &settings.debug_lvl)) b.reload();

            if (settings.debug > 0) b.Switch("Расширенное инфо в дебаг", "Будет выводить в дебаг больше информации", &settings.extra_debug);
            // if (settings.debug == Debug::DBG_LOGGER) {
            if (settings.debug == Debug::DBG_LOGGER) b.Log(logger);

            {
                sets::Buttons r(b);

                if (settings.debug == Debug::DBG_LOGGER || settings.debug == Debug::DBG_LCD)
                    if (b.Button("Очистить"))  //
                    {
                        logger.clear();

                        ui.notif("OK");
                        b.reload();
                    }

                if (b.Button("Инфо")) {
                    logger.clear();
                    ui.printDebugInfo();
                    ui.notif("OK");
                    b.reload();
                }
            }
            // }

        }  // sets::Group r(b, "Дебаг");
    }
    if (ui.fine_tuning && !settings.welcome_message)
        if (b.Button("показать приветствие", sets::Colors::Orange)) {
            settings.welcome_message = true;
            b.reload();
        }

    // Перезагрузить // Обновить экран
    {
        sets::Buttons r(b);
        if (b.Button("Перезагрузить", 0xEB5453)) {
            ui.notif("OK");
            b.reload();

            sett.tick();
            delay(200);
            ESP.restart();
        }  // 11337763
        if (b.Button("На экран")) {
            static unsigned long lastCallTime = 0;
            ui.show_display_button = false;
            if (millis() - lastCallTime < 10000) {
                ui.alert("Слишком часто");
                return;
            } else {
                // display.clearScreen();

                gluon.Display();

                ui.notif("OK");
                b.reload();
            }
            lastCallTime = millis();

            b.reload();
        }
    }

}  // build
void update(sets::Updater& u) {
    if (ui.show_cnfrm_reset) {
        ui.show_cnfrm_reset = false;
        u.confirm(H(reset_sett));
    }
    if (ui.show_cnfrm_delete) {
        ui.show_cnfrm_delete = false;
        u.confirm(H(delete_sett));
    }
    if (ui.show_alert) {
        ui.show_alert = false;
        u.alert(ui.alertStr);
    }
    if (ui.show_notif) {
        ui.show_notif = false;
        u.notice(ui.notifStr);
    }
}
size_t sketchSize;
void sett_init() {
    sett.begin(true, wifi.generateUniqueMDNSName(settings.ssid_ap).c_str());
    sett.onBuild(build);
    sett.onUpdate(update);
    switch (settings.color_theme) {
        case 0:
            sett.config.theme = sets::Colors::Black;
            break;
        case 1:
            sett.config.theme = sets::Colors::Gray;
            break;
        case 2:
            sett.config.theme = sets::Colors::White;
            break;
        case 3:
            sett.config.theme = sets::Colors::Red;
            break;
        case 4:
            sett.config.theme = sets::Colors::Orange;
            break;
        case 5:
            sett.config.theme = sets::Colors::Yellow;
            break;
        case 6:
            sett.config.theme = sets::Colors::Green;
            break;
        case 7:
            sett.config.theme = sets::Colors::Mint;
            break;
        case 8:
            sett.config.theme = sets::Colors::Aqua;
            break;
        case 9:
            sett.config.theme = sets::Colors::Blue;
            break;
        case 10:
            sett.config.theme = sets::Colors::Violet;
            break;
        case 11:
            sett.config.theme = sets::Colors::Pink;
            break;
        default:
            sett.config.theme = sets::Colors::Default;
            break;
    }

    sett.setProjectInfo("Часы " NAME "</br> Серийный номер: " SERIAL_NUMBER "</br>");  // , "https://github.com/TonTon-Macout/GLUONiCA";
    sett.setVersion(VERSION);
    sett.setUpdatePeriod(600);
    sett.setPopupTimeout(5500);

    sketchSize = ESP.getSketchSize();

    sett.onUpdateFWStart([]() {
        // led.on();
        DEBUG(SIMBOL_ERR, "==============================");
        DEBUG(SIMBOL_WAR, "==== OTA  UPDATE  STARTED ====");
        // printOta(223);
    });

    // sett.onUpdateFWProgress([](size_t current, size_t final) {
    //
    //    Serial.printf("progress: %u%%\r", (current / (sketchSize / 100)));
    //
    //    Serial.print("current ");
    //    Serial.println(current);
    //
    //    Serial.print("final ");
    //    Serial.println(final);
    //
    //    printOta(current / (sketchSize / 100));
    //});

    sett.onUpdateFWDone([](bool res) {
        // led.off();
        DEBUG(SIMBOL_INFO, "======== OTA FINISHED ========");
        DEBUG(SIMBOL_WAR, "========== RESTART  ==========");
        // printOta(225);
    });
}

//=========================================================================================//

//=========================================================================================//
void listFiles(const char* dirname, uint8_t level) {
    // Открываем директорию
    File root = LittleFS.open(dirname);
    if (!root) {
        DEBUG(SIMBOL_ERR, ("[LittleFS] Ошибка открытия директории: ") + String(dirname));
        return;
    }
    if (!root.isDirectory()) {
        DEBUG(SIMBOL_ERR, ("[LittleFS] Не является директорией: ") + String(dirname));
        return;
    }

    // Увеличиваем отступ для вложенных директорий
    String indent = "";
    for (uint8_t i = 0; i < level; i++) {
        indent += "  ";  // Два пробела для каждого уровня вложенности
    }

    // Обходим файлы в директории
    File file = root.openNextFile();
    while (file) {
        String fileName = file.name();
        if (file.isDirectory()) {
            DEBUG(SIMBOL_INFO, indent + F("[LittleFS] Директория: ") + fileName);
            // Рекурсивно обходим поддиректорию
            listFiles((String(dirname) + "/" + fileName).c_str(), level + 1);
        } else {
            DEBUG(SIMBOL_INFO, indent + ("[ФАЙЛ] ") + fileName + (", Размер: ") + String(file.size()) + (" байт"));
        }
        file = root.openNextFile();
    }
}
//=========================================================================================//
bool if_button() {
    if (knp_boot.click()) {
        DEBUG(SIMBOL_WAR, "КЛИК!");
        return true;
    }

    return false;
}
void count() {
    if (_setup_) return;  // сюды не заходим - поломается

    wifi.tick();
    ArduinoOTA.handle();
    sett.tick();
    if (WIFI && settings.mqtt) ha.loop();
    if_button();

    if (base.tick() == FD_WRITE) DEBUG(SIMBOL_INFO, "[BASE] Сохранили настройки");
    if (NTP.tick()) updt_time();

    static uint16_t tmr = status.get_quiet_mode() ? settings.sleep_duration_quiet_mode * 60 : settings.sleep_duration * 60;

    EVERY_MS(tmr * 1000) {
        DEBUG(SIMBOL_WAR, "=== Пора получить погоду ===");
        DEBUG(SIMBOL_INFO, status.get_quiet_mode() ? "тихий режим: " : "обычный режим", status.get_quiet_mode() ? String(settings.sleep_duration_quiet_mode * 60) : String(settings.sleep_duration * 60));
        updt_time();
        status.CheckSensors();
        if (WIFI && status.getWeather())
            DEBUG(SIMBOL_INFO, "Погода получена");
        else
            DEBUG(SIMBOL_ERR, "Ошибка получения погоды");

        gluon.Display();
        if (settings.mqtt && settings.send_mqtt && settings.mqtt_send_after_weather) {
            sendMqttData();
        }

        // uint16_t tmr = status.get_quiet_mode() ? settings.sleep_duration_quiet_mode * 60 : settings.sleep_duration * 60;
        // Timer_print.start(tmr*1000);
    }

    EVERY_S(10) {
        status.check_settings();
        tmr = status.get_quiet_mode() ? settings.sleep_duration_quiet_mode * 60 : settings.sleep_duration * 60;
    };

    EVERY_S(settings.period_mqtt * 60) {
        if (settings.mqtt && settings.send_mqtt && !settings.mqtt_send_after_weather) {
            sendMqttData();
        }
    }

    // раз в час
    static bool new_hour = false;
    if (minute == 0 && second <= 4 && !new_hour) {
        status.mqtt_count = 0;
        ha.resetReconnectAttempts();
        new_hour = true;
    } else if (new_hour && second > 4)
        new_hour = false;
}

void setup() {
    StartTime = millis();
    Serial.begin(SERIAL_SPEED);
    bool first = false;

    // ======== FILE SYSTEM ========
#ifdef ESP32
    LittleFS.begin(true);
#else
    LittleFS.begin();
#endif

    base.addWithoutWipe(true);
    FDstat_t stat = base.read();

    DEBUG(SIMBOL_WAR, F("========== ЗАГРУЗКА =========="));
    DEBUG(("Прошивка: "), VERSION);
    DEBUG(("Имя: "), settings.name);
    DEBUG(("Сетевое имя: "), settings.ssid_ap);

    gluon.InitialiseDisplay();
    bool boot_flag = if_button();

    DEBUG(F("\n"));
    if (!settings.debug)
        DEBUG(SIMBOL_WAR, F("Дебаг выключен"));
    else {
        if (settings.debug == Debug::DBG_SERIAL) DEBUG(SIMBOL_INFO, "Дебаг включен, SERIAL: " + String(SERIAL_SPEED));
        if (settings.debug == Debug::DBG_LCD) DEBUG(SIMBOL_INFO, "Дебаг включен, LCD");
        if (settings.debug == Debug::DBG_LOGGER) DEBUG(SIMBOL_INFO, "Дебаг включен, LOGGER");
    }

    DEBUG(SIMBOL_WAR, F("--------- FILE SYSTEM ---------"));
    boot_flag = if_button();
    switch (stat) {
        case FD_FS_ERR:
            DEBUG(SIMBOL_ERR, F("[File system] file system error!!!"));
            break;
        case FD_FILE_ERR:
            DEBUG(SIMBOL_ERR, F("[File system] file error"));
            break;
        case FD_WRITE:
            DEBUG(SIMBOL_INFO, F("[File system] first start, data write!"));
            first = true;
            break;
        case FD_ADD:
            DEBUG(SIMBOL_INFO, F("[File system] data add"));
            break;
        case FD_READ:
            DEBUG(SIMBOL_INFO, F("[File system] data read ok"));
            break;

        default:
            DEBUG(SIMBOL_ERR, F("[File system] file system n/a error!!!"));
            break;
    }
    DEBUG(SIMBOL_INFO, F("[LittleFS] Список файлов:"));
    if (settings.debug > 0) listFiles("/");

    if (settings.show_logo_screen) gluon.displayLogo();
    // === I2C ===
    DEBUG(SIMBOL_WAR, F("------------ I2C ------------"));
    boot_flag = if_button();
    status.I2C_Init();
    status.I2C_sensors_init();

    // ======== WIFI ========
    DEBUG(SIMBOL_WAR, F("------------ WIFI -----------"));
    boot_flag = if_button();
    wifi.begin();
    //
    if ((!settings.sleep && settings.show_logo_screen && settings.show_ip_in_logo_screen) ||
        first ||
        wifi.isAccessPoint() ||
        (boot_flag && settings.show_logo_screen && settings.show_ip_in_logo_screen))
        gluon.displayHost();

    // === NTP ===
    DEBUG(SIMBOL_WAR, F("------------ NTP ------------"));
    boot_flag = if_button();
    ntp_init();
    updt_time();

    // === WEATHER ===

    DEBUG(SIMBOL_WAR, F("------------ WEATHER ------------"));
    boot_flag = if_button();
    bool RxWeather = false;
    if (WIFI)
        RxWeather = status.getWeather();
    else
        DEBUG(SIMBOL_ERR, "[WEATHER] Неудалось подключиться к вайфай");
    if (RxWeather) DEBUG(SIMBOL_WAR, "[WEATHER] Погода получена!");

    DEBUG(SIMBOL_WAR, F("------------ SENSORS ------------"));
    bool sensr_avialable = status.CheckSensors();

    if (RxWeather || sensr_avialable)
        gluon.Display();
    else
        DEBUG(SIMBOL_ERR, "[DISPLAY] Нет данных для отображения!");

    if (settings.sleep && !boot_flag) {
        if (WIFI && settings.mqtt) {
            mqtt_init(true);
            sendMqttData();
        }
        BeginSleep();
    }

    wifi.setAutoDisableTimeout(settings.sleep_duration);
    wifi.resetActivityTimer();

    // === OTA ===
    ota_init();

    // === MQTT ===
    if (WIFI)
        if (settings.mqtt) {
            mqtt_init();
        }

    // ======== SETTINGS ========
    DEBUG(SIMBOL_WAR, "---------- SETTINGS ---------");
    sett_init();

    // ======== СТАРТУЕМ ========
    Datime dt = NTP.get();
    DEBUG(SIMBOL_WAR, "===============================");

    DEBUG(SIMBOL_ERR, "[START TIME] ", dt.dateToString() + " " + dt.timeToString());
    // if (settings.tele_start_message && !status.get_quiet_mode())
    //     tele.send("Стартууеем!!!\n" + String(settings.ssid_ap) + "\n" + dt.dateToString() + " " + dt.timeToString());

    uint16_t tmr = status.get_quiet_mode() ? settings.sleep_duration_quiet_mode * 60 : settings.sleep_duration * 60;
    Timer_print.start(tmr * 1000);

    DEBUG(SIMBOL_WAR, "======== СТАРТУУЕЕМ!!! ========");
    DEBUG(" \n");

    _setup_ = false;
}
// #########################################################################################
void loop() {
    count();
}

// #########################################################################################
void BeginSleep2() {
    updt_time();
    display.powerOff();
    ha.end();
    wifi.disableWiFi();

    long SleepTimer = settings.sleep_duration * 60;                  // theoretical sleep duration
    long offset = (minute % settings.sleep_duration) * 60 + second;  // number of seconds elapsed after last theoretical wake-up time point
    if (offset > settings.sleep_duration / 2 * 60) {                 // waking up too early will cause <offset> too large
        offset -= settings.sleep_duration * 60;                      // then we should make it negative, so as to extend this coming sleep duration
    }
    esp_sleep_enable_timer_wakeup((SleepTimer - offset) * 1000000LL);  // do compensation to cover ESP32 RTC timer source inaccuracies

    DEBUG("[SLEEP] Входим в " + String(SleepTimer) + " секундный период сна");
    DEBUG("[SLEEP] Бодрствовали: " + String((millis() - StartTime) / 1000.0, 3) + " секунд");
    DEBUG("[SLEEP] Запускаем период глубокого сна...");
    DEBUG(SIMBOL_WAR, "[SLEEP] Бай - бай!");

    esp_deep_sleep_start();  //
}
// #########################################################################################

void BeginSleep() {
    updt_time();
    display.powerOff();
    ha.end();
    wifi.disableWiFi();

    uint16_t interval_minutes = status.get_quiet_mode()
                                    ? settings.sleep_duration_quiet_mode
                                    : settings.sleep_duration;

    if (interval_minutes == 0) interval_minutes = 10;  // защита от нуля

    long SleepTimer = interval_minutes * 60LL;  // теоретический интервал

    // Сколько секунд прошло с начала текущего интервала
    long seconds_in_interval = (minute % interval_minutes) * 60 + second;

    // Время до следующего ровного момента
    long time_to_next = SleepTimer - seconds_in_interval;

    // Минимальный осмысленный сон (например, 30 секунд)
    const long MIN_SLEEP = 30;
    if (time_to_next < MIN_SLEEP) {
        time_to_next += SleepTimer;  // переходим к следующему полному интервалу
    }

    esp_sleep_enable_timer_wakeup(time_to_next * 1000000LL);
    DEBUG(SIMBOL_WAR, "[SLEEP] Город засыпает, просыпается мафия.");
    if (status.get_quiet_mode()) DEBUG(SIMBOL_WAR, "[SLEEP] Ночной режим активен, период сна увеличен");
    DEBUG("[SLEEP] Интервал: " + String(interval_minutes) + " мин");
    DEBUG("[SLEEP] Текущее: " + String(minute) + ":" + String(second));
    DEBUG("[SLEEP] До следующего ровного момента: " + String(time_to_next) + " сек");
    DEBUG("[SLEEP] Бодрствовали: " + String((millis() - StartTime) / 1000.0, 3) + " сек");
    DEBUG(SIMBOL_WAR, "[SLEEP] Бай - бай!");

    esp_deep_sleep_start();
}