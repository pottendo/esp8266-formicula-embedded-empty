#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <MQTT.h>
#include "defs.h"
// myqtthub credentials
#ifdef EXTMQTT
static const char *mqtt_server = "node02.myqtthub.com";
static int mqtt_port = 8883;
static const char *clientID = "fcce";
static const char *user = "fcc-users";
static const char *pw = "foRmicula666";
WiFiClientSecure net;
#else
static const char *mqtt_server = "pottendo-pi30-phono";
static int mqtt_port = 1883;
static const char *clientID = "fcce";
//static const char *user = "fcc-users";
//static const char *pw = "foRmicula666";
WiFiClient net;
#endif

static MQTTClient client;
static unsigned long fcc_last_seen;

void callback(String &topic, String &payload)
{
    log_msg("fcce - " + topic + ": " + payload);
    if (topic.startsWith("fcc/cc-alive"))
    {
        log_msg("fcc is alive (" + String((millis() - fcc_last_seen) / 1000) + "s), re-arming watchdog.");
        fcc_last_seen = millis();
        return;
    }
    if (topic.startsWith("fcc/reset-request"))
    {
        log_msg("fcc requested reset... rebooting");
        ESP.restart();
    }
}

void reconnect()
{
    static int reconnects = 0;
    static unsigned long last = 0;
    static unsigned long connection_wd = 0;

    // Loop until we're reconnected
    if ((!client.connected()) &&
        ((millis() - last) > 2500))
    {
        if (connection_wd == 0)
            connection_wd = millis();
        last = millis();
        reconnects++;
        //static char buf[32];
        //snprintf(buf, 32, "fcce", reconnects);
        log_msg("fcce not connected, attempting MQTT connection..." + String(reconnects));

        // Attempt to connect
#ifdef EXTMQTT
        net.setInsecure();
        if (client.connect(clientID, user, pw))
#else
        if (client.connect(clientID))
#endif
        {
            log_msg("connected");
            client.subscribe("#", 0);
            client.publish("fcce/config", "Formicula Control Center - aloha...");
            reconnects = 0;
            connection_wd = 0;
            log_msg("fcce connected.");
        }
        else
        {
            unsigned long t1 = (millis() - connection_wd) / 1000;
            log_msg(String("Connection lost for: ") + String(t1) + "s...");
            if (t1 > 300)
            {
                log_msg("mqtt reconnections failed for 5min... rebooting");
                ESP.restart();
            }
            log_msg("mqtt connect failed, rc=" + String(client.lastError()) + "... retrying.");
        }
    }
}

bool mqtt_reset(void)
{
    log_msg("mqtt reset requested: " + String(client.lastError()));
    return true;
}

void mqtt_publish(String topic, String msg)
{
    if (!client.connected() &&
        (mqtt_reset() == false))
    {
        log_msg("mqtt client not connected...");
        return;
    }
    //log_msg("fcc publish: " + topic + " - " + msg);
    client.publish(topic.c_str(), msg.c_str());
}

void setup_mqtt(void)
{
    client.begin(mqtt_server, mqtt_port, net);
    client.onMessage(callback);
    delay(50);
}

void loop_mqtt_dummy()
{
    static long value = 0;
    static char msg[50];
    static unsigned long lastMsg = millis();

    unsigned long now = millis();
    if (now - lastMsg > 2000)
    {
        if (!client.connected())
        {
            mqtt_reset();
        }
        else
        {
            ++value;
            snprintf(msg, 50, "Hello world from fcce #%ld", value);
            mqtt_publish("fcce/config", msg);
            const char *c;
            if (value % 2)
                c = "1";
            else
                c = "0";

            mqtt_publish("fcce/Infrarot", c);
        }
        lastMsg = now;
    }
}

void loop_mqtt()
{
    if (!client.connected())
    {
        reconnect();
    }
    client.loop();
    //loop_mqtt_dummy();

    if ((millis() - fcc_last_seen) > 240 * 1000)
    {
        log_msg("fcc not seen for 4min, rebooting.");
        ESP.restart();
    }
}

#ifdef PUBSUB
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include "defs.h"
//static const String mqtt_server{"pottendo-pi30-phono"};
//static const String mqtt_server{"fcce"};
static IPAddress server;
//#define server_str WiFi.localIP()
// myqtthub credentials
static const char *server_str = "node02.myqtthub.com";
static const char *clientID = "fcce";
static const char *user = "fcc-users";
static const char *pw = "foRmicula666";
//static IPAddress server_str{127, 0, 0, 1};

extern "C"
{
    int myprintf(const char *s, ...)
    {
        int res;
        va_list arg;
        va_start(arg, s);
        res = vprintf(s, arg);
        va_end(arg);
        return res;
    }
    int os_printf2(const char *s, ...)
    {
        int res;
        va_list arg;
        va_start(arg, s);
        res = vprintf(s, arg);
        va_end(arg);
        return res;
    }

    int mysnprintf(char *s, size_t n, const char *fmt, ...)
    {
        int res;
        va_list arg;
        va_start(arg, fmt);
        res = vsnprintf(s, n, fmt, arg);
        va_end(arg);
        return res;
    }
}

void callback(char *t, byte *payload, unsigned int len)
{
    if (len == 0)
    {
        log_msg("MQTT invalid message received... ignoring");
        return;
    }
    char *buf = (char *)malloc(len * sizeof(char) + 1);
    snprintf(buf, len + 1, "%s", payload);
    //log_msg(String("Sensor ") + t + " delivered: " + buf + "(" + String(len) + ")");
    const String topic{strchr(t + 1, '/')};

    // check if config things arriving
    if (topic.startsWith("/config"))
    {
        //        ui->update_config(String(buf));
    }

    free(buf);
}

WiFiClient wClient;
PubSubClient *client;

void reconnect()
{
    static int reconnects = 0;
    static unsigned long last = 0;
    static unsigned long connection_wd = 0;

    // Loop until we're reconnected
    if ((!client->connected()) &&
        ((millis() - last) > 2500))
    {
        if (connection_wd == 0)
            connection_wd = millis();
        last = millis();
        reconnects++;
        //static char buf[32];
        //snprintf(buf, 32, "fcce", reconnects);
        log_msg("fcce not connected, attempting MQTT connection..." + String(reconnects));

        // Attempt to connect
        if (client->connect(clientID, user, pw))
        {
            log_msg("connected");
            client->subscribe("#", 0);
            client->publish("fcce/config", "Formicula Control Center - aloha...");
            reconnects = 0;
            connection_wd = 0;
            log_msg("fcce connected.");
        }
        else
        {
            unsigned long t1 = (millis() - connection_wd) / 1000;
            log_msg(String("Connection lost for: ") + String(t1) + "s...");
            if (t1 > 300)
            {
                log_msg("mqtt reconnections failed for 5min... rebooting");
                ESP.restart();
            }
            log_msg("mqtt connect failed, rc=" + String(client->state()) + "... retrying.");
        }
    }
}

bool mqtt_reset(void)
{
    log_msg("mqtt reset requested: " + String(client->state()));
    return true;
}

void mqtt_publish(String topic, String msg)
{
    if ((client == nullptr) ||
        (!client->connected() &&
         (mqtt_reset() == false)))
    {
        log_msg("mqtt client not connected...");
        return;
    }
    //log_msg("fcc publish: " + topic + " - " + msg);
    client->publish(topic.c_str(), msg.c_str());
}

void setup_mqtt(void)
{
    client = new PubSubClient{wClient};
    //server = MDNS.queryHost(mqtt_server.c_str());
    delay(25);
    //client->setServer(server, 1883);
    client->setServer(server_str, 1883);
    client->setCallback(callback);

    // Allow the hardware to sort itself out
    delay(500);
}

void loop_mqtt_dummy()
{
    static long value = 0;
    static char msg[50];
    static unsigned long lastMsg = millis();

    unsigned long now = millis();
    if (now - lastMsg > 2000)
    {
        if (!client->connected())
        {
            mqtt_reset();
        }
        else
        {
            ++value;
            snprintf(msg, 50, "Hello world from fcce #%ld", value);
            mqtt_publish("fcce/config", msg);
            const char *c;
            if (value % 2)
                c = "1";
            else
                c = "0";

            mqtt_publish("fcce/Infrarot", c);
        }
        lastMsg = now;
    }
}

void loop_mqtt()
{
    if (!client->connected())
    {
        reconnect();
    }
    client->loop();
    //loop_mqtt_dummy();
}
#endif /* PUBSUB */

#ifdef UMQTT
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <MQTT.h>
#include "defs.h"

static const String my_clientID{"fcce"};
static unsigned long fcc_last_seen;
//#define MQTT_SERVER "192.168.188.30"
#define MQTT_SERVER WiFi.localIP().toString().c_str()
//#define MQTT_SERVER "fcce"

static MQTT *mqtt_client;
void myConnectedCb(void)
{
    log_msg("mqtt client connected.");
}

void myDisconnectedCb(void)
{
    static int errcount = 0;
    log_msg("mqtt client disconnected... reconnecting");
    delay(1000);
    mqtt_client->connect();
    delay(25);
    if (!mqtt_client->isConnected() &&
        (++errcount > 10))
    {
        log_msg("too many mqtt disconnections, rebooting...");
        ESP.restart();
    }
}

void myPublishedCb(void)
{
    //log_msg("mqtt client published cb...");
}

void myDataCb(String &topic, String &data)
{
    log_msg("fcce - " + topic + ": " + data);
    if (topic.startsWith("fcc/cc-alive"))
    {
        log_msg("fcc is alive (" + String((millis() - fcc_last_seen) / 1000) + "s), re-arming watchdog.");
        fcc_last_seen = millis();
        return;
    }
    if (topic.startsWith("fcc/reset-request"))
    {
        log_msg("fcc requested reset... rebooting");
        ESP.restart();
    }
}

void setup_mqtt(void)
{
    mqtt_client = new MQTT{my_clientID.c_str(), MQTT_SERVER, 1883};
    // setup callbacks
    mqtt_client->onConnected(myConnectedCb);
    mqtt_client->onDisconnected(myDisconnectedCb);
    mqtt_client->onPublished(myPublishedCb);
    mqtt_client->onData(myDataCb);
    log_msg("connect mqtt client...");
    delay(20);
    mqtt_client->connect();
    delay(20);
    mqtt_client->subscribe("fcce/config");
    mqtt_client->subscribe("fcc/#");
    log_msg("mqtt setup done");
    mqtt_publish("/sensor-alive", "Formicula embedded starting...");
    fcc_last_seen = millis(); /* give fcc 10min to connect, otherwise restart */
}

void loop_mqtt(void)
{
    if ((millis() - fcc_last_seen) > 240 * 1000)
    {
        log_msg("fcc not seen for 4min, rebooting.");
        ESP.restart();
    }
}

void mqtt_publish(String topic, String msg)
{
    static int errcount = 0;
    //log_msg("publishing: " + my_clientID + topic + msg);
    if (mqtt_client->publish(my_clientID + topic, msg, 0, 0))
    {
        errcount = 0;
        return;
    }

    /* zlorfik, mqtt sucks again */
    log_msg("mqtt not connected " + String(errcount) + "- discarding: " + topic + "-" + msg);
    if (++errcount > 10)
    {
        log_msg("too many mqtt errors, rebooting...");
        ESP.restart(); /* lets try a reboot */
    }
}

void mqtt_publish(const char *topic, const char *msg)
{
    mqtt_publish(String(topic), String(msg));
}
#endif