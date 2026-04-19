#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <vector>
#include <functional>

class HA_MQTT {
   public:
    struct Sensor {
        String name;
        String key;
        String unit;
        String device_class;
        int precision;
    };
    
   private:
    WiFiClient* wifi;
    PubSubClient mqtt;

    String server;
    int port;

    String client_name;
    String _model = "GLUONiCA";
    String _manufacturer = "Vanila";
    String user;
    String pass;

    String topic_pub;
    String topic_sub;

    String device_id;

    uint16_t _socket_timeout = 5;
    uint16_t _keepalive = 25;
    unsigned long  _wifi_timeout = 2500;
    unsigned long reconnectInterval = 8000;


    std::vector<Sensor> sensors;

    std::vector<String> discoveredTopics;
    const uint8_t maxDevices = 10;

    std::function<void(String, String)> userCallback;

    uint8_t _max_attempts = 10;
    uint8_t _reconnect_attempts = 0;

    unsigned long lastReconnect = 0;


    bool connected = false;
    bool discoverySent = false;

    uint16_t _buf_size = 1024;

   public:
    HA_MQTT(WiFiClient& client) : mqtt(client) {
        wifi = &client;
        //wifi->setTimeout(_wifi_timeout);
        sensors.reserve(10);
        
    }

    bool isConnected(){
        return mqtt.connected();
    }

    bool connect(){
        return reconnect(true);
    }

    void end(){
        mqtt.disconnect();
    }

    void setServer(const char* host, int p) {
        server = host;
        port = p;
        mqtt.setServer(host, p);
    }

    void setAuth(const char* u, const char* p) {
        user = u;
        pass = p;
    }

    void setClient(const char* name) {
        client_name = name;
        #ifdef ESP8266
        device_id = client_name + "_" + String(ESP.getChipId(), HEX);
        #else
        device_id = client_name + "_" + String(ESP.getEfuseMac(), HEX);
        #endif
        
    }

    void setModel(const char* model) {
        _model = model;
    }

    void setManufacturer(const char* manufacturer) {
        _manufacturer = manufacturer;
    }

    void setTopics(const char* pub, const char* sub) {
        topic_pub = pub;
        topic_sub = sub;
    }

    void onMessage(std::function<void(String, String)> cb) {
        userCallback = cb;
    }

    void addSensor(const char* name,
                   const char* key,
                   const char* unit = "",
                   const char* device_class = "",
                   int precision = 0) {
        sensors.push_back({name, key, unit, device_class, precision});
    }

    String availabilityTopic() {
        String topic = topic_pub; 
        topic.trim();

        if (topic.endsWith("/")) {
            topic.remove(topic.length() - 1);
        }

        return topic + "/status";
    }
    // удалит последнее в топике до / 
    // было топик/устройства/айди/ 
    // вернет топик/устройства
    String getBaseTopic(String topic) {
        String _topic = topic;
        _topic.trim();

        // Убираем все завершающие слеши
        while (_topic.endsWith("/")) {
            _topic.remove(_topic.length() - 1);
        }

        // Находим последний слеш и оставляем только то, что до него
        int lastSlash = _topic.lastIndexOf('/');
        if (lastSlash != -1) {
            _topic = _topic.substring(0, lastSlash);
        }

        return _topic;
    }

    void setBufferSize(uint16_t size = 1024){
        _buf_size = size;
        mqtt.setBufferSize(_buf_size);
    }
    bool begin() {
        mqtt.setCallback([this](char* topic, byte* payload, unsigned int length) {
            this->callback(topic, payload, length);
        });

        mqtt.setBufferSize(_buf_size);
        mqtt.setSocketTimeout(_socket_timeout);
        mqtt.setKeepAlive(_keepalive);
        return reconnect(true);
    }
    // секунд
    void setSocketTimeout(uint16_t timeout) {
        _socket_timeout = timeout;
        mqtt.setSocketTimeout(_socket_timeout);
    }
    //секунд
    void setKeepAlive(uint16_t keepAlive) {
        _keepalive = keepAlive;
        mqtt.setKeepAlive(_keepalive);
    }
    // миллисекунд
    //void setWiFiTimeout(uint16_t timeout) {
    //    _wifi_timeout = timeout;
    //    wifi->setTimeout(_wifi_timeout);
    //}
    // миллисекунд
    void setReconnectInterval(unsigned long interval){
        reconnectInterval = interval;
    }

    void loop() {
        if (!mqtt.connected()) {
            if (millis() - lastReconnect > reconnectInterval) {
                lastReconnect = millis();
                reconnect(false);
            }
        }
        mqtt.loop();
    }

    void setMaxAttempts(uint8_t max_attempts) {
        _max_attempts = max_attempts;
    }

    uint8_t getMaxAttempts() const {
        return _max_attempts;
    }

    uint8_t getReconnectAttempts() {
        return _reconnect_attempts;
    }

    void resetReconnectAttempts() {
        _reconnect_attempts = 0;
    }



    bool reconnect(bool force) {
        if (!WiFi.isConnected()) return false;
      
        mqtt.setSocketTimeout(_socket_timeout);
        mqtt.setKeepAlive(_keepalive);

        if (force) _reconnect_attempts = 0;

        if (_max_attempts > 0 && _reconnect_attempts >= _max_attempts) {
            connected = false;
            return false;
        }

        _reconnect_attempts++;

        if (!mqtt.connected()) {
            bool ok = mqtt.connect(
                client_name.c_str(),
                user.c_str(),
                pass.c_str(),
                availabilityTopic().c_str(),
                0,
                true,
                "offline"
            );

            if (ok) {
                resubscribe();

                publishBirth();

                if (!discoverySent) {
                    sendDiscovery();
                    discoverySent = true;
                }

                connected = true;
                _reconnect_attempts = 0;
                return true;
            }
        }
        else {
            if (force){
                bool ok = mqtt.connect(
                    client_name.c_str(),
                    user.c_str(),
                    pass.c_str(),
                    availabilityTopic().c_str(),
                    0,
                    true,
                    "offline"
                );
            
                if (ok) {
                    resubscribe();
                
                    publishBirth();
                
                    if (!discoverySent) {
                        sendDiscovery();
                        discoverySent = true;
                    }
                
                    connected = true;
                    _reconnect_attempts = 0;
                    
                }
            }
            return true;
        }

        connected = false;
        return false;
    }

    void resubscribe() {
        mqtt.subscribe(topic_sub.c_str());
        mqtt.subscribe("homeassistant/status");

        String status_topic = getBaseTopic(topic_pub) + "/+/status";
        mqtt.subscribe(status_topic.c_str());
    }

    void publishBirth() {
       // JsonDocument doc;
       // doc["status"] = "online";
       // doc["ip"] = WiFi.localIP().toString();
       // doc["rssi"] = WiFi.RSSI();
       // doc["fw"] = VERSION;

       // String out;
       // serializeJson(doc, out);

        //mqtt.publish(availabilityTopic().c_str(), out.c_str(), true);
        mqtt.publish(availabilityTopic().c_str(), "online", true);
    }

    void callback(char* topic, byte* payload, unsigned int length) {
        String t = topic;

        String msg;
        for (uint16_t i = 0; i < length; i++)
            msg += (char)payload[i];

        // HA restart
        if (t == "homeassistant/status") {
            if (msg == "online") {
                sendDiscovery();
            }
            return;
        }

        // ===== DEVICE DISCOVERY =====
        if(t.endsWith("/status")){
             String tp = topic;
             String msg;
             for (uint16_t i = 0; i < length; i++)
                 msg += (char)payload[i];
                 
             if (msg == "online" || msg == "offline") {
             
                 // убираем /status
                 String base = tp.substring(0, tp.lastIndexOf("status"));
             
                 bool exists = false;
             
                 for (auto &dev : discoveredTopics) {
                     if (dev == base) {
                         exists = true;
                         break;
                     }
                 }
             
                 if (!exists && discoveredTopics.size() < maxDevices) {
                     discoveredTopics.push_back(base);
                 }
             }
         
             return;
        }

        // JSON commands
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, msg);

        if (!err && doc.containsKey("cmd")) {
            String cmd = doc["cmd"];

            if (cmd == "reboot") {
                ESP.restart();
            }

            if (cmd == "ota") {
                String url = doc["url"];
                handleOTA(url);
            }
        }

        if (userCallback) userCallback(t, msg);
    }
    String getTopicsString() {
        String result;

        for (uint8_t i = 0; i < discoveredTopics.size(); i++) {
            result += discoveredTopics[i];
            if (i < discoveredTopics.size() - 1) {
                result += ";";
            }
        }

        return result;
    }
    String getDevicesString() {
        String result;

        for (uint8_t i = 0; i < discoveredTopics.size(); i++) {
            String topic = discoveredTopics[i];
            topic.trim();

            while (topic.endsWith("/")) {
                topic.remove(topic.length() - 1);
            }

            int lastSlash = topic.lastIndexOf('/');
            String deviceId = (lastSlash != -1) ? topic.substring(lastSlash + 1) : topic;

            result += deviceId;

            if (i < discoveredTopics.size() - 1) {
                result += ";";
            }
        }

        return result;
    }
    String getTopic(uint8_t index) {
        if (index >= discoveredTopics.size()) return "";
        return discoveredTopics[index];
    }
    uint8_t getDevicesCount() {
         return discoveredTopics.size();
    }
    void clearDevices() {
        discoveredTopics.clear();
    }

    void handleOTA(const String& url) {
        Serial.println("OTA requested: " + url);
        // тут можно подключить HTTPUpdate
        // ESPhttpUpdate.update(url);
    }

    void sendDiscovery() {
        for (auto& s : sensors) {
            JsonDocument doc;

            doc["name"] = s.name;
            doc["unique_id"] = device_id + "_" + s.key;
            doc["state_topic"] = topic_pub;
            doc["value_template"] = "{{ value_json." + s.key + " }}";
            doc["availability_topic"] = availabilityTopic();

            if (s.unit.length()) doc["unit_of_measurement"] = s.unit;
            if (s.device_class.length()) doc["device_class"] = s.device_class;
            if (s.precision) doc["suggested_display_precision"] = s.precision;

            JsonObject dev = doc["device"].to<JsonObject>();
            dev["identifiers"][0] = device_id;
            dev["name"] = client_name;
            dev["model"] = _model;
            dev["manufacturer"] = _manufacturer;

            String payload;
            serializeJson(doc, payload);

            String topic = "homeassistant/sensor/" + device_id + "/" + s.key + "/config";

            if (!mqtt.publish(topic.c_str(), payload.c_str(), true)) {
                Serial.println("Discovery publish failed");
            }

            yield();
        }
    }

    bool sendState(std::function<void(JsonDocument&)> fill, bool retain = true) {
        if (!mqtt.connected()) return false;

        JsonDocument doc;
        fill(doc);

        String out;
        serializeJson(doc, out);

        if (!mqtt.publish(topic_pub.c_str(), out.c_str(), retain)) {
            Serial.println("State publish failed");
            return false;
        }
        return true;

    }

    
    bool sendAndSleep(std::function<void(JsonDocument&)> fill, bool retain = true) {
        if (!reconnect(true)) return false;

        sendState(fill, retain);

        mqtt.disconnect();
        delay(100);

        return true;
    }
};