#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>    
   

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
bool ReceiveOneCallWeather(WiFiClient& client, bool print);


String ConvertUnixTime(int unix_time);
float mm_to_inches(float value_mm);
float hPa_to_inHg(float value_hPa);
int JulianDate(int d, int m, int y);
float SumOfPrecip(float DataArray[], int readings);
String TitleCase(String text);
double NormalizedMoonPhase(int d, int m, int y);
bool DecodeOneCallWeatherFromString(const String& payload, bool print);

// Вспомогательные для Open-Meteo
String WMO_to_Icon(int code, bool is_day = true);
int iso_to_unix(const char* iso);

// #########################################################################################
bool ReceiveOneCallWeather(WiFiClient& client, bool print) {
  

  client.stop(); // close connection before sending a new request

  HTTPClient http;

  String uri_hourly = "/v1/forecast?"
               "latitude=" + String(settings.latitude) +
               "&longitude=" + String(settings.longitude) +
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

    // Для отладки — выводим размер и начало ответа
    DEBUG(SIMBOL_INFO, "[WEATHER] Payload received, size: ", payload.length(), " bytes");


    // Показываем первые 400 символов (чтобы увидеть, JSON ли это)
   //if (payload.length() > 400) {
   //  DEBUG("First 400 chars:");
   //  DEBUG(payload.substring(0, 400));
   //} else {
   //  DEBUG("Full payload:");
   //  DEBUG(payload);
   //}


    bool ok = DecodeOneCallWeatherFromString(payload, print);

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
// #########################################################################################

bool DecodeOneCallWeatherFromString(const String& payload, bool print) {
  if (print) DEBUG(SIMBOL_WAR, "===== Decoding Data from Open-Meteo ======");

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



  return true;
}

bool DecodeOneCallWeatherFromStringHourly(const String& payload, bool print) {
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



// #########################################################################################
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