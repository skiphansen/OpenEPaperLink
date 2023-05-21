#include <Arduino.h>
#include <WiFi.h>

#include "AsyncUDP.h"
#include "commstructs.h"
#include "newproto.h"
#include "tag_db.h"
#include "web.h"
#include "serialap.h"
#include "udp.h"

#define UDPIP IPAddress(239, 10, 0, 1)
#define UDPPORT 16033

UDPcomm udpsync;

extern uint8_t channelList[6];
extern espSetChannelPower curChannel;

void init_udp() {
    udpsync.init();
}

UDPcomm::UDPcomm() {
    // Constructor
}

UDPcomm::~UDPcomm() {
    // Destructor
}

void UDPcomm::init() {
    if (udp.listenMulticast(UDPIP, UDPPORT)) {
        udp.onPacket([this](AsyncUDPPacket packet) {
            if (packet.remoteIP() != WiFi.localIP()) {
                this->processPacket(packet);
            }
        });
    }
    setAPchannel();
}

void UDPcomm::processPacket(AsyncUDPPacket packet) {
    switch (packet.data()[0]) {
        case PKT_AVAIL_DATA_INFO: {
            espAvailDataReq* adr = (espAvailDataReq*)&packet.data()[1];
            processDataReq(adr, false);
            break;
        }
        case PKT_XFER_COMPLETE: {
            espXferComplete* xfc = (espXferComplete*)&packet.data()[1];
            processXferComplete(xfc, false);
            break;
        }
        case PKT_XFER_TIMEOUT: {
            espXferComplete* xfc = (espXferComplete*)&packet.data()[1];
            processXferTimeout(xfc, false);
            break;
        }
        case PKT_AVAIL_DATA_REQ: {
            pendingData* pending = (pendingData*)&packet.data()[1];
            prepareExternalDataAvail(pending, packet.remoteIP());
            break;
        }
        case PKT_APLIST_REQ: {
            IPAddress senderIP = packet.remoteIP();

            APlist APitem;
            APitem.src = WiFi.localIP();
            strcpy(APitem.alias, APconfig["alias"]);
            APitem.channelId = curChannel.channel;
            APitem.tagCount = getTagCount();
            APitem.version = apInfo.version;

            uint8_t buffer[sizeof(struct APlist) + 1];
            buffer[0] = PKT_APLIST_REPLY;
            memcpy(buffer + 1, &APitem, sizeof(struct APlist));
            udp.writeTo(buffer, sizeof(buffer), senderIP, UDPPORT);
            break;
        }
        case PKT_APLIST_REPLY: {
            APlist* APreply = (APlist*)&packet.data()[1];
            //remove active channel from list
            for (int i = 0; i < 6; i++) {
                if (channelList[i] == APreply->channelId) channelList[i] = 0;
            }
            wsSendAPitem(APreply);
            break;
        }
    }
}

void autoselect(void* pvParameters) {
    // reset channel list
    uint8_t values[] = {11, 15, 20, 25, 26, 27};
    memcpy(channelList, values, sizeof(values));
    // wait 5s for channelList to collect all AP's
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    curChannel.channel = 0;
    for (int i = 0; i < 6; i++) {
        if (channelList[i] > 0) {
            curChannel.channel = channelList[i];
            break;
        }
    }
    if (curChannel.channel == 0) {
        curChannel.channel = 11;
    } 
    APconfig["channel"] = curChannel.channel;
    do {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    } while (!apInfo.isOnline);

    sendChannelPower(&curChannel);
    saveAPconfig();

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    vTaskDelete(NULL);
}

void UDPcomm::getAPList() {
    APlist APitem;
    APitem.src = WiFi.localIP();
    strcpy(APitem.alias, APconfig["alias"]);
    APitem.channelId = curChannel.channel;
    APitem.tagCount = getTagCount();
    APitem.version = apInfo.version;
    wsSendAPitem(&APitem);

    if (APconfig["channel"].as<int>() == 0) {
        xTaskCreate(autoselect, "autoselect", 5000, NULL, configMAX_PRIORITIES - 10, NULL);
    }

    uint8_t buffer[sizeof(struct APlist) + 1];
    buffer[0] = PKT_APLIST_REQ;
    memcpy(buffer + 1, &APitem, sizeof(struct APlist));
    udp.writeTo(buffer, sizeof(buffer), UDPIP, UDPPORT);
}

void UDPcomm::netProcessDataReq(struct espAvailDataReq* eadr) {
    uint8_t buffer[sizeof(struct espAvailDataReq) + 1];
    buffer[0] = PKT_AVAIL_DATA_INFO;
    memcpy(buffer + 1, eadr, sizeof(struct espAvailDataReq));
    udp.writeTo(buffer, sizeof(buffer), UDPIP, UDPPORT);
}

void UDPcomm::netProcessXferComplete(struct espXferComplete* xfc) {
    uint8_t buffer[sizeof(struct espXferComplete) + 1];
    buffer[0] = PKT_XFER_COMPLETE;
    memcpy(buffer + 1, xfc, sizeof(struct espXferComplete));
    udp.writeTo(buffer, sizeof(buffer), UDPIP, UDPPORT);
}

void UDPcomm::netProcessXferTimeout(struct espXferComplete* xfc) {
    uint8_t buffer[sizeof(struct espXferComplete) + 1];
    buffer[0] = PKT_XFER_TIMEOUT;
    memcpy(buffer + 1, xfc, sizeof(struct espXferComplete));
    udp.writeTo(buffer, sizeof(buffer), UDPIP, UDPPORT);
}

void UDPcomm::netSendDataAvail(struct pendingData* pending) {
    uint8_t buffer[sizeof(struct pendingData) + 1];
    buffer[0] = PKT_AVAIL_DATA_REQ;
    memcpy(buffer + 1, pending, sizeof(struct pendingData));
    udp.writeTo(buffer, sizeof(buffer), UDPIP, UDPPORT);
}