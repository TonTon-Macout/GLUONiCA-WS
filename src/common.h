#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>    
#include <time.h>   

extern const int max_readings;
extern const int max_dayly_readings;

// Структуры 
typedef struct { // For current Day and Day 1, 2, 3, etc
  String Time;
  float  High;
  float  Low;
} HL_record_type;

HL_record_type  HLReadings[max_dayly_readings];

typedef struct {
  int      Dt;
  String   Period;
  String   Icon;
  String   Trend;
  String   Main0;
  String   Forecast0;
  String   Forecast1;
  String   Forecast2;
  String   Description;
  String   Time;
  String   Country;
  float    lat;
  float    lon;
  float    Temperature;
  float    FeelsLike;
  float    Humidity;
  float    DewPoint;
  float    High;
  float    Low;
  float    Winddir;
  float    Windspeed;
  float    Rainfall;
  float    Snowfall;
  float    Pressure;
  int      Cloudcover;
  int      Visibility;
  int      Sunrise;
  int      Sunset;
  int      Moonrise;
  int      Moonset;
  int      Timezone;
  float    UVI;
  float    PoP;
  float    Precipitation;
  bool     IsDay;

} Forecast_record_type;

Forecast_record_type  WxConditions[1];
Forecast_record_type  WxForecast[max_readings];
Forecast_record_type  Daily[max_dayly_readings];

// Прототипы
String ConvertUnixTime(int unix_time);
float mm_to_inches(float value_mm);
float hPa_to_inHg(float value_hPa);
int JulianDate(int d, int m, int y);
float SumOfPrecip(float DataArray[], int readings);
String TitleCase(String text);
double NormalizedMoonPhase(int d, int m, int y);

bool ReceiveOpenMeteoWeather(WiFiClient& client, bool print);
bool DecodeOpenMeteoWeatherFromString(const String& payload, bool print);

bool ReceiveMetNoWeather(WiFiClient& client, bool print);
bool DecodeMetNoWeatherFromStream(Stream& stream, bool print);
String MetNoSymbolToIcon(const String& symbol_code);

String WMO_to_Icon(int code, bool is_day = true);
int iso_to_unix(const char* iso);


//Open-Meteo.com #################################################################################
bool ReceiveOpenMeteoWeather(WiFiClient& client, bool print) {
  if(print) DEBUG(SIMBOL_WAR, "[OPEN-METEO][Receive] FreeHeap: ", ESP.getFreeHeap(), " bytes");

  client.stop(); // close connection before sending a new request

  HTTPClient http;

  String uri_hourly = "/v1/forecast?"
               "latitude=" + String(settings.latitude, 5) +
               "&longitude=" + String(settings.longitude, 5) +
               "&current=temperature_2m,apparent_temperature,dewpoint_2m,relative_humidity_2m,"
               "pressure_msl,cloud_cover,visibility,wind_speed_10m,wind_direction_10m,"
               "precipitation,rain,snowfall,weather_code,uv_index,is_day"
               "&hourly=temperature_2m,apparent_temperature,dewpoint_2m,relative_humidity_2m,"
               "pressure_msl,cloud_cover,visibility,wind_speed_10m,wind_direction_10m,"
               "rain,snowfall,weather_code,precipitation_probability"
               "&daily=weather_code,temperature_2m_max,temperature_2m_min,"
               "precipitation_sum,rain_sum,snowfall_sum,"
               "precipitation_probability_max,uv_index_max,sunrise,sunset"
               "&timezone=auto"
               "&forecast_days=7";


  // Создаём строковую переменную uri, в которой будет сформирован полный запрос к API Open-Meteo
  String uri_hourly_24h = "/v1/forecast?"                    // Начало пути API + знак вопроса (начало параметров)
            // Добавляем координаты местоположения
            "latitude=" + String(settings.latitude) +     // Широта 
            "&longitude=" + String(settings.longitude) +  // Долгота

            // === Текущая погода (current) ===
            "&current=temperature_2m,apparent_temperature,dewpoint_2m,relative_humidity_2m,"
            // Запрашиваем текущие значения следующих параметров:
            // temperature_2m           → температура воздуха 2 м над землёй (°C)
            // apparent_temperature     → ощущаемая температура (°C)
            // dewpoint_2m              → точка росы (°C)
            // relative_humidity_2m     → относительная влажность (%)

            "surface_pressure,cloud_cover,visibility,wind_speed_10m,wind_direction_10m,"
            // surface_pressure             → атмосферное давление на уровне земли (гПа)
            // cloud_cover              → облачность (% от 0 до 100)
            // visibility               → видимость (в метрах)
            // wind_speed_10m           → скорость ветра на высоте 10 м (км/ч)
            // wind_direction_10m       → направление ветра (градусы)

            "precipitation,rain,snowfall,weather_code,uv_index,is_day"
            // precipitation            → общее количество осадков за последний час (мм)
            // rain                     → количество дождя (мм)
            // snowfall                 → количество снега (см)
            // weather_code             → код погоды по стандарту WMO (0 = ясно, 61 = дождь и т.д.)
            // uv_index                 → ультрафиолетовый индекс
            // is_day                   → 1 = день, 0 = ночь

            // === Почасовой прогноз ТОЛЬКО на сегодня (24 часа) ===
            "&hourly=temperature_2m,apparent_temperature,dewpoint_2m,relative_humidity_2m,"
            // Запрашиваем почасовые данные на сегодняшний день:
            // temperature_2m           → температура воздуха 2 м (°C)
            // apparent_temperature     → ощущаемая температура (°C)
            // dewpoint_2m              → точка росы (°C)
            // relative_humidity_2m     → относительная влажность (%)

            "pressure_msl,cloud_cover,visibility,wind_speed_10m,wind_direction_10m,"
            // pressure_msl             → атмосферное давление (гПа)
            // cloud_cover              → облачность (%)
            // visibility               → видимость (метры)
            // wind_speed_10m           → скорость ветра (км/ч)
            // wind_direction_10m       → направление ветра (градусы)

            "rain,snowfall,weather_code,precipitation_probability"
            // rain                     → количество дождя (мм)
            // snowfall                 → количество снега (см)
            // weather_code             → код погоды по WMO
            // precipitation_probability→ вероятность осадков (%)

            // === Суточный прогноз на 7 дней (daily) ===
            "&daily=weather_code,temperature_2m_max,temperature_2m_min,"
            // Запрашиваем данные на каждый день (7 дней):
            // weather_code             → код погоды на день
            // temperature_2m_max       → максимальная температура за день (°C)
            // temperature_2m_min       → минимальная температура за день (°C)

            "precipitation_sum,rain_sum,snowfall_sum,"
            // precipitation_sum        → суммарное количество осадков за день (мм)
            // rain_sum                 → сумма дождя за день (мм)
            // snowfall_sum             → сумма снега за день (см)

            "precipitation_probability_max,uv_index_max,sunrise,sunset"
            // precipitation_probability_max → максимальная вероятность осадков за день (%)
            // uv_index_max             → максимальный УФ-индекс за день
            // sunrise                  → время восхода солнца
            // sunset                   → время заката солнца

            // === Дополнительные параметры ===
            "&timezone=auto"           // Автоматически определять часовой пояс по координатам
                                       // (очень удобно — не нужно вручную указывать Europe/Moscow)

            "&forecast_days=7";        // Запрашивать прогноз на 7 дней вперёд
                                       // Для daily — 7 дней
                                       // Для hourly — только сегодняшний день (24 часа) 


  // Создаём строковую переменную uri, в которой будет сформирован полный запрос к API Open-Meteo
  String uri = "/v1/forecast?"                    // Начало пути API + знак вопроса (начало параметров)
            // Добавляем координаты местоположения
            "latitude=" + String(settings.latitude) +     // Широта 
            "&longitude=" + String(settings.longitude) +  // Долгота

            // === Текущая погода (current) ===
            "&current=temperature_2m,apparent_temperature,dewpoint_2m,relative_humidity_2m,"
            // Запрашиваем текущие значения следующих параметров:
            // temperature_2m           → температура воздуха 2 м над землёй (°C)
            // apparent_temperature     → ощущаемая температура (°C)
            // dewpoint_2m              → точка росы (°C)
            // relative_humidity_2m     → относительная влажность (%)

            "surface_pressure,cloud_cover,visibility,wind_speed_10m,wind_direction_10m,"
            // surface_pressure             → атмосферное давление на уровне земли (гПа)
            // cloud_cover              → облачность (% от 0 до 100)
            // visibility               → видимость (в метрах)
            // wind_speed_10m           → скорость ветра на высоте 10 м (км/ч)
            // wind_direction_10m       → направление ветра (градусы)

            "precipitation,rain,snowfall,weather_code,uv_index,is_day"
            // precipitation            → общее количество осадков за последний час (мм)
            // rain                     → количество дождя (мм)
            // snowfall                 → количество снега (см)
            // weather_code             → код погоды по стандарту WMO (0 = ясно, 61 = дождь и т.д.)
            // uv_index                 → ультрафиолетовый индекс
            // is_day                   → 1 = день, 0 = ночь

            // === Суточный прогноз на 7 дней (daily) ===
            "&daily=weather_code,temperature_2m_max,temperature_2m_min,"
            // Запрашиваем данные на каждый день (7 дней):
            // weather_code             → код погоды на день
            // temperature_2m_max       → максимальная температура за день (°C)
            // temperature_2m_min       → минимальная температура за день (°C)

            "precipitation_sum,rain_sum,snowfall_sum,"
            // precipitation_sum        → суммарное количество осадков за день (мм)
            // rain_sum                 → сумма дождя за день (мм)
            // snowfall_sum             → сумма снега за день (см)

            "precipitation_probability_max,uv_index_max,sunrise,sunset"
            // precipitation_probability_max → максимальная вероятность осадков за день (%)
            // uv_index_max             → максимальный УФ-индекс за день
            // sunrise                  → время восхода солнца
            // sunset                   → время заката солнца

            // === Дополнительные параметры ===
            "&timezone=auto"           // Автоматически определять часовой пояс по координатам
                                       // (очень удобно — не нужно вручную указывать Europe/Moscow)

            "&forecast_days=7";        // Запрашивать прогноз на 7 дней вперёд
                                       // Для daily — 7 дней
                                       // Для hourly — только сегодняшний день (24 часа) 

  if (!http.begin(client, "api.open-meteo.com", 80, uri, true)) {  // true = HTTPS
    DEBUG(SIMBOL_ERR, "[WEATHER] HTTP begin failed");
    return false;
  }

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    
    String payload = http.getString();

    DEBUG(SIMBOL_INFO, "[OPEN-METEO.COM] SUCCESS! Payload size: " + String(payload.length()) + " bytes");
    if(print) DEBUG(SIMBOL_WAR, "[OPEN-METEO][Receive] FreeHeap: ", ESP.getFreeHeap(), " bytes");

    // Показываем первые 400 символов (чтобы увидеть, JSON ли это)
   //if (payload.length() > 400) {
   //  DEBUG("First 400 chars:");
   //  DEBUG(payload.substring(0, 400));
   //} else {
   //  DEBUG("Full payload:");
   //  DEBUG(payload);
   //}


    bool ok = DecodeOpenMeteoWeatherFromString(payload, print);

    http.end();
    client.stop();
    return ok;
  } else {
    DEBUG(SIMBOL_ERR, "[WEATHER] connection failed, error:",  http.errorToString(httpCode).c_str(), "(code " + String(httpCode)  + ")");

    http.end();
    client.stop();
    return false;
  }
}

bool DecodeOpenMeteoWeatherFromString(const String& payload, bool print) {
  if (print) DEBUG(SIMBOL_WAR, "===== Decoding Data from Open-Meteo ======");

  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    DEBUG(SIMBOL_ERR, "deserializeJson() failed: ", error.c_str());
    if(print) DEBUG(SIMBOL_WAR, "[OPEN-METEO][Decode] FreeHeap: ", ESP.getFreeHeap(), " bytes");

    if (payload.length() > 200) {
      DEBUG("Problematic part around beginning:");
      DEBUG(payload.substring(0, 200));
      DEBUG("...");
      DEBUG(payload.substring(payload.length() - 200));
    } else {
      DEBUG("Full payload (small):");
      DEBUG(payload);
    }

    return false;
  }

  // CURRENT conditions
  if (print) DEBUG(SIMBOL_WAR, "=== CURRENT ==============");

  JsonObject current = doc["current"];

  WxConditions[0].Timezone    = doc["utc_offset_seconds"] | 0;
  
  const char* time_str = current["time"];
  WxConditions[0].Dt = iso_to_unix(time_str);
  
  WxConditions[0].lat         = doc["latitude"]  | 0.0f;
  WxConditions[0].lon         = doc["longitude"] | 0.0f;

  WxConditions[0].Temperature   = current["temperature_2m"]       | 0.0f;
  WxConditions[0].FeelsLike     = current["apparent_temperature"] | 0.0f;
  WxConditions[0].DewPoint      = current["dewpoint_2m"]          | 0.0f;
  WxConditions[0].Humidity      = current["relative_humidity_2m"] | 0;
  WxConditions[0].Pressure      = current["surface_pressure"]     | 0.0f;
  WxConditions[0].Cloudcover    = current["cloud_cover"]          | 0;
  WxConditions[0].Visibility    = current["visibility"]           | 0;
  WxConditions[0].Windspeed     = current["wind_speed_10m"]       | 0.0f;
  WxConditions[0].Winddir       = current["wind_direction_10m"]   | 0.0f;
  WxConditions[0].Rainfall      = current["rain"] | current["precipitation"] | 0.0f;
  WxConditions[0].Precipitation = current["precipitation_sum"] | current["precipitation"];
  WxConditions[0].Snowfall      = current["snowfall"] | 0.0f;
  WxConditions[0].UVI           = current["uv_index"] | 0.0f;

  WxConditions[0].IsDay         = current["is_day"] | 1;

  int wmo_code                  = current["weather_code"] | 0;
  bool is_day                   = current["is_day"] | 1;
  WxConditions[0].Icon        = WMO_to_Icon(wmo_code, is_day);
  WxConditions[0].Description = "";  // 

  // Sunrise / Sunset из первого дня daily
  JsonArray sunrise_arr = doc["daily"]["sunrise"];
  JsonArray sunset_arr  = doc["daily"]["sunset"];
  if (sunrise_arr.size() > 0) {
    WxConditions[0].Sunrise = iso_to_unix(sunrise_arr[0].as<const char*>());
    WxConditions[0].Sunset  = iso_to_unix(sunset_arr[0].as<const char*>());
  } else {
    WxConditions[0].Sunrise = 0;
    WxConditions[0].Sunset  = 0;
  }

  if (print) {
    if(settings.extra_debug)  DEBUG("Unix: " + String(WxConditions[0].Dt));
    if(settings.extra_debug) DEBUG("Timezone: " + String(WxConditions[0].Timezone));
    String dateStr = ConvertUnixTime(WxConditions[0].Dt);
    DEBUG("Date: " + dateStr);

    if(settings.extra_debug) DEBUG("---");
    DEBUG("Temp: " + String(WxConditions[0].Temperature));
    if(settings.extra_debug) DEBUG("Icon: " + WxConditions[0].Icon);
    if(settings.extra_debug) DEBUG("Pres(Pa): " + String(WxConditions[0].Pressure));
    if(settings.extra_debug) DEBUG("Humi: " + String(WxConditions[0].Humidity));
    DEBUG("Rain: " + String(WxConditions[0].Rainfall));
    DEBUG("Snow: " + String(WxConditions[0].Snowfall));
  }





  // HOURLY data
  if (print) DEBUG(SIMBOL_WAR, "=== HOURLY ==============");

  JsonArray h_time = doc["hourly"]["time"];
  int h_count = min((int)h_time.size(), max_readings);

  for (int r = 0; r < h_count; r++) {
    if (print) DEBUG("Hour " + String(r) + " --------------");

    WxForecast[r].Dt          = iso_to_unix(h_time[r].as<const char*>());
    WxForecast[r].Temperature = doc["hourly"]["temperature_2m"][r]       | 0.0f;
    WxForecast[r].FeelsLike   = doc["hourly"]["apparent_temperature"][r] | 0.0f;
    WxForecast[r].DewPoint    = doc["hourly"]["dewpoint_2m"][r]          | 0.0f;
    WxForecast[r].Humidity    = doc["hourly"]["relative_humidity_2m"][r] | 0;
    WxForecast[r].Pressure    = doc["hourly"]["pressure_msl"][r]         | 0.0f;
    WxForecast[r].Cloudcover  = doc["hourly"]["cloud_cover"][r]          | 0;
    WxForecast[r].Visibility  = doc["hourly"]["visibility"][r]           | 0;
    WxForecast[r].Windspeed   = doc["hourly"]["wind_speed_10m"][r]       | 0.0f;
    WxForecast[r].Winddir     = doc["hourly"]["wind_direction_10m"][r]   | 0.0f;
    WxForecast[r].Rainfall    = doc["hourly"]["rain"][r]                 | 0.0f;
    WxForecast[r].Snowfall    = doc["hourly"]["snowfall"][r]             | 0.0f;
    WxForecast[r].PoP         = (doc["hourly"]["precipitation_probability"][r] | 0) / 100.0f;
    WxForecast[r].Precipitation = (doc["hourly"]["precipitation_sum"][r] | 0) / 100.0f;

    int code = doc["hourly"]["weather_code"][r] | 0;
    WxForecast[r].Icon = WMO_to_Icon(code);

    if (print) {
      if(settings.extra_debug) DEBUG("Unix: " + String(WxForecast[r].Dt));
      String dateStr = ConvertUnixTime(WxForecast[r].Dt);
      DEBUG("Dt: " + dateStr);
      DEBUG("Temp: " + String(WxForecast[r].Temperature));
      if(settings.extra_debug) DEBUG("Icon: " + WxForecast[r].Icon);
      DEBUG("Precip: " + String(WxConditions[r].Precipitation));
    }
  }

  // Обнуляем остаток массива, если данных меньше max_readings
  for (int r = h_count; r < max_readings; r++) {
    WxForecast[r].Dt = 0;
  }








  // DAILY data
  if (print) DEBUG(SIMBOL_WAR, "=== DAILY ==============");

  JsonArray d_time = doc["daily"]["time"];
  int d_count = min((int)d_time.size(), 8);

  for (int r = 0; r < d_count; r++) {
    if (print) DEBUG("\nData for DAY - " + String(r) + " --------------");

    Daily[r].Dt          = iso_to_unix(d_time[r].as<const char*>());
    Daily[r].High        = doc["daily"]["temperature_2m_max"][r] | 0.0f;
    Daily[r].Low         = doc["daily"]["temperature_2m_min"][r] | 0.0f;
    Daily[r].Rainfall    = doc["daily"]["rain_sum"][r]           | 0.0f;
    Daily[r].Snowfall    = doc["daily"]["snowfall_sum"][r]       | 0.0f;
    Daily[r].PoP         = (doc["daily"]["precipitation_probability_max"][r] | 0) / 100.0f;
    Daily[r].UVI         = doc["daily"]["uv_index_max"][r]       | 0.0f;
    Daily[r].Humidity    = 0;  // нет в daily
    Daily[r].Description = "";
    Daily[r].Precipitation = doc["daily"]["precipitation_sum"][r] | 0.0f;

    int code = doc["daily"]["weather_code"][r] | 0;
    Daily[r].Icon = WMO_to_Icon(code);

    if (print) {
      if(settings.extra_debug)DEBUG("Unix: " + String(Daily[r].Dt));
      String dateStr = ConvertUnixTime(Daily[r].Dt);
      DEBUG("Date: " + dateStr);
      DEBUG("High Temp  : " + String(Daily[r].High));
      DEBUG("Low  Temp  : " + String(Daily[r].Low));
      if(settings.extra_debug)DEBUG("Icon: " + Daily[r].Icon);
      if(settings.extra_debug)DEBUG("Rain: " + String(Daily[r].Rainfall));
      if(settings.extra_debug)DEBUG("Snow: " + String(Daily[r].Snowfall));
      DEBUG("Precipitation: " + String(Daily[r].Precipitation));
    }
  }

  for (int r = d_count; r < 8; r++) {
    Daily[r].Dt = 0;
  }

  // Тренд давления
  float pressure_trend = 0.0f;
  if (max_readings >= 3 && WxForecast[2].Dt != 0) {
    pressure_trend = WxForecast[0].Pressure - WxForecast[2].Pressure;
  } else if (max_readings >= 2 && WxForecast[1].Dt != 0) {
    pressure_trend = WxForecast[0].Pressure - WxForecast[1].Pressure;
  }
  pressure_trend = ((int)(pressure_trend * 10)) / 10.0f;

  WxConditions[0].Trend = "=";
  if (pressure_trend > 0)  WxConditions[0].Trend = "+";
  if (pressure_trend < 0)  WxConditions[0].Trend = "-";
  if (pressure_trend == 0) WxConditions[0].Trend = "0";


  if(print) DEBUG(SIMBOL_WAR, "[OPEN-METEO][Decode] FreeHeap: ", ESP.getFreeHeap(), " bytes");
  return true;
}

bool DecodeOpenMeteoWeatherFromStringHourly(const String& payload, bool print) {
  if (print) DEBUG(SIMBOL_WAR, "===== Decoding Data from Open-Meteo ======");

  //StaticJsonDocument<8192> doc;  // 12 КБ — 
  JsonDocument doc;

  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    DEBUG(SIMBOL_ERR, "deserializeJson() failed: ", error.c_str());

    // Для отладки: выводим начало и конец строки, если ошибка
    if (payload.length() > 200) {
      DEBUG("Problematic part around beginning:");
      DEBUG(payload.substring(0, 200));
      DEBUG("...");
      DEBUG(payload.substring(payload.length() - 200));
    } else {
      DEBUG("Full payload (small):");
      DEBUG(payload);
    }

    return false;
  }

  // CURRENT conditions
  if (print) DEBUG(SIMBOL_WAR, "=== CURRENT conditions ===");

  JsonObject current = doc["current"];

  WxConditions[0].Timezone    = doc["utc_offset_seconds"] | 0;
  WxConditions[0].lat         = doc["latitude"]  | 0.0f;
  WxConditions[0].lon         = doc["longitude"] | 0.0f;

  WxConditions[0].Temperature = current["temperature_2m"]       | 0.0f;
  WxConditions[0].FeelsLike   = current["apparent_temperature"] | 0.0f;
  WxConditions[0].DewPoint    = current["dewpoint_2m"]          | 0.0f;
  WxConditions[0].Humidity    = current["relative_humidity_2m"] | 0;
  WxConditions[0].Pressure    = current["pressure_msl"]         | 0.0f;
  WxConditions[0].Cloudcover  = current["cloud_cover"]          | 0;
  WxConditions[0].Visibility  = current["visibility"]           | 0;
  WxConditions[0].Windspeed   = current["wind_speed_10m"]       | 0.0f;
  WxConditions[0].Winddir     = current["wind_direction_10m"]   | 0.0f;
  WxConditions[0].Rainfall    = current["rain"] | current["precipitation"] | 0.0f;
  WxConditions[0].Snowfall    = current["snowfall"] | 0.0f;
  WxConditions[0].UVI         = current["uv_index"] | 0.0f;

  int wmo_code = current["weather_code"] | 0;
  bool is_day  = current["is_day"] | 1;
  WxConditions[0].Icon        = WMO_to_Icon(wmo_code, is_day);
  WxConditions[0].Description = "";  // нет прямого аналога description, оставляем пустым

  // Sunrise / Sunset из первого дня daily
  JsonArray sunrise_arr = doc["daily"]["sunrise"];
  JsonArray sunset_arr  = doc["daily"]["sunset"];
  if (sunrise_arr.size() > 0) {
    WxConditions[0].Sunrise = iso_to_unix(sunrise_arr[0].as<const char*>());
    WxConditions[0].Sunset  = iso_to_unix(sunset_arr[0].as<const char*>());
  } else {
    WxConditions[0].Sunrise = 0;
    WxConditions[0].Sunset  = 0;
  }

  if (print) {
    DEBUG("Temp: " + String(WxConditions[0].Temperature));
    DEBUG("Icon: " + WxConditions[0].Icon);
    DEBUG("Pres (Pa): " + String(WxConditions[0].Pressure));
    DEBUG("Humi: " + String(WxConditions[0].Humidity));
    DEBUG("Rain: " + String(WxConditions[0].Rainfall));
  }

  // HOURLY data
  if (print) DEBUG(SIMBOL_WAR, "=== HOURLY data ===");

  JsonArray h_time = doc["hourly"]["time"];
  int h_count = min((int)h_time.size(), max_readings);

  for (int r = 0; r < h_count; r++) {
    if (print) DEBUG("Hour " + String(r) + " --------------");

    WxForecast[r].Dt          = iso_to_unix(h_time[r].as<const char*>());
    WxForecast[r].Temperature = doc["hourly"]["temperature_2m"][r]       | 0.0f;
    WxForecast[r].FeelsLike   = doc["hourly"]["apparent_temperature"][r] | 0.0f;
    WxForecast[r].DewPoint    = doc["hourly"]["dewpoint_2m"][r]          | 0.0f;
    WxForecast[r].Humidity    = doc["hourly"]["relative_humidity_2m"][r] | 0;
    WxForecast[r].Pressure    = doc["hourly"]["pressure_msl"][r]         | 0.0f;
    WxForecast[r].Cloudcover  = doc["hourly"]["cloud_cover"][r]          | 0;
    WxForecast[r].Visibility  = doc["hourly"]["visibility"][r]           | 0;
    WxForecast[r].Windspeed   = doc["hourly"]["wind_speed_10m"][r]       | 0.0f;
    WxForecast[r].Winddir     = doc["hourly"]["wind_direction_10m"][r]   | 0.0f;
    WxForecast[r].Rainfall    = doc["hourly"]["rain"][r]                 | 0.0f;
    WxForecast[r].Snowfall    = doc["hourly"]["snowfall"][r]             | 0.0f;
    WxForecast[r].PoP         = (doc["hourly"]["precipitation_probability"][r] | 0) / 100.0f;

    int code = doc["hourly"]["weather_code"][r] | 0;
    WxForecast[r].Icon = WMO_to_Icon(code);

    if (print) {
      DEBUG("Unix: " + String(WxForecast[r].Dt));
      String dateStr = ConvertUnixTime(WxForecast[r].Dt);
      DEBUG("Dt: " + dateStr);
      DEBUG("Temp: " + String(WxForecast[r].Temperature));
      DEBUG("Icon: " + WxForecast[r].Icon);
      DEBUG("Rain: " + String(WxConditions[r].Rainfall));
    }
  }

  // Обнуляем остаток массива, если данных меньше max_readings
  for (int r = h_count; r < max_readings; r++) {
    WxForecast[r].Dt = 0;
  }

  // DAILY data
  if (print) DEBUG(SIMBOL_WAR, "=== DAILY Data ==============");

  JsonArray d_time = doc["daily"]["time"];
  int d_count = min((int)d_time.size(), 8);

  for (int r = 0; r < d_count; r++) {
    if (print) DEBUG("\nData for DAY - " + String(r) + " --------------");

    Daily[r].Dt          = iso_to_unix(d_time[r].as<const char*>());
    Daily[r].High        = doc["daily"]["temperature_2m_max"][r] | 0.0f;
    Daily[r].Low         = doc["daily"]["temperature_2m_min"][r] | 0.0f;
    Daily[r].Rainfall    = doc["daily"]["rain_sum"][r]           | 0.0f;
    Daily[r].Snowfall    = doc["daily"]["snowfall_sum"][r]       | 0.0f;
    Daily[r].PoP         = (doc["daily"]["precipitation_probability_max"][r] | 0) / 100.0f;
    Daily[r].UVI         = doc["daily"]["uv_index_max"][r]       | 0.0f;
    Daily[r].Humidity    = 0;  // нет в daily
    Daily[r].Description = "";

    int code = doc["daily"]["weather_code"][r] | 0;
    Daily[r].Icon = WMO_to_Icon(code);

    if (print) {
      DEBUG("Unix: " + String(Daily[r].Dt));
      String dateStr = ConvertUnixTime(Daily[r].Dt);
      DEBUG("Date: " + dateStr);
      DEBUG("High   : " + String(Daily[r].High));
      DEBUG("Low    : " + String(Daily[r].Low));
      DEBUG("Icon   : " + Daily[r].Icon);
      DEBUG("Rain   : " + String(Daily[r].Rainfall));
    }
  }

  for (int r = d_count; r < 8; r++) {
    Daily[r].Dt = 0;
  }

  // Тренд давления
  float pressure_trend = 0.0f;
  if (max_readings >= 3 && WxForecast[2].Dt != 0) {
    pressure_trend = WxForecast[0].Pressure - WxForecast[2].Pressure;
  } else if (max_readings >= 2 && WxForecast[1].Dt != 0) {
    pressure_trend = WxForecast[0].Pressure - WxForecast[1].Pressure;
  }
  pressure_trend = ((int)(pressure_trend * 10)) / 10.0f;

  WxConditions[0].Trend = "=";
  if (pressure_trend > 0)  WxConditions[0].Trend = "+";
  if (pressure_trend < 0)  WxConditions[0].Trend = "-";
  if (pressure_trend == 0) WxConditions[0].Trend = "0";



  return true;
}

/* 
Code	Description
0	Clear sky
1, 2, 3	Mainly clear, partly cloudy, and overcast
45, 48	Fog and depositing rime fog
51, 53, 55	Drizzle: Light, moderate, and dense intensity
56, 57	Freezing Drizzle: Light and dense intensity
61, 63, 65	Rain: Slight, moderate and heavy intensity
66, 67	Freezing Rain: Light and heavy intensity
71, 73, 75	Snow fall: Slight, moderate, and heavy intensity
77	Snow grains
80, 81, 82	Rain showers: Slight, moderate, and violent
85, 86	Snow showers slight and heavy
95 *	Thunderstorm: Slight or moderate
96, 99 *	Thunderstorm with slight and heavy hail
 */
String WMO_to_Icon(int code, bool is_day) {
  String suffix = is_day ? "d" : "n";
  if (code == 0)                  return "01" + suffix;
  if (code == 1 || code == 2)     return "02" + suffix;
  if (code == 3)                  return "03" + suffix;
  if (code >= 45 && code <= 48)   return "50" + suffix;
  if (code >= 51 && code <= 67)   return "09" + suffix;
  if (code >= 71 && code <= 77)   return "13" + suffix;
  if (code >= 80 && code <= 82)   return "10" + suffix;
  if (code >= 95 && code <= 99)   return "11" + suffix;
  return "04" + suffix;  // overcast
}



//MET.NO #########################################################################################
bool ReceiveMetNoWeather(WiFiClient& client, bool print) {
  client.stop();
  if(print) DEBUG(SIMBOL_WAR, "[MET.NO][Receive] FreeHeap: ", ESP.getFreeHeap(), " bytes");
  HTTPClient http;

  String url = "https://api.met.no/weatherapi/locationforecast/2.0/compact?lat=" 
               + String(settings.latitude, 4) 
               + "&lon=" + String(settings.longitude, 4);

  // Можно добавить высоту в метрах?
  // url += "&altitude=150";

  if (!http.begin(url)) {   
    DEBUG(SIMBOL_ERR, "[MET.NO] HTTP begin failed");
    return false;
  }

  String userAgent = NETWORK_NAME " https://github.com/TonTon-Macout/GLUONiCA-WS";
  http.setUserAgent(userAgent);
  http.addHeader("Accept", "application/json");

  if(print) DEBUG(SIMBOL_INFO, "[MET.NO] Url: " + url);
  if(print) DEBUG(SIMBOL_INFO, "[MET.NO] User-Agent: " + userAgent);

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {

    //String payload = http.getString();
    //DEBUG(SIMBOL_INFO, "[MET.NO] SUCCESS! Payload size: " + String(payload.length()) + " bytes");
   // bool ok = DecodeMetNoWeatherFromString(payload, print);
    bool ok = DecodeMetNoWeatherFromStream(http.getStream(), print);
    http.end();
    client.stop();
    return ok;
  } 
  else {
    DEBUG(SIMBOL_ERR, "[MET.NO] HTTP error: " + http.errorToString(httpCode) + " (code " + String(httpCode) + ")");

    String responseBody = http.getString();
    if (responseBody.length() > 0) {
      DEBUG(SIMBOL_WAR, "[MET.NO] Server reply: " + responseBody.substring(0, 600));
    } else {
      DEBUG(SIMBOL_WAR, "[MET.NO] Server reply: (empty body)");
    }

    DEBUG(SIMBOL_WAR, "Ссылка: " + url);
    DEBUG(SIMBOL_WAR, "User Agent: " + userAgent);

    http.end();
    client.stop();
    return false;
  }
}

bool DecodeMetNoWeatherFromStream(Stream& stream, bool print) {
  if (print) DEBUG(SIMBOL_WAR, "===== Decoding Data from MET.NO =====");

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, stream);

  if (error) {
    DEBUG(SIMBOL_ERR, "deserializeJson failed: ", error.c_str());
    DEBUG(SIMBOL_WAR, "FreeHeap: ", ESP.getFreeHeap(), " bytes");
    return false;
  }

  JsonObject properties = doc["properties"];
  JsonArray timeseries  = properties["timeseries"];

  if (timeseries.size() == 0) {
    DEBUG(SIMBOL_ERR, "[MET.NO] No timeseries data");
    return false;
  }

  // координаты и высота
  float lat = doc["geometry"]["coordinates"][1] | settings.latitude;
  float lon = doc["geometry"]["coordinates"][0] | settings.longitude;
  float h   = doc["geometry"]["coordinates"][2] | 0.0f;

  const float g = 9.81f;
  const float R = 287.0f;

  // ==================== CURRENT ====================
  if (print) DEBUG(SIMBOL_WAR, "=== CURRENT ===");

  JsonObject now = timeseries[0]["data"]["instant"]["details"];

  float tC = now["air_temperature"] | 0.0f;
  float tK = tC + 273.15f;
  float p0 = now["air_pressure_at_sea_level"] | 0.0f;
  float p  = p0 * expf(-(g * h) / (R * tK));

  WxConditions[0].Dt          = iso_to_unix(timeseries[0]["time"]) + settings.timeZone * 3600;
  WxConditions[0].Timezone    = settings.timeZone * 3600;
  WxConditions[0].lat         = lat;
  WxConditions[0].lon         = lon;

  WxConditions[0].Temperature = tC;
  WxConditions[0].FeelsLike   = tC;
  WxConditions[0].DewPoint    = now["dew_point_temperature"] | 0.0f;
  WxConditions[0].Humidity    = now["relative_humidity"] | 0;
  WxConditions[0].Pressure    = p;

  WxConditions[0].Cloudcover  = now["cloud_area_fraction"] | 0;
  WxConditions[0].Windspeed   = now["wind_speed"] | 0.0f;
  WxConditions[0].Winddir     = now["wind_from_direction"] | 0.0f;
  WxConditions[0].Visibility  = 0; // нет
  WxConditions[0].UVI         = 0.0f; // нет

  // осадки + тип
  JsonObject next1h = timeseries[0]["data"]["next_1_hours"];

  if (!next1h.isNull()) {
    float precip = next1h["details"]["precipitation_amount"] | 0.0f;
    const char* symbol = next1h["summary"]["symbol_code"] | "";

    WxConditions[0].Precipitation = precip;

    if (strstr(symbol, "snow")) {
      WxConditions[0].Snowfall = precip;
      WxConditions[0].Rainfall = 0.0f;
    } else if (strstr(symbol, "sleet")) {
      WxConditions[0].Snowfall = precip * 0.5f;
      WxConditions[0].Rainfall = precip * 0.5f;
    } else {
      WxConditions[0].Rainfall = precip;
      WxConditions[0].Snowfall = 0.0f;
    }

    WxConditions[0].Icon = MetNoSymbolToIcon(symbol);
  } else {
    WxConditions[0].Rainfall = 0;
    WxConditions[0].Snowfall = 0;
    WxConditions[0].Precipitation = 0;
    WxConditions[0].Icon = "04d";
  }

  //WxConditions[0].IsDay = true;
  if (print) {
    DEBUG("Temp: " + String(WxConditions[0].Temperature));
    DEBUG("Icon: " + WxConditions[0].Icon);
    DEBUG("Rain (1h): " + String(WxConditions[0].Rainfall));
    DEBUG("Snow (1h): " + String(WxConditions[0].Snowfall));
  }
  // ==================== HOURLY ====================
  if (print) DEBUG(SIMBOL_WAR, "=== HOURLY ===");

  int h_count = 0;

  for (size_t i = 0; i < timeseries.size() && h_count < max_readings; i++) {

    if (!timeseries[i]["data"].containsKey("next_1_hours")) continue;

    JsonObject details = timeseries[i]["data"]["instant"]["details"];
    JsonObject next1h  = timeseries[i]["data"]["next_1_hours"];

    float tC = details["air_temperature"] | 0.0f;
    float tK = tC + 273.15f;
    float p0 = details["air_pressure_at_sea_level"] | 0.0f;
    float p  = p0 * expf(-(g * h) / (R * tK));

    WxForecast[h_count].Dt          = iso_to_unix(timeseries[i]["time"]);
    WxForecast[h_count].Temperature = tC;
    WxForecast[h_count].FeelsLike   = tC;
    WxForecast[h_count].Humidity    = details["relative_humidity"] | 0;
    WxForecast[h_count].Pressure    = p;
    WxForecast[h_count].Cloudcover  = details["cloud_area_fraction"] | 0;
    WxForecast[h_count].Windspeed   = details["wind_speed"] | 0.0f;
    WxForecast[h_count].Winddir     = details["wind_from_direction"] | 0.0f;

    float precip = next1h["details"]["precipitation_amount"] | 0.0f;
    const char* symbol = next1h["summary"]["symbol_code"] | "";

    WxForecast[h_count].Precipitation = precip;

    if (strstr(symbol, "snow")) {
      WxForecast[h_count].Snowfall = precip;
      WxForecast[h_count].Rainfall = 0.0f;
    } else if (strstr(symbol, "sleet")) {
      WxForecast[h_count].Snowfall = precip * 0.5f;
      WxForecast[h_count].Rainfall = precip * 0.5f;
    } else {
      WxForecast[h_count].Rainfall = precip;
      WxForecast[h_count].Snowfall = 0.0f;
    }

    WxForecast[h_count].Icon = MetNoSymbolToIcon(symbol);

    h_count++;
  }

  for (int i = h_count; i < max_readings; i++) WxForecast[i].Dt = 0;

  // ==================== DAILY ====================
  if (print) DEBUG(SIMBOL_WAR, "=== DAILY ===");

int d_count = 0;

float day_high = -100;
float day_low  = 100;
float day_rain = 0;
float day_snow = 0;

String prev_day = "";
String day_icon = "04d";

for (size_t i = 0; i < timeseries.size() && d_count < 8; i++) {

  String time_str = timeseries[i]["time"].as<const char*>();
  String day = time_str.substring(0, 10);

  if (day != prev_day && prev_day != "") {
    Daily[d_count].Dt = iso_to_unix((prev_day + "T12:00:00").c_str());
    Daily[d_count].High = day_high;
    Daily[d_count].Low  = day_low;

    Daily[d_count].Rainfall = day_rain;
    Daily[d_count].Snowfall = day_snow;
    Daily[d_count].Precipitation = day_rain + day_snow;

    Daily[d_count].Icon = day_icon;

    if (print) {
      DEBUG("=== DAY " + String(d_count) + " ===");
    
      String dateStr = ConvertUnixTime(Daily[d_count].Dt);
      DEBUG("Date: " + dateStr);
    
      DEBUG("High: " + String(Daily[d_count].High));
      DEBUG("Low : " + String(Daily[d_count].Low));
    
      DEBUG("Rain: " + String(Daily[d_count].Rainfall));
      DEBUG("Snow: " + String(Daily[d_count].Snowfall));
      DEBUG("Total Precip: " + String(Daily[d_count].Precipitation));
    
      DEBUG("Icon: " + Daily[d_count].Icon);
    
      DEBUG("--------------------------");
    }

    d_count++;

    // reset
    day_high = -100;
    day_low  = 100;
    day_rain = 0;
    day_snow = 0;
    day_icon = "04d";
  }

  prev_day = day;

  float t = timeseries[i]["data"]["instant"]["details"]["air_temperature"] | 0.0f;

  if (t > day_high) day_high = t;
  if (t < day_low)  day_low  = t;

  float precip = 0;
  String symbol = "";

  if (timeseries[i]["data"].containsKey("next_6_hours")) {
    precip = timeseries[i]["data"]["next_6_hours"]["details"]["precipitation_amount"] | 0.0f;
    symbol = timeseries[i]["data"]["next_6_hours"]["summary"]["symbol_code"].as<const char*>();
  } 
  else if (timeseries[i]["data"].containsKey("next_1_hours")) {
    precip = timeseries[i]["data"]["next_1_hours"]["details"]["precipitation_amount"] | 0.0f;
    symbol = timeseries[i]["data"]["next_1_hours"]["summary"]["symbol_code"].as<const char*>();
  }

  // разделение rain/snow
  if (symbol.indexOf("snow") != -1) {
    day_snow += precip;
  } else {
    day_rain += precip;
  }

  // иконка дня (берём около полудня)
  if (time_str.indexOf("12:00") != -1 && symbol.length() > 0) {
    day_icon = MetNoSymbolToIcon(symbol);
  }
}

// последний день
if (prev_day != "" && d_count < 8) {
  Daily[d_count].Dt = iso_to_unix((prev_day + "T12:00:00").c_str());
  Daily[d_count].High = day_high;
  Daily[d_count].Low  = day_low;
  Daily[d_count].Rainfall = day_rain;
  Daily[d_count].Snowfall = day_snow;
  Daily[d_count].Precipitation = day_rain + day_snow;
  Daily[d_count].Icon = day_icon;

  if (print) {
    DEBUG("=== DAY " + String(d_count) + " ===");

    String dateStr = ConvertUnixTime(Daily[d_count].Dt);
    DEBUG("Date: " + dateStr);

    DEBUG("High: " + String(Daily[d_count].High));
    DEBUG("Low : " + String(Daily[d_count].Low));

    DEBUG("Rain: " + String(Daily[d_count].Rainfall));
    DEBUG("Snow: " + String(Daily[d_count].Snowfall));
    DEBUG("Total Precip: " + String(Daily[d_count].Precipitation));

    DEBUG("Icon: " + Daily[d_count].Icon);

    DEBUG("--------------------------");
 }

  d_count++;
}

for (int i = d_count; i < 8; i++) Daily[i].Dt = 0;

  if (print) DEBUG(SIMBOL_WAR, "[MET.NO][Decode] Done");

  return true;
}

String MetNoSymbolToIcon(const String& symbol_code) {

  bool isNight = symbol_code.indexOf("_night") != -1;

  String base = symbol_code;
  base.replace("_day", "");
  base.replace("_night", "");
  base.replace("_polartwilight", "");

  String icon = "04d"; // default

  // порядок важен!
  if (base.indexOf("thunder") != -1) {
    icon = "11d";
  }
  else if (base.indexOf("snow") != -1) {
    icon = "13d";
  }
  else if (base.indexOf("sleet") != -1) {
    icon = "13d"; // можно поменять на 10d если хочешь "более дождь"
  }
  else if (base.indexOf("showers") != -1) {
    icon = "09d";
  }
  else if (base.indexOf("rain") != -1) {
    icon = "10d";
  }
  else if (base.indexOf("fog") != -1) {
    icon = "50d";
  }
  else if (base.indexOf("clearsky") != -1) {
    icon = "01d";
  }
  else if (base.indexOf("fair") != -1) {
    icon = "02d";
  }
  else if (base.indexOf("partlycloudy") != -1) {
    icon = "03d";
  }
  else if (base.indexOf("cloudy") != -1) {
    icon = "04d";
  }

  // заменяем d → n
  if (isNight) {
    icon.setCharAt(icon.length() - 1, 'n');
  }

  return icon;
}



bool DecodeMetNoWeatherFromStream3(Stream& stream, bool print) {
  if (print) DEBUG(SIMBOL_WAR, "===== Decoding Data from MET.NO =====");

  JsonDocument doc;   // ArduinoJson 6/7
  //DeserializationError error = deserializeJson(doc, payload);
  DeserializationError error = deserializeJson(doc, stream);
  if (error) {
    DEBUG(SIMBOL_ERR, "deserializeJson filed: ", error.c_str());
    DEBUG(SIMBOL_WAR, "FreeHeap: ", ESP.getFreeHeap(), " bytes");
    return false;
  }
  if(print) DEBUG(SIMBOL_WAR, "[MET.NO][Decode] FreeHeap: ", ESP.getFreeHeap(), " bytes");

  JsonObject properties = doc["properties"];
  JsonArray timeseries  = properties["timeseries"];

  if (timeseries.size() == 0) {
    DEBUG(SIMBOL_ERR, "[MET.NO] No timeseries data");
    return false;
  }

  if (print) DEBUG(SIMBOL_WAR, "=== CURRENT CONDITIONS ===");

  JsonObject now = timeseries[0]["data"]["instant"]["details"];

  WxConditions[0].Dt          = iso_to_unix(timeseries[0]["time"].as<const char*>()) + settings.timeZone*3600;
  WxConditions[0].Timezone    = 0;                    // met.no отдаёт в UTC
  WxConditions[0].lat         = doc["geometry"]["coordinates"][1] | settings.latitude;
  WxConditions[0].lon         = doc["geometry"]["coordinates"][0] | settings.longitude;

  WxConditions[0].Temperature = now["air_temperature"] | 0.0f;
 // WxConditions[0].FeelsLike   = now["air_temperature"] | 0.0f;   
  WxConditions[0].DewPoint    = now["dew_point_temperature"] | 0.0f;
  WxConditions[0].Humidity    = now["relative_humidity"] | 0;

  float p0 = now["air_pressure_at_sea_level"] | 0.0f;
  float tC = now["air_temperature"] | 0.0f;
  float h =  doc["geometry"]["coordinates"][2] | 0.0f;
  float tK = tC + 273.15f;
  const float g = 9.81f;
  const float R = 287.0f;
  float p = p0 * expf(-(g * h) / (R * tK)); 
  WxConditions[0].Pressure = p;

  WxConditions[0].Cloudcover  = now["cloud_area_fraction"] | 0;
  WxConditions[0].Windspeed   = now["wind_speed"] | 0.0f;
  WxConditions[0].Winddir     = now["wind_from_direction"] | 0.0f;
  WxConditions[0].UVI         = 0.0f;   // нет в compact

  // Осадки сейчас (из next_1_hours)
  JsonObject next1h = timeseries[0]["data"]["next_1_hours"];
  if (!next1h.isNull()) {
    const char* symbol = next1h["summary"]["symbol_code"] | "";
    WxConditions[0].Icon = MetNoSymbolToIcon(symbol ? symbol : "");

    float precip = next1h["details"]["precipitation_amount"] | 0.0f;
    WxConditions[0].Precipitation = precip;

    if (strstr(symbol, "snow")) {
        WxConditions[0].Snowfall = precip;
        WxConditions[0].Rainfall = 0.0f;
    }
    else if (strstr(symbol, "sleet")) {
        // мокрый снег  типа половина того половина другого???
        WxConditions[0].Snowfall = precip * 0.5f;
        WxConditions[0].Rainfall = precip * 0.5f;
    }
    else {
        WxConditions[0].Rainfall = precip;
        WxConditions[0].Snowfall = 0.0f;
    }


    
  } else {
    WxConditions[0].Rainfall = 0.0f;
    WxConditions[0].Icon = "04d";
  }

 // WxConditions[0].IsDay = true;   

  if (print) {
    DEBUG("Temp: " + String(WxConditions[0].Temperature));
    DEBUG("Icon: " + WxConditions[0].Icon);
    DEBUG("Rain (1h): " + String(WxConditions[0].Rainfall));
  }

  // ==================== HOURLY ====================
  if (print) DEBUG(SIMBOL_WAR, "=== HOURLY ===");

  int h_count = 0;
  for (size_t i = 0; i < timeseries.size() && h_count < max_readings; i++) {
    const char* time_str = timeseries[i]["time"];
    // Берём только шаги с next_1_hours (почасовые)
    if (timeseries[i]["data"].containsKey("next_1_hours")) {
      JsonObject details = timeseries[i]["data"]["instant"]["details"];
      JsonObject next1h_details = timeseries[i]["data"]["next_1_hours"]["details"];

      WxForecast[h_count].Dt          = iso_to_unix(time_str);
      WxForecast[h_count].Temperature = details["air_temperature"] | 0.0f;
      WxForecast[h_count].FeelsLike   = details["air_temperature"] | 0.0f;
      WxForecast[h_count].Humidity    = details["relative_humidity"] | 0;
      WxForecast[h_count].Pressure    = details["air_pressure_at_sea_level"] | 0.0f;
      WxForecast[h_count].Cloudcover  = details["cloud_area_fraction"] | 0;
      WxForecast[h_count].Windspeed   = details["wind_speed"] | 0.0f;
      WxForecast[h_count].Winddir     = details["wind_from_direction"] | 0.0f;
      WxForecast[h_count].Rainfall    = next1h_details["precipitation_amount"] | 0.0f;
      WxForecast[h_count].Precipitation = WxForecast[h_count].Rainfall;

      const char* symbol = timeseries[i]["data"]["next_1_hours"]["summary"]["symbol_code"];
      WxForecast[h_count].Icon = MetNoSymbolToIcon(symbol ? symbol : "");

      h_count++;
    }
  }

  for (int r = h_count; r < max_readings; r++) WxForecast[r].Dt = 0;

  // ==================== DAILY (max/min + осадки) ====================
  if (print) DEBUG(SIMBOL_WAR, "=== DAILY (7 дней) ===");

  // Для daily нам нужно агрегировать по дням (мет.no не отдаёт готовые daily)
  // Простой способ: группируем по календарным дням и ищем max/min + суммируем осадки

  int d_count = 0;
  float day_high = -100, day_low = 100;
  float day_precip = 0.0f;
  String current_day = "";
  String prev_day = "";

  for (size_t i = 0; i < timeseries.size() && d_count < 8; i++) {
    const char* time_str = timeseries[i]["time"].as<const char*>();
    String this_day = String(time_str).substring(0, 10);  // "2026-04-27"

    if (this_day != prev_day && prev_day != "") {
      // Сохраняем предыдущий день
      Daily[d_count].Dt          = iso_to_unix((prev_day + "T12:00:00").c_str()); // середина дня
      Daily[d_count].High        = day_high;
      Daily[d_count].Low         = day_low;
      Daily[d_count].Precipitation = day_precip;
      Daily[d_count].Rainfall    = day_precip;   // мет.no даёт общее precipitation
      Daily[d_count].Snowfall    = 0.0f;

      // Берём символ из середины дня (примерно 6-12 часов)
      // Можно улучшить позже
      Daily[d_count].Icon = "04d"; // заглушка, можно взять из первого next_6_hours дня

      if (print) {
        DEBUG("Day " + String(d_count) + ": " + prev_day);
        DEBUG("  High: " + String(day_high) + "  Low: " + String(day_low) + "  Precip: " + String(day_precip));
      }

      d_count++;
      day_high = -100; day_low = 100; day_precip = 0.0f;
    }

    prev_day = this_day;

    // Обновляем max/min и сумму осадков за день
    float t = timeseries[i]["data"]["instant"]["details"]["air_temperature"] | 0.0f;
    if (t > day_high) day_high = t;
    if (t < day_low)  day_low  = t;

    // Суммируем осадки (next_6_hours или next_1_hours)
    if (timeseries[i]["data"].containsKey("next_6_hours")) {
      day_precip += timeseries[i]["data"]["next_6_hours"]["details"]["precipitation_amount"] | 0.0f;
    } else if (timeseries[i]["data"].containsKey("next_1_hours")) {
      day_precip += timeseries[i]["data"]["next_1_hours"]["details"]["precipitation_amount"] | 0.0f;
    }
  }

  // Не забудь последний день
  if (prev_day != "" && d_count < 8) {
    Daily[d_count].High = day_high;
    Daily[d_count].Low  = day_low;
    Daily[d_count].Precipitation = day_precip;
    Daily[d_count].Rainfall = day_precip;
    Daily[d_count].Icon = "04d";
    d_count++;
  }

  for (int r = d_count; r < 8; r++) Daily[r].Dt = 0;

  if(print) DEBUG(SIMBOL_WAR, "[MET.NO][Decode] FreeHeap: ", ESP.getFreeHeap(), " bytes");
  return true;
}
//symbol_code met.no → Icon  
String MetNoSymbolToIcon3(const String& symbol_code) {
  if (symbol_code.indexOf("clearsky") != -1)      return "01d";
  if (symbol_code.indexOf("fair") != -1)          return "02d";
  if (symbol_code.indexOf("partlycloudy") != -1)  return "03d";
  if (symbol_code.indexOf("cloudy") != -1)        return "04d";

  if (symbol_code.indexOf("rain") != -1)          return "10d";
  if (symbol_code.indexOf("showers") != -1)       return "09d";
  if (symbol_code.indexOf("snow") != -1)          return "13d";
  if (symbol_code.indexOf("sleet") != -1)         return "09d";   // мокрый снег
  if (symbol_code.indexOf("thunder") != -1)       return "11d";

  if (symbol_code.indexOf("fog") != -1)           return "50d";

  // ночные варианты
  if (symbol_code.indexOf("_night") != -1) {
    String dayIcon = MetNoSymbolToIcon(symbol_code.substring(0, symbol_code.indexOf("_night")));
    dayIcon.setCharAt(dayIcon.length()-1, 'n');
    return dayIcon;
  }

  return "04d"; // по умолчанию облачно
}



// #########################################################################################
String IconToEmoji(String icon) {
  if (icon == "01d") return "☀️";   // ясно день
  if (icon == "01n") return "🌙";   // ясно ночь

  if (icon == "02d") return "🌤️";  // малооблачно день
  if (icon == "02n") return "🌤️";  // малооблачно ночь

  if (icon == "03d" || icon == "03n") return "⛅"; // переменная облачность

  if (icon == "04d" || icon == "04n") return "☁️"; // облачно

  if (icon == "09d" || icon == "09n") return "🌧️"; // дождь

  if (icon == "10d") return "🌦️"; // дождь с солнцем
  if (icon == "10n") return "🌧️"; // дождь ночью

  if (icon == "11d" || icon == "11n") return "⛈️"; // гроза

  if (icon == "13d" || icon == "13n") return "❄️"; // снег

  if (icon == "50d" || icon == "50n") return "🌫️"; // туман

  return "❓"; // неизвестно
}

int iso_to_unix(const char* iso) {
  if (!iso || strlen(iso) < 10) return 0;
  struct tm tm = {0};
  int year, mon, mday, hour=0, min=0, sec=0;
  sscanf(iso, "%d-%d-%dT%d:%d:%d", &year, &mon, &mday, &hour, &min, &sec);
  tm.tm_year = year - 1900;
  tm.tm_mon  = mon - 1;
  tm.tm_mday = mday;
  tm.tm_hour = hour;
  tm.tm_min  = min;
  tm.tm_sec  = sec;
  tm.tm_isdst = -1;
  return (int)mktime(&tm);
}


// #########################################################################################
String ConvertUnixTime(int unix_time) {
  time_t tm = unix_time;
  struct tm *now_tm = gmtime(&tm);
  char output[40];
  strftime(output, sizeof(output), "%H:%M %d/%m/%y", now_tm);
  
  
  return output;
}

// 13:22:33
String ConvertTime_HMS(time_t unix_time) {
    struct tm timeinfo;
    gmtime_r(&unix_time, &timeinfo);

    char buffer[10];
    strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);

    return String(buffer);
}

// 21 АПРЕЛЯ 2026
String ConvertDate_Full(time_t unix_time) {
    struct tm timeinfo;
    gmtime_r(&unix_time, &timeinfo);

    const char* months[] = {
        "ЯНВАРЯ", "ФЕВРАЛЯ", "МАРТА", "АПРЕЛЯ",
        "МАЯ", "ИЮНЯ", "ИЮЛЯ", "АВГУСТА",
        "СЕНТЯБРЯ", "ОКТЯБРЯ", "НОЯБРЯ", "ДЕКАБРЯ"
    };

    char buffer[40];
    snprintf(buffer, sizeof(buffer), "%d %s %d",
             timeinfo.tm_mday,
             months[timeinfo.tm_mon],
             timeinfo.tm_year + 1900);

    return String(buffer);
}

// ВТОРНИК
String ConvertWeekDay(time_t unix_time) {
    struct tm timeinfo;
    gmtime_r(&unix_time, &timeinfo);

    const char* days[] = {
        "ВОСКРЕСЕНЬЕ", "ПОНЕДЕЛЬНИК", "ВТОРНИК",
        "СРЕДА", "ЧЕТВЕРГ", "ПЯТНИЦА", "СУББОТА"
    };

    return String(days[timeinfo.tm_wday]);
}

float mm_to_inches(float value_mm){
  return 0.0393701 * value_mm;
}

float hPa_to_inHg(float value_hPa){
  return 0.02953 * value_hPa;
}

int JulianDate(int d, int m, int y) {
  int mm, yy, k1, k2, k3, j;
  yy = y - (int)((12 - m) / 10);
  mm = m + 9;
  if (mm >= 12) mm = mm - 12;
  k1 = (int)(365.25 * (yy + 4712));
  k2 = (int)(30.6001 * mm + 0.5);
  k3 = (int)((int)((yy / 100) + 49) * 0.75) - 38;
  j = k1 + k2 + d + 59 + 1;
  if (j > 2299160) j = j - k3;
  return j;
}

float SumOfPrecip(float DataArray[], int readings) {
  float sum = 0;
  for (int i = 0; i < readings; i++) {
    sum += DataArray[i];
  }
  return sum;
}

String TitleCase(String text){
  if (text.length() > 0) {
    String temp_text = text.substring(0,1);
    temp_text.toUpperCase();
    return temp_text + text.substring(1);
  }
  else return text;
}

double NormalizedMoonPhase(int d, int m, int y) {
  int j = JulianDate(d, m, y);
  double Phase = (j + 4.867) / 29.53059;
  return (Phase - (int) Phase);
}