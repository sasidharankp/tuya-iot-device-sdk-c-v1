#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <errno.h>
#include <wiringPi.h>

#include "tuya_log.h"
#include "tuya_config.h"
#include "tuya_iot.h"
#include "cJSON.h"

/* Beginning Of Pin Configurations*/
#define DHT_PIN 7
#define DOOR_PIN 0
#define SWITCH_PIN 2
#define UV_LED_PIN 6
#define ALARM_PIN 26
#define FAN_PIN 27
#define LIGHT_PIN 28
#define SPRINKLER_PIN 29
int INPUT_PINS[] = {0, 2, 7};
int OUTPUT_PINS[] = {6, 26, 27, 28, 29};
/* End Of Pin Configurations*/

const char *config_string = "{\"101\":{\"gpio_pin\":\"7\",\"dp_name\":\"Temperature\"},\"102\":{\"gpio_pin\":\"7\",\"dp_name\":\"Humidity\"},\"103\":{\"gpio_pin\":\"0\",\"dp_name\":\"Door\"},\"104\":{\"gpio_pin\":\"2\",\"dp_name\":\"Switch\"},\"105\":{\"gpio_pin\":\"6\",\"dp_name\":\"UV Light\"},\"106\":{\"gpio_pin\":\"27\",\"dp_name\":\"Exhaust Fan\"},\"107\":{\"gpio_pin\":\"26\",\"dp_name\":\"Alarm\"},\"108\":{\"gpio_pin\":\"28\",\"dp_name\":\"Night Light\"},\"109\":{\"gpio_pin\":\"29\",\"dp_name\":\"Sprinkler Switch\"}}";
const cJSON *config_JSON;
/* Beginning Of Data points(DP) declaration*/
#define TEMP_DP "101"
#define HUMID_DP "102"
#define DOOR_DP "103"
#define SWITCH_DP "104"
#define UV_LED_DP "105"
#define FAN_DP "106"
#define ALARM_DP "107"
#define LIGHT_DP "108"
#define SPRINKLER_DP "109"
/* End Of Data points(DP) declaration */

/* Beginning Of Constants declaration*/
#define MAXTIMINGS 85
#define SOFTWARE_VER "1.0.0"
/* END Of Constants declaration*/

/* Beginning Of Variables declaration*/
int dht11_dat[5] = {0, 0, 0, 0, 0};

static char *bool_array[2] = {"false", "true"};
static char *bool_array1[2] = {"OFF", "ON"};
char message[20];

time_t last_update;
static int sensor_data[4];

/* END Of Variables declaration*/

/* for APP QRCode scan test */
extern void example_qrcode_print(const char *productkey, const char *uuid);

/* Tuya device handle */
tuya_iot_client_t client;

/* Hardware switch control function */
void actuate_hardware(int pin_number, int value, char *device_name)
{

    TY_LOGI("%s is %s", device_name, bool_array1[value]);
    digitalWrite(pin_number, value);
}

/* DP data reception processing function */
void user_dp_download_on(tuya_iot_client_t *client, const char *json_dps)
{
    TY_LOGD("Data point download value: %s", json_dps);

    /* Parsing json string to cJSON object */
    cJSON *dps = cJSON_Parse(json_dps);
    if (dps == NULL)
    {
        TY_LOGE("JSON parsing error, exit!");
        return;
    }

    char *DP_KEY = dps->child->string;
    int DP_VALUE = dps->child->valueint;

    cJSON *device_details = cJSON_GetObjectItem(config_JSON, DP_KEY);
    int PIN_NUMBER = atoi(device_details->child->valuestring);
    char *DEVICE_NAME = device_details->child->next->valuestring;
    printf("Pin Number of %s is %d and the Device Name is %s \n", DP_KEY, PIN_NUMBER, DEVICE_NAME);

    actuate_hardware(PIN_NUMBER, DP_VALUE, DEVICE_NAME);

    /* relese cJSON DPS object */
    cJSON_Delete(dps);

    /* Report the received data to synchronize the switch status. */
    tuya_iot_dp_report_json(client, json_dps);
}

/* Tuya OTA event callback */
void user_upgrade_notify_on(tuya_iot_client_t *client, cJSON *upgrade)
{
    TY_LOGI("----- Upgrade information -----");
    TY_LOGI("OTA Channel: %d", cJSON_GetObjectItem(upgrade, "type")->valueint);
    TY_LOGI("Version: %s", cJSON_GetObjectItem(upgrade, "version")->valuestring);
    TY_LOGI("Size: %s", cJSON_GetObjectItem(upgrade, "size")->valuestring);
    TY_LOGI("MD5: %s", cJSON_GetObjectItem(upgrade, "md5")->valuestring);
    TY_LOGI("HMAC: %s", cJSON_GetObjectItem(upgrade, "hmac")->valuestring);
    TY_LOGI("URL: %s", cJSON_GetObjectItem(upgrade, "url")->valuestring);
    TY_LOGI("HTTPS URL: %s", cJSON_GetObjectItem(upgrade, "httpsUrl")->valuestring);
}

/* Tuya SDK event callback */
static void user_event_handler_on(tuya_iot_client_t *client, tuya_event_msg_t *event)
{
    TY_LOGD("Tuya Event ID:%d(%s)", event->id, EVENT_ID2STR(event->id));
    switch (event->id)
    {
    case TUYA_EVENT_BIND_START:
        /* Print the QRCode for Tuya APP bind */
        example_qrcode_print(client->config.productkey, client->config.uuid);
        break;

    case TUYA_EVENT_MQTT_CONNECTED:
        TY_LOGI("Device MQTT Connected!");
        break;

    case TUYA_EVENT_DP_RECEIVE:
        user_dp_download_on(client, (const char *)event->value.asString);
        break;

    case TUYA_EVENT_UPGRADE_NOTIFY:
        user_upgrade_notify_on(client, event->value.asJSON);
        break;

    default:
        break;
    }
}

void publish_sensor_data(char *dp, int sensor_value, _Bool bool_data_type)
{
    if (bool_data_type)
    {
        sprintf(message, "{\"%s\":%s}", dp, bool_array[sensor_value]);
    }
    else
    {
        sprintf(message, "{\"%s\":%d}", dp, sensor_value);
    }
    //printf("Publishing Sensor Data %s \n",message);
    tuya_iot_dp_report_json(&client, message);
}

void switch_interrupt(void)
{
    if (digitalRead(SWITCH_PIN))
    {
        sensor_data[2] = !sensor_data[2];
        //printf( "LED is %d\n", sensor_data[2]);
        digitalWrite(UV_LED_PIN, sensor_data[2]);
        publish_sensor_data(UV_LED_DP, sensor_data[2], TRUE);
    }
}

void door_interrupt(void)
{
    if (sensor_data[3] != digitalRead(DOOR_PIN))
    {
        //if((time(NULL)-door_state_update>=1)){
        //printf( "Door is %d\n", digitalRead(DOOR_PIN) );
        sensor_data[3] = digitalRead(DOOR_PIN);
        publish_sensor_data(DOOR_DP, sensor_data[3], TRUE);
        //door_state_update= time(NULL);
        //}
    }
}

void get_dht_data()
{
    uint8_t laststate = HIGH;
    uint8_t counter = 0;
    uint8_t j = 0, i;

    dht11_dat[0] = dht11_dat[1] = dht11_dat[2] = dht11_dat[3] = dht11_dat[4] = 0;

    /* pull pin down for 18 milliseconds */
    pinMode(DHT_PIN, OUTPUT);
    digitalWrite(DHT_PIN, LOW);
    delay(18);
    /* then pull it up for 40 microseconds */
    digitalWrite(DHT_PIN, HIGH);
    delayMicroseconds(40);
    /* prepare to read the pin */
    pinMode(DHT_PIN, INPUT);

    /* detect change and read data */
    for (i = 0; i < MAXTIMINGS; i++)
    {
        counter = 0;
        while (digitalRead(DHT_PIN) == laststate)
        {
            counter++;
            delayMicroseconds(1);
            if (counter == 255)
            {
                break;
            }
        }
        laststate = digitalRead(DHT_PIN);

        if (counter == 255)
            break;

        /* ignore first 3 transitions */
        if ((i >= 4) && (i % 2 == 0))
        {
            /* shove each bit into the storage bytes */
            dht11_dat[j / 8] <<= 1;
            if (counter > 50) /* <- !! here !! */
                dht11_dat[j / 8] |= 1;
            j++;
        }
    }

    if ((j >= 40) && (dht11_dat[4] == ((dht11_dat[0] + dht11_dat[1] + dht11_dat[2] + dht11_dat[3]) & 0xFF)))
    {
        if (sensor_data[0] != dht11_dat[2])
        {
            sensor_data[0] = dht11_dat[2];
            publish_sensor_data(TEMP_DP, sensor_data[0], FALSE);
        }
        if (sensor_data[1] != dht11_dat[0])
        {
            sensor_data[1] = dht11_dat[0];
            publish_sensor_data(HUMID_DP, sensor_data[1], FALSE);
        }
    }
    else
        printf("Temperature Sensor Error\n");
}

PI_THREAD(sensor_thread)
{

    if (wiringPiISR(SWITCH_PIN, INT_EDGE_BOTH, &switch_interrupt) < 0 || wiringPiISR(DOOR_PIN, INT_EDGE_BOTH, &door_interrupt) < 0)
    {
        fprintf(stderr, "Unable to setup ISR: %s\n", strerror(errno));
        exit(1);
    }

    while (1)
    {
        if ((time(NULL) - last_update >= 5))
        {
            //printf("last update at %ld, time now %ld, differnce is %ld \n", last_update, time(NULL), (time(NULL)-last_update));
            get_dht_data();
            //printf( "Temperature = %d Humidity = %d %%\n",temp_sensor_values[2], temp_sensor_values[0]);
            last_update = time(NULL);
        }
    }
}

int wiringPI_setup()
{
    if (wiringPiSetup() < 0)
    {
        fprintf(stderr, "Unable to setup wiringPi: %s\n", strerror(errno));
        return 1;
    }
    int i;
    for (i = 0; i < sizeof(INPUT_PINS) / sizeof(int); i++)
    {
        pinMode(INPUT_PINS[i], INPUT);
    }
    int j;
    for (j = 0; j < sizeof(OUTPUT_PINS) / sizeof(int); j++)
    {
        pinMode(OUTPUT_PINS[j], OUTPUT);
    }
}

void config_setup()
{
    config_JSON = cJSON_Parse(config_string);

    if (config_JSON == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            printf("JSON Parse Error");
        }
        exit(1);
    }
}

int main(int argc, char **argv)
{
    config_setup();

    wiringPI_setup();
    if (piThreadCreate(sensor_thread) != 0)
    {
        printf("Unable to Start sensor_thread\n");
        exit(1);
    }
    int ret = OPRT_OK;

    /* Initialize Tuya device configuration */
    ret = tuya_iot_init(&client, &(const tuya_iot_config_t){
                                     .software_ver = SOFTWARE_VER,
                                     .productkey = TUYA_PRODUCT_KEY,
                                     .uuid = TUYA_DEVICE_UUID,
                                     .authkey = TUYA_DEVICE_AUTHKEY,
                                     .event_handler = user_event_handler_on});

    assert(ret == OPRT_OK);

    /* Start tuya iot task */
    tuya_iot_start(&client);

    //publish_sensor_data(DOOR_PIN, digitalRead(DOOR_PIN), TRUE);

    for (;;)
    {
        /* Loop to receive packets, and handles client keepalive */
        tuya_iot_yield(&client);
    }

    return ret;
}
