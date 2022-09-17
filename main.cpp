/**
 * Copyright (c) 2017, Arm Limited and affiliates.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>

#include "lorawan/LoRaWANInterface.h"
#include "lorawan/system/lorawan_data_structures.h"
#include "events/EventQueue.h"

// Application helpers
#include "trace_helper.h"
#include "lora_radio_helper.h"
#include "lora_phy_helper.h"

using namespace events;

// Max payload size can be LORAMAC_PHY_MAXPAYLOAD.
// This example only communicates with much shorter messages (<30 bytes).
// If longer messages are used, these buffers must be changed accordingly.
uint8_t tx_buffer[7];
uint8_t rx_buffer[7];


//bool swapper = 0;
/*
 * Sets up an application dependent transmission timer in ms. Used only when Duty Cycling is off for testing
 */
#define TX_TIMER                        30000

/**
 * Maximum number of events for the event queue.
 * 10 is the safe number for the stack events, however, if application
 * also uses the queue for whatever purposes, this number should be increased.
 */
#define MAX_NUMBER_OF_EVENTS            10

/**
 * Maximum number of retries for CONFIRMED messages before giving up
 */
#define CONFIRMED_MSG_RETRY_COUNTER     3

/**
 * Datarate used when ADR is disabled
 */
#define DATA_RATE      DR_0

/**
 * LoRaWAN Regions from https://github.com/ARMmbed/mbed-os/blob/a6610e61691c23b02dd8513c9194acc316690327/connectivity/lorawan/lorastack/phy/loraphy_target.h#L23
 */


/**
* This event queue is the global event queue for both the
* application and stack. To conserve memory, the stack is designed to run
* in the same thread as the application and the application is responsible for
* providing an event queue to the stack that will be used for ISR deferment as
* well as application information event queuing.
*/
static EventQueue ev_queue(MAX_NUMBER_OF_EVENTS *EVENTS_EVENT_SIZE);

/**
 * Event handler.
 *
 * This will be passed to the LoRaWAN stack to queue events for the
 * application which in turn drive the application.
 */
static void lora_event_handler(lorawan_event_t event);

/**
 * Constructing Mbed LoRaWANInterface and passing it the radio object from lora_radio_helper.
 */
static LoRaWANInterface lorawan(radio, phy);

/**
 * Application specific callbacks
 */
static lorawan_app_callbacks_t callbacks;

/**
 * Entry point for application
 */
int main(void)
{
    // setup tracing
    setup_trace();

    // stores the status of a call to LoRaWAN protocol
    lorawan_status_t retcode;

    // Initialize LoRaWAN stack
    if (lorawan.initialize(&ev_queue) != LORAWAN_STATUS_OK) {
        printf("\n LoRa initialization failed! \n");
        return -1;
    }

    printf("\n Mbed LoRaWANStack initialized \n");

    // prepare application callbacks
    callbacks.events = mbed::callback(lora_event_handler);
    lorawan.add_app_callbacks(&callbacks);

    // Set number of retries in case of CONFIRMED messages
    if (lorawan.set_confirmed_msg_retries(CONFIRMED_MSG_RETRY_COUNTER)
            != LORAWAN_STATUS_OK) {
        printf("\n set_confirmed_msg_retries failed! \n\n");
        return -1;
    }

    printf("\n CONFIRMED message retries : %d \n",
           CONFIRMED_MSG_RETRY_COUNTER);

    // Enable adaptive data rate
    if (lorawan.disable_adaptive_datarate() != LORAWAN_STATUS_OK) {
        printf("\n disable_adaptive_datarate failed! \n");
        return -1;
    }

    printf("\n Adaptive data  rate (ADR) - Disabled \n");

    retcode = lorawan.connect();

    if (retcode == LORAWAN_STATUS_OK ||
            retcode == LORAWAN_STATUS_CONNECT_IN_PROGRESS) {
    } else {
        printf("\n Connection error, code = %d \n", retcode);
        return -1;
    }

    printf("\n Connection - In Progress ...\n");


    // make your event queue dispatching events forever
    ev_queue.dispatch_forever();

    return 0;
}

/**
 * Sends a message to the Network Server and ask for ACK
 */
static void send_message()
{
    uint16_t packet_len;
    int16_t retcode;

    packet_len = sprintf((char *) tx_buffer, "0");

    retcode = lorawan.send(MBED_CONF_LORA_APP_PORT, tx_buffer, packet_len,
                           MSG_UNCONFIRMED_FLAG);

    if (retcode < 0) {
        retcode == LORAWAN_STATUS_WOULD_BLOCK ? printf("send - WOULD BLOCK\n")
        : printf("\n send() - Error code %d \n", retcode);

        if (retcode == LORAWAN_STATUS_WOULD_BLOCK) {
            //retry in 3 seconds
            if (MBED_CONF_LORA_DUTY_CYCLE_ON) {
                ev_queue.call_in(3000, send_message);
            }
        }
        return;
    }

    printf("\n %d bytes scheduled for transmission \n", retcode);
    memset(tx_buffer, 0, sizeof(tx_buffer));
}

/**
 * Receive a message from the Network Server
 */
static void receive_message()
{
    uint8_t port;
    int flags;
    int16_t retcode = lorawan.receive(rx_buffer, sizeof(rx_buffer), port, flags);

    if (retcode < 0) {
        printf("\n receive() - Error code %d \n", retcode);
        return;
    }

    printf(" RX Data on port %u (%d bytes): ", port, retcode);
    for (uint8_t i = 0; i < retcode; i++) {
        printf("%02x ", rx_buffer[i]);
    }
    printf("\n");
    
    memset(rx_buffer, 0, sizeof(rx_buffer));
}

/**
 * Event handler
 */
static void lora_event_handler(lorawan_event_t event)
{
    lorawan_tx_metadata txMetadata;
    lorawan_status_t retcode_tx_data;
    switch (event) {
        case CONNECTED:
            printf("\n Connection - Successful \n");
            if (MBED_CONF_LORA_DUTY_CYCLE_ON) {
                send_message();
            } else {
                ev_queue.call_every(TX_TIMER, send_message);
            }

            // Set data rate as it will have been changed after join to use whichever was successful at join
            if (lorawan.set_datarate(DATA_RATE) != LORAWAN_STATUS_OK) {
                printf("\n set_datarate failed! \n");
            } else {
                printf("\n Datarate set successfully \n");
            }
    
            break;

        case DISCONNECTED:
            ev_queue.break_dispatch();
            printf("\n Disconnected Successfully \n");
            break;

        case TX_DONE:
            printf("\n Message Sent to Network Server \n");

            retcode_tx_data = lorawan.get_tx_metadata(txMetadata);
            if (retcode_tx_data == LORAWAN_STATUS_OK) {
                printf("\n TX Time-on-air: %u \n Channel: %u \n TX Power: %u \n Data Rate: %u \n Number of retransmissions: %u \n Stale: %u\n",
                     txMetadata.tx_toa, txMetadata.channel, txMetadata.tx_power, txMetadata.data_rate, txMetadata.nb_retries, txMetadata.stale);
            } else {
                printf("Error getting TX metadata %d\r\n", retcode_tx_data);
            }
            if (MBED_CONF_LORA_DUTY_CYCLE_ON) {
                send_message();
            }

            // Set data rate as it will have been changed after join to use whichever was successful at join
            //if (swapper) {
            //    swapper = 0;
            //    if (lorawan.set_datarate(DATA_RATE) != LORAWAN_STATUS_OK) {
            //        printf("\n set_datarate failed! \n");
            //    } else {
            //        printf("\n Datarate set to DR 0 successfully \n");
            //    }
            //} else {
            //    swapper = 1;
            //    if (lorawan.set_datarate(DR_3) != LORAWAN_STATUS_OK) {
            //        printf("\n set_datarate failed! \n");
            //    } else {
            //        printf("\n Datarate set to DR3 successfully \n");
            //    }
            //}
            

            break;

        case TX_TIMEOUT:
        case TX_ERROR:
            printf("\n TX ERROR - EventCode = %d \n", event);
            break;
        case TX_CRYPTO_ERROR:
        case TX_SCHEDULING_ERROR:
            printf("\n Transmission Error - EventCode = %d \n", event);
            // try again
            if (MBED_CONF_LORA_DUTY_CYCLE_ON) {
                send_message();
            }
            break;

        case RX_DONE:
            printf("\n Received message from Network Server \n");
            receive_message();
            break;

        case RX_TIMEOUT:
        case RX_ERROR:
            printf("\n Error in reception - Code = %d \n", event);
            break;

        case JOIN_FAILURE:
            printf("\n OTAA Failed - Check Keys \n");
            break;

        case UPLINK_REQUIRED:
            printf("\n Uplink required by NS \n");
            if (MBED_CONF_LORA_DUTY_CYCLE_ON) {
                send_message();
            }
            break;

        default:
            MBED_ASSERT("Unknown Event");
    }
}

// EOF
