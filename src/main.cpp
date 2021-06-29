/*
 *  UnoMQTTDSCKeybusInterface.
 *  Using ENC28J60 Ethernet PHY in replacement of standard EthernetShield.
 *  Based on: HomeAssistant-MQTT 1.4 (Arduino) from https://github.com/taligentx/dscKeybusInterface
 *
  *  Processes the security system status and allows for control using Home Assistant via MQTT.
 *
 *  Home Assistant: https://www.home-assistant.io
 *  Mosquitto MQTT broker: https://mosquitto.org
 *
 *  Usage:
 *    1. Set the security system access code to permit disarming through Home Assistant.
 *    2. Set the MQTT server address in the sketch.
 *    3. Copy the example configuration to Home Assistant's configuration.yaml and customize.
 *    4. Upload the sketch.
 *    5. Restart Home Assistant.
 *
 *  Example Home Assistant configuration.yaml for 2 partitions, 3 zones:

# https://www.home-assistant.io/components/mqtt/
mqtt:
  broker: URL or IP address
  client_id: homeAssistant

# https://www.home-assistant.io/components/alarm_control_panel.mqtt/
alarm_control_panel:
  - platform: mqtt
    name: "Security Partition 1"
    state_topic: "dsc/Get/Partition1"
    availability_topic: "dsc/Status"
    command_topic: "dsc/Set"
    payload_disarm: "1D"
    payload_arm_home: "1S"
    payload_arm_away: "1A"
    payload_arm_night: "1N"
  - platform: mqtt
    name: "Security Partition 2"
    state_topic: "dsc/Get/Partition2"
    availability_topic: "dsc/Status"
    command_topic: "dsc/Set"
    payload_disarm: "2D"
    payload_arm_home: "2S"
    payload_arm_away: "2A"
    payload_arm_night: "2N"

# https://www.home-assistant.io/components/sensor.mqtt/
sensor:
  - platform: mqtt
    name: "Security Partition 1"
    state_topic: "dsc/Get/Partition1/Message"
    availability_topic: "dsc/Status"
    icon: "mdi:shield"
  - platform: mqtt
    name: "Security Partition 2"
    state_topic: "dsc/Get/Partition2/Message"
    availability_topic: "dsc/Status"
    icon: "mdi:shield"

# https://www.home-assistant.io/components/binary_sensor.mqtt/
binary_sensor:
  - platform: mqtt
    name: "Security Trouble"
    state_topic: "dsc/Get/Trouble"
    device_class: "problem"
    payload_on: "1"
    payload_off: "0"
  - platform: mqtt
    name: "Smoke Alarm 1"
    state_topic: "dsc/Get/Fire1"
    device_class: "smoke"
    payload_on: "1"
    payload_off: "0"
  - platform: mqtt
    name: "Smoke Alarm 2"
    state_topic: "dsc/Get/Fire2"
    device_class: "smoke"
    payload_on: "1"
    payload_off: "0"
  - platform: mqtt
    name: "Zone 1"
    state_topic: "dsc/Get/Zone1"
    device_class: "door"
    payload_on: "1"
    payload_off: "0"
  - platform: mqtt
    name: "Zone 2"
    state_topic: "dsc/Get/Zone2"
    device_class: "window"
    payload_on: "1"
    payload_off: "0"
  - platform: mqtt
    name: "Zone 3"
    state_topic: "dsc/Get/Zone3"
    device_class: "motion"
    payload_on: "1"
    payload_off: "0"
  - platform: mqtt
    name: "PGM 1"
    state_topic: "dsc/Get/PGM1"
    payload_on: "1"
    payload_off: "0"
  - platform: mqtt
    name: "PGM 8"
    state_topic: "dsc/Get/PGM8"
    payload_on: "1"
    payload_off: "0"

 *  Example button card configuration to add a panic button: https://www.home-assistant.io/lovelace/button/

type: entity-button
name: Panic alarm
tap_action:
  action: call-service
  service: mqtt.publish
  service_data:
    payload: P
    topic: dsc/Set
hold_action:
  action: none
show_icon: true
show_name: true
entity: alarm_control_panel.security_partition_1

 *  The commands to set the alarm state are setup in Home Assistant with the partition number (1-8) as a
 *  prefix to the command, except to trigger the panic alarm:
 *    Partition 1 disarm: "1D"
 *    Partition 2 arm stay: "2S"
 *    Partition 2 arm away: "2A"
 *    Partition 1 arm night: "1N"
 *    Panic alarm: "P"
 *
 *  The interface listens for commands in the configured mqttSubscribeTopic, and publishes partition status in a
 *  separate topic per partition with the configured mqttPartitionTopic appended with the partition number:
 *    Disarmed: "disarmed"
 *    Arm stay: "armed_home"
 *    Arm away: "armed_away"
 *    Arm night: "armed_night"
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
 *  PGM outputs states are published as an integer in a separate topic per PGM with the configured mqttPgmTopic
 *  appended with the PGM output number:
 *    Open: "1"
 *    Closed: "0"
 *
 *  Release notes
 *    1.4 - Added PGM outputs 1-14 status
 *    1.2 - Added night arm (arming with no entry delay)
 *          Added status update on initial MQTT connection and reconnection
 *          Add appendPartition() to simplify sketch
 *          Removed writeReady check, moved into library
 *    1.1 - Added status update on initial MQTT connection and reconnection
 *    1.0 - Initial release
 *
 *  Wiring:
 *      DSC Aux(+) --- Arduino Vin pin
 *
 *      DSC Aux(-) --- Arduino Ground
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
 *  Virtual keypad uses an NPN transistor to pull the data line low - most small signal NPN transistors should
 *  be suitable, for example:
 *   -- 2N3904
 *   -- BC547, BC548, BC549
 *
 *  Issues and (especially) pull requests are welcome:
 *  https://github.com/taligentx/dscKeybusInterface
 *
 *  This example code is in the public domain.
 */
#define MQTT_KEEPALIVE 60

#include "secret.h"
#include <UIPEthernet.h>
#include <PubSubClient.h>
#include <dscKeybusInterface.h>

#define VERSION "1.1"

#define UARTBAUD                    (115200)

#define NULLTERM_LEN                (sizeof('\0'))

// MQTT Properties
IPAddress const MQTTBrokerIP        (192, 168, 1, 1);
#define MQTTBrokerPort              (1883)
#define MQTTBrokerUser              SecretMQTTUsername // Username this device should connect with. Define string in secret.h
#define MQTTBrokerPass              SecretMQTTPassword // Password this device should connect with. Define string in secret.h
#define MQTTClientName              "alarmsys"
#define MQTTTopicPrefix             MQTTClientName
#define MQTTTopicGet                "/get"
#define MQTTTopicSet                "/set"
#define MQTTPartitionTopic          MQTTTopicPrefix MQTTTopicGet "/partition"  // Sends armed and alarm status per partition: alarmsys/get/Partition1 ... alarmsys/get/Partition8
#define MQTTZoneTopic               MQTTTopicPrefix MQTTTopicGet "/zone"       // Sends zone status per zone: alarmsys/get/Zone1 ... alarmsys/get/Zone64
#define MQTTPGMTopic                MQTTTopicPrefix MQTTTopicGet "/pgm"        // Sends pgm status per zone: alarmsys/get/pgm1 ... alarmsys/get/pgm64
#define MQTTFireTopic               MQTTTopicPrefix MQTTTopicGet "/fire"       // Sends fire status per partition: alarmsys/get/Fire1 ... alarmsys/get/Fire8
#define MQTTTroubleTopic            MQTTTopicPrefix MQTTTopicGet "/trouble"    // Sends trouble status
#define MQTTSubscribeTopic          MQTTTopicPrefix MQTTTopicSet               // Receives messages to write to the panel
#define MQTTPubAvailable            MQTTTopicPrefix MQTTTopicGet "/available"
#define MQTTPubParitionNumberlen    (2 * sizeof(char))
#define MQTTSubPayloadArmSuffix     ('A')
#define MQTTSubPayloadDisarmSuffix  ('D')
#define MQTTSubPayloadArmStaySuffix ('S')
#define MQTTSubPayloadSilenceSuffix ('T')
#define MQTTSubPayloadNightSuffix   ('N')
#define MQTTPubPayloadArm           "armed_away"
#define MQTTPubPayloadDisarm        "disarmed"
#define MQTTPubPayloadArmStay       "armed_home"
#define MQTTPubPayloadArmNight      "armed_night"
#define MQTTPubPayloadPending       "pending"
#define MQTTPubPayloadAlarmTrigger  "triggered"
#define MQTTPubPayloadZoneTrigger   "1"
#define MQTTPubPayloadZoneIdle      "0"
#define MQTTPubPayloadFireTrigger   "1"
#define MQTTPubPayloadFireIdle      "0"
#define MQTTPubPayloadTroubleActive "1"
#define MQTTPubPayloadTroubleIdle   "0"
#define MQTTWillQos                 (0)
#define MQTTWillRetain              (1)
#define MQTTAvailablePayload        "online"
#define MQTTUnavailablePayload      "offline"
#define MQTTNotRetain               (false)
#define MQTTRetain                  (true)
#define ConnectBrokerRetryInterval_ms (2000)

// Configures the Keybus interface with the specified pins - dscWritePin is optional, leaving it out disables the
// virtual keypad.
#define dscClockPin (2)  // Arduino Uno hardware interrupt pin: 2,3
#define dscReadPin  (3)  // Arduino Uno: 2-12
#define dscWritePin (4)  // Arduino Uno: 2-12
#define accessCode  SecretDscAccessCode   // An access code is required to disarm/night arm and may be required to arm based on panel configuration. Define string in secret.h
#define DefaultPartitionId (1)


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
boolean mqttHandle (void);
static bool publishMQTTMessage (char const * const sMQTTSubscription, char const * const sMQTTData, bool retain);
static void advanceTimers (void);
static void appendPartition(const char* sourceTopic, byte sourceNumber, char* publishTopic);

// Static variables
static uint32_t mqttActionTimer;
static unsigned long previous;

void setup (void) 
{
  Serial.begin(UARTBAUD);

  Serial.print(F("DSC MQTT interface "));
  Serial.println(F(VERSION));

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
  if(true == mqttHandle()) // Only process if connected to MQTT broker
  {
    dsc.loop();

    if(dsc.statusChanged)   // Processes data only when a valid Keybus command has been read
    {
      dsc.statusChanged = false;                   // Reset the status tracking flag

      // If the Keybus data buffer is exceeded, the sketch is too busy to process all Keybus commands.  Call
      // handlePanel() more often, or increase dscBufferSize in the library: src/dscKeybusInterface.h
      if(dsc.bufferOverflow) 
      {
        Serial.println(F("Keybus buffer overflow"));
        dsc.bufferOverflow = false;
      }

      // Checks if the interface is connected to the Keybus
      if (dsc.keybusChanged) 
      {
        bool messageSent = false;

        if (dsc.keybusConnected) 
        {
          messageSent = publishMQTTMessage(MQTTPubAvailable, MQTTAvailablePayload, MQTTRetain);  
        }
        else 
        {
          messageSent = publishMQTTMessage(MQTTPubAvailable, MQTTUnavailablePayload, MQTTRetain);  
        }

        if(messageSent)
        {
          dsc.keybusChanged = false;  // Resets the Keybus data status flag
        }
      }

      // Sends the access code when needed by the panel for arming
      if(dsc.accessCodePrompt) 
      {
        dsc.accessCodePrompt = false;
        dsc.write(accessCode);
      }

      if(dsc.troubleChanged) 
      {
        bool messageSent = false;

        if(dsc.trouble) 
        {
          messageSent = publishMQTTMessage(MQTTTroubleTopic, MQTTPubPayloadTroubleActive, MQTTRetain);
        }
        else 
        {
          messageSent = publishMQTTMessage(MQTTTroubleTopic, MQTTPubPayloadTroubleIdle, MQTTRetain);
        }
        
        if(messageSent)
        {
          dsc.troubleChanged = false;  // Resets the trouble status flag
        }
      }
      // Publishes status per partition
      for(byte partition = 0; partition < dscPartitions; partition++) 
      {

        // Skips processing if the partition is disabled or in installer programming
        if (dsc.disabled[partition]) 
        {
          continue;
        }

        // Publishes armed/disarmed status
        if(dsc.armedChanged[partition]) 
        {
          // Appends the mqttPartitionTopic with the partition number
          char publishTopic[strlen(MQTTPartitionTopic) + MQTTPubParitionNumberlen];
          appendPartition(MQTTPartitionTopic, partition, publishTopic);  // Appends the mqttPartitionTopic with the partition number

          bool messageSent = false;

          if(dsc.armed[partition]) 
          {
            if (dsc.armedAway[partition] && dsc.noEntryDelay[partition])
            {
              messageSent = publishMQTTMessage(publishTopic, MQTTPubPayloadArmNight, MQTTRetain);
            }
            else if (dsc.armedAway[partition])
            {
              messageSent = publishMQTTMessage(publishTopic, MQTTPubPayloadArm, MQTTRetain);
            }
            else if(dsc.armedStay[partition] && dsc.noEntryDelay[partition]) 
            {
              messageSent = publishMQTTMessage(publishTopic, MQTTPubPayloadArmNight, MQTTRetain);
            }
            else if(dsc.armedStay[partition])
            {
              messageSent = publishMQTTMessage(publishTopic, MQTTPubPayloadArmStay, MQTTRetain);
            }
            else
            {
              messageSent = true;
            }
          }
          else
          {
            messageSent = publishMQTTMessage(publishTopic, MQTTPubPayloadDisarm, MQTTRetain);
          }

          if(messageSent)
          {
            dsc.armedChanged[partition] = false;  // Resets the partition armed status flag
          }
        }

        // Publishes exit delay status
        if(dsc.exitDelayChanged[partition]) 
        {
          // Appends the mqttPartitionTopic with the partition number
          char publishTopic[strlen(MQTTPartitionTopic) + MQTTPubParitionNumberlen + NULLTERM_LEN];
          appendPartition(MQTTPartitionTopic, partition, publishTopic);  // Appends the mqttPartitionTopic with the partition number

          bool messageSent = false;
        
          if(dsc.exitDelay[partition]) 
          {
            messageSent = publishMQTTMessage(publishTopic, MQTTPubPayloadPending, MQTTRetain);  // Publish as a retained message
          }
          else if((false == dsc.exitDelay[partition]) && (false == dsc.armed[partition])) 
          {
            messageSent = publishMQTTMessage(publishTopic, MQTTPubPayloadDisarm, MQTTRetain);
          }
          else
          {
            messageSent = true;
          }

          if(messageSent)
          {
            dsc.exitDelayChanged[partition] = false;  // Resets the exit delay status flag
          }
        }

        // Publishes alarm status
        if(dsc.alarmChanged[partition]) 
        {
          // Appends the mqttPartitionTopic with the partition number
          char publishTopic[strlen(MQTTPartitionTopic) + MQTTPubParitionNumberlen + NULLTERM_LEN];
          appendPartition(MQTTPartitionTopic, partition, publishTopic);  // Appends the mqttPartitionTopic with the partition number

          bool messageSent = false;
          
          if(dsc.alarm[partition]) 
          {
            messageSent = publishMQTTMessage(publishTopic, MQTTPubPayloadAlarmTrigger, MQTTRetain);  // Alarm tripped
          }
          else if(false == dsc.armedChanged[partition])
          {
            messageSent = publishMQTTMessage(publishTopic, MQTTPubPayloadDisarm, MQTTRetain);
          }
          else
          {
            messageSent = true;
          }

          if(messageSent)
          {
            dsc.alarmChanged[partition] = false;  // Resets the partition alarm status flag
          }
        }

        if (dsc.armedChanged[partition]) 
        {
          dsc.armedChanged[partition] = false;  // Resets the partition armed status flag
        }

        // Publishes fire alarm status
        if(dsc.fireChanged[partition]) 
        {
          // Appends the mqttFireTopic with the partition number
          char firePublishTopic[strlen(MQTTFireTopic) + MQTTPubParitionNumberlen + NULLTERM_LEN];
          appendPartition(MQTTFireTopic, partition, firePublishTopic);  // Appends the mqttFireTopic with the partition number

          bool messageSent = false;

          if(dsc.fire[partition]) 
          {
            messageSent = publishMQTTMessage(firePublishTopic, MQTTPubPayloadFireTrigger, MQTTNotRetain);  // Fire alarm tripped
          }
          else 
          {
            messageSent = publishMQTTMessage(firePublishTopic, MQTTPubPayloadFireIdle, MQTTNotRetain);  // Fire alarm restored
          }

          if(messageSent)
          {
            dsc.fireChanged[partition] = false;  // Resets the fire status flag
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
        bool allZoneReported = true;
        
        for(byte zoneGroup = 0; zoneGroup < dscZones; zoneGroup++) 
        {
          for(byte zoneBit = 0; zoneBit < 8; zoneBit++) 
          {
            if(bitRead(dsc.openZonesChanged[zoneGroup], zoneBit))   // Checks an individual open zone status flag
            {
              // Appends the mqttZoneTopic with the zone number
              char zonePublishTopic[strlen(MQTTZoneTopic) + 3];
              char zone[3];
              strcpy(zonePublishTopic, MQTTZoneTopic);
              itoa(zoneBit + 1 + (zoneGroup * 8), zone, 10);
              strcat(zonePublishTopic, zone);

              bool messageSent = false;

              if(bitRead(dsc.openZones[zoneGroup], zoneBit)) 
              {
                messageSent = publishMQTTMessage(zonePublishTopic, MQTTPubPayloadZoneTrigger, MQTTRetain); // Zone open
              }
              else 
              {
                messageSent = publishMQTTMessage(zonePublishTopic, MQTTPubPayloadZoneIdle, MQTTRetain); // Zone closed
              }

              allZoneReported &= messageSent;

              if(messageSent)
              {
                bitClear(dsc.openZonesChanged[zoneGroup], zoneBit);  // Resets the individual open zone status flag
              }
            }
          }
        }

        if(allZoneReported)
        {
          dsc.openZonesStatusChanged = false;                           // Resets the open zones status flag
        }
      }

      // Publishes PGM outputs 1-14 status in a separate topic per zone
      // PGM status is stored in the pgmOutputs[] and pgmOutputsChanged[] arrays using 1 bit per PGM output:
      //   pgmOutputs[0] and pgmOutputsChanged[0]: Bit 0 = PGM 1 ... Bit 7 = PGM 8
      //   pgmOutputs[1] and pgmOutputsChanged[1]: Bit 0 = PGM 9 ... Bit 5 = PGM 14
      if (dsc.pgmOutputsStatusChanged) 
      {
        bool allPGMReported = true;

        for (byte pgmGroup = 0; pgmGroup < 2; pgmGroup++) 
        {
          for (byte pgmBit = 0; pgmBit < 8; pgmBit++) 
          {
            if (bitRead(dsc.pgmOutputsChanged[pgmGroup], pgmBit))   // Checks an individual PGM output status flag
            {
              // Appends the mqttPgmTopic with the PGM number
              char pgmPublishTopic[strlen(MQTTPGMTopic) + 3];
              char pgm[3];
              strcpy(pgmPublishTopic, MQTTPGMTopic);
              itoa(pgmBit + 1 + (pgmGroup * 8), pgm, 10);
              strcat(pgmPublishTopic, pgm);

              bool messageSent = false;

              if (bitRead(dsc.pgmOutputs[pgmGroup], pgmBit)) 
              {
                messageSent = publishMQTTMessage(pgmPublishTopic, MQTTPubPayloadZoneTrigger, MQTTRetain); // PGM enabled
              }
              else 
              {
                messageSent = publishMQTTMessage(pgmPublishTopic, MQTTPubPayloadZoneIdle, MQTTRetain); // PGM disabled
              }

              allPGMReported &= messageSent;

              if(messageSent)
              {
                bitClear(dsc.pgmOutputsChanged[pgmGroup], pgmBit);  // Resets the individual PGM output status flag
              }
            }
          }
        }

        if(allPGMReported)
        {
          dsc.pgmOutputsStatusChanged = false;  // Resets the PGM outputs status flag
        }
      }
    }
  }

  advanceTimers();
  Ethernet.maintain();
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

  Serial.print(millis());
  Serial.print(F(": MQTT in : "));
  Serial.print(topic);
  Serial.print(F(" "));
  Serial.println(szTemp);


  byte partition = DefaultPartitionId;
  byte payloadIndex = 0;

  // Checks if a partition number 1-8 has been sent and sets the second character as the payload
  if(('1' <= payload[0]) && ('8' >= payload[0])) 
  {
    partition = payload[0] - '1';
    payloadIndex = 1;
  }

  // Panic alarm
  if('P' == payload[payloadIndex]) 
  {
    dsc.write('p');
  }

  // Resets status if attempting to change the armed mode while armed or not ready
  if((MQTTSubPayloadDisarmSuffix != payload[payloadIndex]) && (false == dsc.ready[partition])) 
  {
    dsc.armedChanged[partition] = true;
    dsc.statusChanged = true;
    return;
  }

  // Arm stay
  if((MQTTSubPayloadArmStaySuffix == payload[payloadIndex]) && (false == dsc.armed[partition]) && (false == dsc.exitDelay[partition])) 
  {
    dsc.writePartition = partition + 1;         // Sets writes to the partition number
    dsc.write('s');                             // Virtual keypad arm stay
  }

  // Arm away
  else if((MQTTSubPayloadArmSuffix == payload[payloadIndex]) && (false == dsc.armed[partition]) && (false == dsc.exitDelay[partition])) 
  {
    dsc.writePartition = partition + 1;         // Sets writes to the partition number
    dsc.write('w');                             // Virtual keypad arm away
  }

  // Disarm
  else if(MQTTSubPayloadDisarmSuffix == payload[payloadIndex] && (dsc.exitDelay[partition] || dsc.entryDelay[partition] || dsc.armed[partition])) 
  {
    dsc.writePartition = partition + 1;         // Sets writes to the partition number
    dsc.write(accessCode);
  }

  // Arm night
  else if((MQTTSubPayloadNightSuffix == payload[payloadIndex]) && (false == dsc.armed[partition]) && (false == dsc.exitDelay[partition]))
  {
    dsc.writePartition = partition + 1;         // Sets writes to the partition number
    dsc.write('n');                             // Virtual keypad arm away
  }

  // Silence trouble
  else if((MQTTSubPayloadSilenceSuffix == payload[payloadIndex]) && (false == dsc.armed[partition]) && (false == dsc.exitDelay[partition])) 
  {
    dsc.writePartition = partition + 1;         // Sets writes to the partition number
    dsc.write('#');
  }
}


boolean mqttHandle (void) 
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
        Ethernet.begin(mac, ip, MQTTBrokerIP, gateway, subnet);
      } 
      else 
      {
        Serial.println(F("MQTT connected."));
        mqtt.subscribe(MQTTSubscribeTopic); 
        mqttActionTimer = 0;
      }
    }
  }
  
  return mqtt.loop();
}

// Publish MQTT data to MQTT broker
static bool publishMQTTMessage (char const * const sMQTTSubscription, char const * const sMQTTData, bool retain)
{
  // Define and send message about door state
  bool result = mqtt.publish(sMQTTSubscription, sMQTTData, retain); 

  // Debug info
  Serial.print(millis());
  Serial.print(F(": MQTT Out : "));
  Serial.print(sMQTTSubscription);
  Serial.print(F(" "));
  Serial.println(sMQTTData);

  return result;
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

static void appendPartition(const char* sourceTopic, byte sourceNumber, char* publishTopic) 
{
  char partitionNumber[2];
  strcpy(publishTopic, sourceTopic);
  itoa(sourceNumber + 1, partitionNumber, 10);
  strcat(publishTopic, partitionNumber);
}
