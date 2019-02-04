/*
 *  UnoMQTTDSCKeybusInterface.
 *  Using ENC28J60 Ethernet PHY in replacement of standard EthernetShield.
 *  Based on: HomeAssistant-MQTT 1.0 (Arduino) from https://github.com/taligentx/dscKeybusInterface
 *
 *  Processes the security system status and allows for control using Home Assistant via MQTT.
 *
 *  Home Assistant: https://www.home-assistant.io
 *  Mosquitto MQTT broker: https://mosquitto.org
 *
 *  The commands to set the alarm state are setup in Home Assistant with the partition number (1-8) as a prefix to the command:
 *    Partition 1 disarm: "1D"
 *    Partition 2 arm stay: "2S"
 *    Partition 2 arm away: "2A"
 *
 *  The interface listens for commands in the configured mqttSubscribeTopic, and publishes partition status in a
 *  separate topic per partition with the configured mqttPartitionTopic appended with the partition number:
 *    Disarmed: "disarmed"
 *    Arm stay: "armed_home"
 *    Arm away: "armed_away"
 *    Exit delay in progress: "pending"
 *    Alarm tripped: "triggered"
 *
 *  The trouble state is published as an integer in the configured mqttTroubleTopic:
 *    Trouble: "1"
 *    Trouble restored: "0"
 *
 *  Zone states are published as an integer in a separate topic per zone with the configured mqttZoneTopic appended
 *  with the zone number:
 *    Open: "1"
 *    Closed: "0"
 *
 *  Fire states are published as an integer in a separate topic per partition with the configured mqttFireTopic
 *  appended with the partition number:
 *    Fire alarm: "1"
 *    Fire alarm restored: "0"
 *
 *  Example Home Assistant configuration.yaml:

      # https://www.home-assistant.io/components/mqtt/
      mqtt:
        broker: URL or IP address
        client_id: homeAssistant

      # https://www.home-assistant.io/components/alarm_control_panel.mqtt/
      alarm_control_panel:
        - platform: mqtt
          name: "Security System Partition 1"
          state_topic: "alarmsys/get/Partition1"
          command_topic: "alarmsys/set"
          payload_disarm: "1D"
          payload_arm_home: "1S"
          payload_arm_away: "1A"
        - platform: mqtt
          name: "Security System Partition 2"
          state_topic: "alarmsys/get/Partition2"
          command_topic: "alarmsys/set"
          payload_disarm: "2D"
          payload_arm_home: "2S"
          payload_arm_away: "2A"

      # https://www.home-assistant.io/components/binary_sensor/
      binary_sensor:
        - platform: mqtt
          name: "Security Trouble"
          state_topic: "alarmsys/get/Trouble"
          device_class: "problem"
          payload_on: "1"
          payload_off: "0"
        - platform: mqtt
          name: "Smoke Alarm 1"
          state_topic: "alarmsys/get/Fire1"
          device_class: "smoke"
          payload_on: "1"
          payload_off: "0"
        - platform: mqtt
          name: "Smoke Alarm 2"
          state_topic: "alarmsys/get/Fire2"
          device_class: "smoke"
          payload_on: "1"
          payload_off: "0"
        - platform: mqtt
          name: "Zone 1"
          state_topic: "alarmsys/get/Zone1"
          device_class: "door"
          payload_on: "1"
          payload_off: "0"
        - platform: mqtt
          name: "Zone 2"
          state_topic: "alarmsys/get/Zone2"
          device_class: "window"
          payload_on: "1"
          payload_off: "0"
        - platform: mqtt
          name: "Zone 3"
          state_topic: "alarmsys/get/Zone3"
          device_class: "motion"
          payload_on: "1"
          payload_off: "0"

 *  Wiring:
 *      DSC Aux(-) --- Arduino ground
 *
 *                                         +--- dscClockPin (Arduino Uno: 2,3)
 *      DSC Yellow --- 15k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *                                         +--- dscReadPin (Arduino Uno: 2-12)
 *      DSC Green ---- 15k ohm resistor ---|
 *                                         +--- 10k ohm resistor --- Ground
 *
 *  Virtual keypad (optional):
 *      DSC Green ---- NPN collector --\
 *                                      |-- NPN base --- 1k ohm resistor --- dscWritePin (Arduino Uno: 2-12)
 *            Ground --- NPN emitter --/
 *
 *  Power (when disconnected from USB):
 *      DSC Aux(+) --- Arduino Vin pin
 *
 *  Virtual keypad uses an NPN transistor to pull the data line low - most small signal NPN transistors should
 *  be suitable, for example:
 *   -- 2N3904
 *   -- BC547, BC548, BC549
 *
 *  Issues and (especially) pull requests are welcome:
 *  https://github.com/taligentx/dscKeybusInterface
 */
#include <UIPEthernet.h>
#include <PubSubClient.h>
#include <dscKeybusInterface.h>

#define UARTBAUD                (115200)

// MQTT Properties
IPAddress const MQTTBrokerIP    (192, 168, 1, 254);
#define MQTTBrokerPort          (1883)
#define MQTTBrokerUser          "" // Username this device should connect with
#define MQTTBrokerPass          "" // Password this device should connect with    
#define MQTTClientName          "alarmsys"
#define MQTTPartitionTopic      "alarmsys/get/partition"  // Sends armed and alarm status per partition: alarmsys/get/Partition1 ... alarmsys/get/Partition8
#define MQTTZoneTopic           "alarmsys/get/zone"            // Sends zone status per zone: alarmsys/get/Zone1 ... alarmsys/get/Zone64
#define MQTTFireTopic           "alarmsys/get/fire"            // Sends fire status per partition: alarmsys/get/Fire1 ... alarmsys/get/Fire8
#define MQTTTroubleTopic        "alarmsys/get/trouble"      // Sends trouble status
#define MQTTSubscribeTopic      "alarmsys/set"            // Receives messages to write to the panel
#define MQTTPubAvailable        "alarmsys/get/available"
#define MQTTWillQos             (0)
#define MQTTWillRetain          (0)
#define MQTTAvailablePayload    "online"
#define MQTTUnavailablePayload  "offline"
#define MQTTNotRetain           (false)
#define MQTTRetain              (true)
#define ConnectBrokerRetryInterval_ms (2000)
#define PublishAvailableInterval_ms   (30000)


// Configures the Keybus interface with the specified pins - dscWritePin is optional, leaving it out disables the
// virtual keypad.
#define dscClockPin (2)  // Arduino Uno hardware interrupt pin: 2,3
#define dscReadPin  (3)  // Arduino Uno: 2-12
#define dscWritePin (4)  // Arduino Uno: 2-12
#define accessCode  ""   // An access code is required to disarm/night arm and may be required to arm based on panel configuration.


// Network properties
uint8_t const mac[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 };
IPAddress const ip(192, 168, 1, 190);
IPAddress const gateway(192, 168, 0, 1);
IPAddress const subnet(255,255,254,0);

// Class definitions
EthernetClient ethClient;
PubSubClient mqtt(MQTTBrokerIP, MQTTBrokerPort, ethClient);
dscKeybusInterface dsc(dscClockPin, dscReadPin, dscWritePin);

// Function prototypes
void mqttCallback (char* topic, byte* payload, unsigned int length);
void mqttHandle (void);
static void publishMQTTMessage (char const * const sMQTTSubscription, char const * const sMQTTData, bool retain);
static void advanceTimers (void);

// Static variables
static uint32_t mqttActionTimer;
static unsigned long previous;

void setup (void) 
{
  Serial.begin(UARTBAUD);
  Serial.println();

  // Initializes ethernet with DHCP
  Serial.println(F("Init Ethernet."));
  Ethernet.begin(mac, ip, MQTTBrokerIP, gateway, subnet);
  Serial.print(F("IP address: "));
  Serial.println(Ethernet.localIP());

  mqtt.setCallback(mqttCallback);

  // Starts the Keybus interface and optionally specifies how to print data.
  // begin() sets Serial by default and can accept a different stream: begin(Serial1), etc.
  dsc.begin();

  mqttActionTimer = 0;
  previous = 0;
  Serial.println(F("Setup Complete."));
}


void loop (void) 
{
  mqttHandle();

  if(dsc.handlePanel() && dsc.statusChanged)   // Processes data only when a valid Keybus command has been read
  {
    dsc.statusChanged = false;                   // Reset the status tracking flag

    // If the Keybus data buffer is exceeded, the sketch is too busy to process all Keybus commands.  Call
    // handlePanel() more often, or increase dscBufferSize in the library: src/dscKeybusInterface.h
    if(dsc.bufferOverflow) 
    {
      Serial.println(F("Keybus buffer overflow"));
    }

    dsc.bufferOverflow = false;

    // Sends the access code when needed by the panel for arming
    if(dsc.accessCodePrompt && dsc.writeReady) 
    {
      dsc.accessCodePrompt = false;
      dsc.write(accessCode);
    }

    if(dsc.troubleChanged) 
    {
      dsc.troubleChanged = false;  // Resets the trouble status flag
      if(dsc.trouble) 
      {
        publishMQTTMessage(MQTTTroubleTopic, "1", true);
      }
      else 
      {
        publishMQTTMessage(MQTTTroubleTopic, "0", true);
      }
    }
    // Publishes status per partition
    for(byte partition = 0; partition < dscPartitions; partition++) 
    {
      // Publishes exit delay status
      if(dsc.exitDelayChanged[partition]) 
      {
        dsc.exitDelayChanged[partition] = false;  // Resets the exit delay status flag

        // Appends the mqttPartitionTopic with the partition number
        char publishTopic[strlen(MQTTPartitionTopic) + 1];
        char partitionNumber[2];
        strcpy(publishTopic, MQTTPartitionTopic);
        itoa(partition + 1, partitionNumber, 10);
        strcat(publishTopic, partitionNumber);

        if(dsc.exitDelay[partition]) 
        {
          publishMQTTMessage(publishTopic, "pending", true);  // Publish as a retained message
        }
        else if((false == dsc.exitDelay[partition]) && (false == dsc.armed[partition])) 
        {
          publishMQTTMessage(publishTopic, "disarmed", true);
        }
      }

      // Publishes armed/disarmed status
      if(dsc.armedChanged[partition]) 
      {
        dsc.armedChanged[partition] = false;  // Resets the partition armed status flag

        // Appends the mqttPartitionTopic with the partition number
        char publishTopic[strlen(MQTTPartitionTopic) + 1];
        char partitionNumber[2];
        strcpy(publishTopic, MQTTPartitionTopic);
        itoa(partition + 1, partitionNumber, 10);
        strcat(publishTopic, partitionNumber);

        if(dsc.armed[partition]) 
        {
          if(dsc.armedAway[partition]) 
          {
            publishMQTTMessage(publishTopic, "armed_away", true);
          }
          else if(dsc.armedStay[partition]) 
          {
            publishMQTTMessage(publishTopic, "armed_home", true);
          }
        }
        else 
        {
          publishMQTTMessage(publishTopic, "disarmed", true);
        }
      }

      // Publishes alarm status
      if(dsc.alarmChanged[partition]) 
      {
        dsc.alarmChanged[partition] = false;  // Resets the partition alarm status flag
        if (dsc.alarm[partition]) 
        {

          // Appends the mqttPartitionTopic with the partition number
          char publishTopic[strlen(MQTTPartitionTopic) + 1];
          char partitionNumber[2];
          strcpy(publishTopic, MQTTPartitionTopic);
          itoa(partition + 1, partitionNumber, 10);
          strcat(publishTopic, partitionNumber);

          publishMQTTMessage(publishTopic, "triggered", true);  // Alarm tripped
        }
      }

      // Publishes fire alarm status
      if(dsc.fireChanged[partition]) 
      {
        dsc.fireChanged[partition] = false;  // Resets the fire status flag

        // Appends the mqttFireTopic with the partition number
        char firePublishTopic[strlen(MQTTFireTopic) + 1];
        char partitionNumber[2];
        strcpy(firePublishTopic, MQTTFireTopic);
        itoa(partition + 1, partitionNumber, 10);
        strcat(firePublishTopic, partitionNumber);

        if(dsc.fire[partition]) 
        {
          publishMQTTMessage(firePublishTopic, "1", false);  // Fire alarm tripped
        }
        else 
        {
          publishMQTTMessage(firePublishTopic, "0", false);  // Fire alarm restored
        }
      }
    }

    // Publishes zones 1-64 status in a separate topic per zone
    // Zone status is stored in the openZones[] and openZonesChanged[] arrays using 1 bit per zone, up to 64 zones:
    //   openZones[0] and openZonesChanged[0]: Bit 0 = Zone 1 ... Bit 7 = Zone 8
    //   openZones[1] and openZonesChanged[1]: Bit 0 = Zone 9 ... Bit 7 = Zone 16
    //   ...
    //   openZones[7] and openZonesChanged[7]: Bit 0 = Zone 57 ... Bit 7 = Zone 64
    if(dsc.openZonesStatusChanged) 
    {
      dsc.openZonesStatusChanged = false;                           // Resets the open zones status flag
      for(byte zoneGroup = 0; zoneGroup < dscZones; zoneGroup++) 
      {
        for(byte zoneBit = 0; zoneBit < 8; zoneBit++) 
        {
          if(bitRead(dsc.openZonesChanged[zoneGroup], zoneBit))   // Checks an individual open zone status flag
          {
            bitWrite(dsc.openZonesChanged[zoneGroup], zoneBit, 0);  // Resets the individual open zone status flag

            // Appends the mqttZoneTopic with the zone number
            char zonePublishTopic[strlen(MQTTZoneTopic) + 2];
            char zone[3];
            strcpy(zonePublishTopic, MQTTZoneTopic);
            itoa(zoneBit + 1 + (zoneGroup * 8), zone, 10);
            strcat(zonePublishTopic, zone);

            if(bitRead(dsc.openZones[zoneGroup], zoneBit)) 
            {
              publishMQTTMessage(zonePublishTopic, "1", true); // Zone open
            }
            else 
            {
              publishMQTTMessage(zonePublishTopic, "0", true); // Zone closed
            }
          }
        }
      }
    }
  }

  advanceTimers();
}


// Handles messages received in the mqttSubscribeTopic
void mqttCallback (char* topic, byte* payload, unsigned int length) 
{
  // Handles unused parameters
  (void)length;

  // Debug info
#define MQTTPayloadMaxExpectedSize (3) 
  char szTemp[MQTTPayloadMaxExpectedSize + sizeof('\0')];
  memset(szTemp, 0x00, MQTTPayloadMaxExpectedSize + sizeof('\0'));
  memcpy(szTemp, payload, length > MQTTPayloadMaxExpectedSize ? MQTTPayloadMaxExpectedSize : length);

  Serial.print(F("MQTT in : "));
  Serial.print(topic);
  Serial.print(F(" "));
  Serial.println(szTemp);


  byte partition = 0;
  byte payloadIndex = 0;

  // Checks if a partition number 1-8 has been sent and sets the second character as the payload
  if(('1' <= payload[0]) && ('8' >= payload[0])) 
  {
    partition = payload[0] - '1';
    payloadIndex = 1;
  }

  // Arm stay
  if(('S' == payload[payloadIndex]) && (false == dsc.armed[partition]) && (false == dsc.exitDelay[partition])) 
  {
    while(false == dsc.writeReady) 
    {
      dsc.handlePanel();  // Continues processing Keybus data until ready to write
    }

    dsc.writePartition = partition + 1;         // Sets writes to the partition number
    dsc.write('s');                             // Virtual keypad arm stay
  }

  // Arm away
  else if(('A' == payload[payloadIndex]) && (false == dsc.armed[partition]) && (false == dsc.exitDelay[partition])) 
  {
    while(false == dsc.writeReady) 
    {
      dsc.handlePanel();  // Continues processing Keybus data until ready to write
    }

    dsc.writePartition = partition + 1;         // Sets writes to the partition number
    dsc.write('w');                             // Virtual keypad arm away
  }

  // Disarm
  else if(('D' == payload[payloadIndex]) && (dsc.armed[partition] || dsc.exitDelay[partition])) 
  {
    while(false == dsc.writeReady) 
    {
      dsc.handlePanel();  // Continues processing Keybus data until ready to write
    }
    
    dsc.writePartition = partition + 1;         // Sets writes to the partition number
    dsc.write(accessCode);
  }
}


void mqttHandle (void) 
{
  // If not MQTT connected, try connecting
  if(false == mqtt.connected())  
  {
    // Connect to MQTT broker, retry periodically
    if(0 == mqttActionTimer)
    {
      if(false == mqtt.connect(MQTTClientName, MQTTBrokerUser, MQTTBrokerPass, MQTTPubAvailable, MQTTWillQos, MQTTWillRetain, MQTTUnavailablePayload)) 
      {
        mqttActionTimer = ConnectBrokerRetryInterval_ms;
        Serial.println(F("MQTT connection failed."));
      } 
      else 
      {
        Serial.println(F("MQTT connected."));
        mqtt.subscribe(MQTTSubscribeTopic); 
      }
    }
  }
  else 
  {
    if(0 == mqttActionTimer)
    {
      mqttActionTimer = PublishAvailableInterval_ms;

      publishMQTTMessage(MQTTPubAvailable, MQTTAvailablePayload, MQTTNotRetain);  
    }

    mqtt.loop();
  }
}

// Publish MQTT data to MQTT broker
static void publishMQTTMessage (char const * const sMQTTSubscription, char const * const sMQTTData, bool retain)
{
  // Define and send message about door state
  mqtt.publish(sMQTTSubscription, sMQTTData, retain); 

  // Debug info
  Serial.print(F("MQTT Out : "));
  Serial.print(sMQTTSubscription);
  Serial.print(F(" "));
  Serial.println(sMQTTData);

}

static void advanceTimers (void)
{
  unsigned long const current = millis();
  if(current != previous)
  {
    previous = current;

    if(mqttActionTimer)
    {
      mqttActionTimer--;
    }
  }
}
