#pragma once
#include <EspMqttBase.h>

class MqttLight : public EspMqttBase {
public:
    void setup(const char* deviceId);
    void publishState();

protected:
    void onConnected() override;
    void onMessage(const char* topic, const char* payload) override;

private:
    void publishDiscovery();

    String _topicState;
    String _topicCommand;
    String _topicDiscovery;
};

extern MqttLight Mqtt;
