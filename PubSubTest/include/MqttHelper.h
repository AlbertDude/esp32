//-----------------------------------------------------------------
// MQTT Helper
//
// Currently only supports single instance per app
// - though some of the infrastructure is in place to support multiple instances
//  - multiple instances might be useful for connection to multiple MQTT brokers


template <size_t MAX_SUBSCRIPTIONS=3>
class MqttHelper
{
public:

    // call from setup()
    void Setup(WiFiClient & wifi_client, const char* mqtt_server_addr, const char* name="MqttHelper", int mqtt_server_port=1883)
    {
        SerialLog::Log("MqttHelper<" + String(MAX_SUBSCRIPTIONS) + ">: " + name + " " + mqtt_server_addr + ":" + String(mqtt_server_port));

        strncpy(name_, name, kMaxNameLen);
        pubsubclient_.setClient(wifi_client);
        pubsubclient_.setServer(mqtt_server_addr, mqtt_server_port);
        pubsubclient_.setCallback(Callback);

        // Initial mqtt connection
        ReconnectBlocking();
    }

    // call from loop()
    void Loop()
    {
        if (!pubsubclient_.connected()) 
        {
            static long prev_attempt = 0;
            long now = millis();
            if (now - prev_attempt > kReconnectAttemptInterval) 
            {
                // Attempt to reconnect
                ReconnectNonBlocking();
                prev_attempt = now;
            }
        } 
        else 
        {
            pubsubclient_.loop();
        }
    }

    typedef std::function<void(String payload)> TopicHandler;

    void Subscribe(const char* topic, TopicHandler topic_handler)
    {
        assert( strlen(topic) < kMaxTopicLen );
        assert( num_subscriptions_ < MAX_SUBSCRIPTIONS );

        strncpy(subscriptions_[num_subscriptions_].topic, topic, kMaxTopicLen);
        subscriptions_[num_subscriptions_].topic_handler = topic_handler;
        num_subscriptions_++;
        RenewSubscriptions();
        RegisterInstance(topic, this);
    }

    bool Publish(const char* topic, const char* payload)
    {
        return pubsubclient_.publish(topic, payload);
    }

private:
    PubSubClient pubsubclient_;
    const long kReconnectAttemptInterval = 5000; // Try reconnections every 5 s

    static const size_t kMaxNameLen = 32;
    char name_[kMaxNameLen];

    static const size_t kMaxTopicLen = 128;
    struct Subscription
    {
        char            topic[kMaxTopicLen];
        TopicHandler    topic_handler;
    };
    Subscription subscriptions_[MAX_SUBSCRIPTIONS];
    size_t  num_subscriptions_ = 0;

    // Hack: list of registered instances
    // - currently just supports a single instance
    // - idea for extending to multiple instances
    //  - associate instances with topics --> use topic to look up proper instance
    static MqttHelper * instance_;
    static void RegisterInstance(const char* topic, MqttHelper * instance)
    {
        // TODO: to support multiple instances, would store instance corresponding to topic
        instance_ = instance;
    }
    static MqttHelper * GetInstance(const char* topic)
    {
        // TODO: to support multiple instances, would lookup instance corresponding to topic
        assert( instance_ != nullptr );
        return instance_;
    }

    // The callback function has the following signature:
    //   void callback(const char[] topic, byte* payload, unsigned int length)
    // See: https://pubsubclient.knolleary.net/api#callback
    //
    // So there isn't a great way to provide instance-specific callbacks
    // - the thought is that this instance discrimination can be done via the topic
    static void Callback(const char* topic, byte* message_bytes, unsigned int length) 
    {
        SerialLog::Log("Message arrived on topic: " + String(topic));

        String message;
        for (int i = 0; i < length; i++) 
        {
            message += (char)message_bytes[i];
        }
        SerialLog::Log("Message: " + message);

        MqttHelper * instance = GetInstance(topic);
        if (instance)
        {
            for( int i=0; i<instance->num_subscriptions_; i++)
            {
                if (String(topic) == instance->subscriptions_[i].topic) 
                {
                    instance->subscriptions_[i].topic_handler(message);
                }
            }
        }
    }

    bool RenewSubscriptions()
    {
        bool success = true;
        for( int i=0; i<num_subscriptions_; i++)
        {
            success &= pubsubclient_.subscribe(subscriptions_[i].topic);
        }

        return success;
    }

    boolean ReconnectNonBlocking() 
    {
        bool connected = pubsubclient_.connect(name_);
        if (connected)
        {
            SerialLog::Log("MQTT connected");
            // subscriptions
            SerialLog::Log("MQTT subscribed topics: ");
            for( int i=0; i<num_subscriptions_; i++)
            {
                SerialLog::Log("  " + String(subscriptions_[i].topic));
            }
            RenewSubscriptions();
        }
        return connected;
    }

    void ReconnectBlocking() 
    {
        // Loop until we're reconnected
        while (!pubsubclient_.connected()) 
        {
            SerialLog::Log("Attempting MQTT connection...");
            // Attempt to connect
            if (!ReconnectNonBlocking())
            {
                SerialLog::Log("Failed, rc=" + String(pubsubclient_.state()));
                SerialLog::Log("Retry in 5 seconds");
                delay(kReconnectAttemptInterval); // Wait before retrying
            }
        }
    }

};

template <size_t MAX_SUBSCRIPTIONS>
MqttHelper<MAX_SUBSCRIPTIONS> * MqttHelper<MAX_SUBSCRIPTIONS>::instance_ = nullptr;

