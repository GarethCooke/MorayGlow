#include <unity.h>
#include "state.h"

// Required by Unity — called before/after each test
void setUp() {}
void tearDown() {}

// ---- stateToJson ----

void test_stateToJson_on() {
    String result = stateToJson(true, "#ff0000");
    TEST_ASSERT_EQUAL_STRING("{\"on\":true,\"color\":\"#ff0000\"}", result.c_str());
}

void test_stateToJson_off() {
    String result = stateToJson(false, "#ffffff");
    TEST_ASSERT_EQUAL_STRING("{\"on\":false,\"color\":\"#ffffff\"}", result.c_str());
}

// ---- rgbToHex ----

void test_rgbToHex_red() {
    TEST_ASSERT_EQUAL_STRING("#ff0000", rgbToHex(255, 0, 0).c_str());
}

void test_rgbToHex_green() {
    TEST_ASSERT_EQUAL_STRING("#00ff00", rgbToHex(0, 255, 0).c_str());
}

void test_rgbToHex_blue() {
    TEST_ASSERT_EQUAL_STRING("#0000ff", rgbToHex(0, 0, 255).c_str());
}

void test_rgbToHex_black() {
    TEST_ASSERT_EQUAL_STRING("#000000", rgbToHex(0, 0, 0).c_str());
}

void test_rgbToHex_white() {
    TEST_ASSERT_EQUAL_STRING("#ffffff", rgbToHex(255, 255, 255).c_str());
}

// ---- parseMqttCommand ----

void test_parseMqttCommand_on() {
    bool   on    = false;
    String color = "#000000";
    bool   ok    = parseMqttCommand("{\"state\":\"ON\"}", on, color);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_TRUE(on);
    TEST_ASSERT_EQUAL_STRING("#000000", color.c_str()); // unchanged
}

void test_parseMqttCommand_off() {
    bool   on    = true;
    String color = "#ffffff";
    bool   ok    = parseMqttCommand("{\"state\":\"OFF\"}", on, color);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_FALSE(on);
}

void test_parseMqttCommand_color() {
    bool   on    = true;
    String color = "#000000";
    bool   ok    = parseMqttCommand("{\"color\":{\"r\":255,\"g\":128,\"b\":0}}", on, color);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_STRING("#ff8000", color.c_str());
}

void test_parseMqttCommand_malformed() {
    bool   on    = false;
    String color = "#ffffff";
    bool   ok    = parseMqttCommand("not json {{", on, color);
    TEST_ASSERT_FALSE(ok);
}

// ---- Entry point ----

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_stateToJson_on);
    RUN_TEST(test_stateToJson_off);

    RUN_TEST(test_rgbToHex_red);
    RUN_TEST(test_rgbToHex_green);
    RUN_TEST(test_rgbToHex_blue);
    RUN_TEST(test_rgbToHex_black);
    RUN_TEST(test_rgbToHex_white);

    RUN_TEST(test_parseMqttCommand_on);
    RUN_TEST(test_parseMqttCommand_off);
    RUN_TEST(test_parseMqttCommand_color);
    RUN_TEST(test_parseMqttCommand_malformed);

    return UNITY_END();
}
