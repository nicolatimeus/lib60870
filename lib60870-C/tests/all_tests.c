#include "unity.h"
#include "iec60870_common.h"
#include "cs104_slave.h"
#include "cs104_connection.h"
#include "hal_time.h"
#include "hal_thread.h"
#include "buffer_frame.h"
#include <string.h>
#include <stdlib.h>

void setUp(void) { }
void tearDown(void) {}

static struct sCS101_AppLayerParameters defaultAppLayerParameters = {
    /* .sizeOfTypeId =  */ 1,
    /* .sizeOfVSQ = */ 1,
    /* .sizeOfCOT = */ 2,
    /* .originatorAddress = */ 0,
    /* .sizeOfCA = */ 2,
    /* .sizeOfIOA = */ 3,
    /* .maxSizeOfASDU = */ 249
};

typedef struct sCS104_IPAddress* CS104_IPAddress;

struct sCS104_IPAddress
{
    uint8_t address[16];
    eCS104_IPAddressType type;
};

static void
CS104_IPAddress_setFromString(CS104_IPAddress self, const char* ipAddrStr)
{
    if (strchr(ipAddrStr, '.') != NULL) {
        /* parse IPv4 string */
        self->type = IP_ADDRESS_TYPE_IPV4;

        int i;

        for (i = 0; i < 4; i++) {
            self->address[i] = strtoul(ipAddrStr, NULL, 10);

            ipAddrStr = strchr(ipAddrStr, '.');

            if ((ipAddrStr == NULL) || (*ipAddrStr == 0))
                break;

            ipAddrStr++;
        }
    }
    else {
        self->type = IP_ADDRESS_TYPE_IPV6;

        int i;

        for (i = 0; i < 8; i++) {
            uint32_t val = strtoul(ipAddrStr, NULL, 16);

            self->address[i * 2] = val / 0x100;
            self->address[i * 2 + 1] = val % 0x100;

            ipAddrStr = strchr(ipAddrStr, ':');

            if ((ipAddrStr == NULL) || (*ipAddrStr == 0))
                break;

            ipAddrStr++;
        }
    }
}

static bool
CS104_IPAddress_equals(CS104_IPAddress self, CS104_IPAddress other)
{
    if (self->type != other->type)
        return false;

    int size;

    if (self->type == IP_ADDRESS_TYPE_IPV4)
        size = 4;
    else
        size = 16;

    int i;

    for (i = 0; i < size; i++) {
        if (self->address[i] != other->address[i])
            return false;
    }

    return true;
}

void
CS101_ASDU_encode(CS101_ASDU self, Frame frame);

CS101_ASDU
CS101_ASDU_createFromBuffer(CS101_AppLayerParameters parameters, uint8_t* msg, int msgLength);

void
test_CP56Time2a(void)
{
    struct sCP56Time2a currentTime;

    uint64_t currentTimestamp = Hal_getTimeInMs();

    CP56Time2a_createFromMsTimestamp(&currentTime, currentTimestamp);

    uint64_t convertedTimestamp = CP56Time2a_toMsTimestamp(&currentTime);

    TEST_ASSERT_EQUAL_UINT64(currentTimestamp, convertedTimestamp);
}

void
test_CP56Time2aToMsTimestamp(void)
{
    struct sCP56Time2a timeval;

    timeval.encodedValue[0] = 0x85;
    timeval.encodedValue[1] = 0x49;
    timeval.encodedValue[2] = 0x0c;
    timeval.encodedValue[3] = 0x09;
    timeval.encodedValue[4] = 0x55;
    timeval.encodedValue[5] = 0x03;
    timeval.encodedValue[6] = 0x11;

    uint64_t convertedTimeval = CP56Time2a_toMsTimestamp(&timeval);

    TEST_ASSERT_EQUAL_UINT64((uint64_t) 1490087538821, convertedTimeval);
}

void
test_StepPositionInformation(void)
{
    StepPositionInformation spi;

    spi = StepPositionInformation_create(NULL, 101, 0, true, IEC60870_QUALITY_GOOD);

    TEST_ASSERT_EQUAL_INT(0, StepPositionInformation_getValue(spi));
    TEST_ASSERT_TRUE(StepPositionInformation_isTransient(spi));

    StepPositionInformation_create(spi, 101, 63, false, IEC60870_QUALITY_GOOD);

    TEST_ASSERT_EQUAL_INT(63, StepPositionInformation_getValue(spi));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient(spi));

    StepPositionInformation_create(spi, 101, -64, false, IEC60870_QUALITY_GOOD);

    TEST_ASSERT_EQUAL_INT(-64, StepPositionInformation_getValue(spi));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient(spi));

    StepPositionInformation_create(spi, 101, 64, false, IEC60870_QUALITY_GOOD);

    TEST_ASSERT_EQUAL_INT(63, StepPositionInformation_getValue(spi));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient(spi));

    StepPositionInformation_create(spi, 101, -65, false, IEC60870_QUALITY_GOOD);

    TEST_ASSERT_EQUAL_INT(-64, StepPositionInformation_getValue(spi));
    TEST_ASSERT_FALSE(StepPositionInformation_isTransient(spi));

    StepPositionInformation_destroy(spi);
}

void
test_addMaxNumberOfIOsToASDU(void)
{
    struct sCS101_AppLayerParameters salParameters;

    salParameters.maxSizeOfASDU = 100;
    salParameters.originatorAddress = 0;
    salParameters.sizeOfCA = 2;
    salParameters.sizeOfCOT = 2;
    salParameters.sizeOfIOA = 3;
    salParameters.sizeOfTypeId = 1;
    salParameters.sizeOfVSQ = 1;

    CS101_ASDU asdu = CS101_ASDU_create(&salParameters, false, CS101_COT_SPONTANEOUS, 0, 1, false, false);

    int ioa = 100;

    bool added = false;

    do {
        InformationObject io = (InformationObject) SinglePointInformation_create(NULL, ioa, true, IEC60870_QUALITY_GOOD);

        added = CS101_ASDU_addInformationObject(asdu, io);

        InformationObject_destroy(io);

        ioa++;
    }
    while (added);

    CS101_ASDU_destroy(asdu);

    TEST_ASSERT_EQUAL_INT(124, ioa);
}


void
test_SingleEventType(void)
{
    tSingleEvent singleEvent = 0;

    EventState eventState = SingleEvent_getEventState(&singleEvent);

    TEST_ASSERT_EQUAL_INT(IEC60870_EVENTSTATE_INDETERMINATE_0, eventState);

    QualityDescriptorP qdp = SingleEvent_getQDP(&singleEvent);

    TEST_ASSERT_EQUAL_INT(0, qdp);
}

void
test_EventOfProtectionEquipmentWithTime(void)
{
#ifndef _WIN32
    tSingleEvent singleEvent = 0;
    struct sCP16Time2a elapsedTime;
    struct sCP56Time2a timestamp;

    EventOfProtectionEquipmentWithCP56Time2a e = EventOfProtectionEquipmentWithCP56Time2a_create(NULL, 1, &singleEvent, &elapsedTime, &timestamp);

    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) e);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) e);

    CS101_ASDU_encode(asdu, f);

    InformationObject_destroy((InformationObject) e);
    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    InformationObject io = CS101_ASDU_getElement(asdu2, 1);

    TEST_ASSERT_NOT_NULL(io);

    EventOfProtectionEquipmentWithCP56Time2a e2 = (EventOfProtectionEquipmentWithCP56Time2a) io;

    SingleEvent se = EventOfProtectionEquipmentWithCP56Time2a_getEvent(e2);

    QualityDescriptorP qdp = SingleEvent_getQDP(se);

    InformationObject_destroy(io);
    CS101_ASDU_destroy(asdu2);

    TEST_ASSERT_EQUAL_INT(0, qdp);
#endif
}

struct test_CS104SlaveConnectionIsRedundancyGroup_Info
{
    bool running;
    CS104_Slave slave;
};

static void*
test_CS104SlaveConnectionIsRedundancyGroup_enqueueThreadFunction(void* parameter)
{
    struct test_CS104SlaveConnectionIsRedundancyGroup_Info* info =  (struct test_CS104SlaveConnectionIsRedundancyGroup_Info*) parameter;

    CS101_AppLayerParameters alParams = CS104_Slave_getAppLayerParameters(info->slave);

    int16_t scaledValue = 0;

    while (info->running) {

        CS101_ASDU newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_PERIODIC, 0, 1, false, false);

          InformationObject io = (InformationObject) MeasuredValueScaled_create(NULL, 110, scaledValue, IEC60870_QUALITY_GOOD);

          scaledValue++;

          CS101_ASDU_addInformationObject(newAsdu, io);

          InformationObject_destroy(io);

          CS104_Slave_enqueueASDU(info->slave, newAsdu);

          CS101_ASDU_destroy(newAsdu);
    }

    return NULL;
}

void
test_CS104SlaveConnectionIsRedundancyGroup()
{
    CS104_Slave slave = CS104_Slave_create(100, 100);

    CS104_Slave_setServerMode(slave, CS104_MODE_CONNECTION_IS_REDUNDANCY_GROUP);
    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    struct test_CS104SlaveConnectionIsRedundancyGroup_Info info;
    info.running = true;
    info.slave = slave;

    Thread enqueueThread = Thread_create(test_CS104SlaveConnectionIsRedundancyGroup_enqueueThreadFunction, &info, false);
    Thread_start(enqueueThread);

    CS104_Connection con = CS104_Connection_create("127.0.0.1", 20004);

    int i;

    for (i = 0; i < 50; i++) {
        bool result = CS104_Connection_connect(con);
        TEST_ASSERT_TRUE(result);

        CS104_Connection_sendStartDT(con);

        Thread_sleep(10);

        CS104_Connection_close(con);
    }

    info.running = false;
    Thread_destroy(enqueueThread);

    CS104_Connection_destroy(con);

    CS104_Slave_destroy(slave);
}

void
test_CS104SlaveSingleRedundancyGroup()
{
    CS104_Slave slave = CS104_Slave_create(100, 100);

    CS104_Slave_setServerMode(slave, CS104_MODE_SINGLE_REDUNDANCY_GROUP);
    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    struct test_CS104SlaveConnectionIsRedundancyGroup_Info info;
    info.running = true;
    info.slave = slave;

    Thread enqueueThread = Thread_create(test_CS104SlaveConnectionIsRedundancyGroup_enqueueThreadFunction, &info, false);
    Thread_start(enqueueThread);

    CS104_Connection con = CS104_Connection_create("127.0.0.1", 20004);

    int i;

    for (i = 0; i < 50; i++) {
        bool result = CS104_Connection_connect(con);
        TEST_ASSERT_TRUE(result);

        CS104_Connection_sendStartDT(con);

        Thread_sleep(10);

        CS104_Connection_close(con);
    }

    info.running = false;
    Thread_destroy(enqueueThread);

    CS104_Connection_destroy(con);

    CS104_Slave_destroy(slave);
}

struct stest_CS104SlaveEventQueue1 {
    int asduHandlerCalled;
    int spontCount;
    int16_t lastScaledValue;
};

static bool
test_CS104SlaveEventQueue1_asduReceivedHandler (void* parameter, int address, CS101_ASDU asdu)
{
    struct stest_CS104SlaveEventQueue1* info = (struct stest_CS104SlaveEventQueue1*) parameter;

    info->asduHandlerCalled++;

    if (CS101_ASDU_getCOT(asdu) == CS101_COT_SPONTANEOUS) {
        info->spontCount++;


        if (CS101_ASDU_getTypeID(asdu) == M_ME_NB_1) {
            static uint8_t ioBuf[250];

            MeasuredValueScaled mv = (MeasuredValueScaled) CS101_ASDU_getElementEx(asdu, (InformationObject) ioBuf, 0);

            info->lastScaledValue = MeasuredValueScaled_getValue(mv);
        }
    }

    return true;
}

void
test_CS104SlaveEventQueue1()
{
    CS104_Slave slave = CS104_Slave_create(10, 10);

    CS104_Slave_setServerMode(slave, CS104_MODE_SINGLE_REDUNDANCY_GROUP);
    CS104_Slave_setLocalPort(slave, 20004);

    CS104_Slave_start(slave);

    CS101_AppLayerParameters alParams = CS104_Slave_getAppLayerParameters(slave);

    struct stest_CS104SlaveEventQueue1 info;
    info.asduHandlerCalled = 0;
    info.spontCount = 0;
    info.lastScaledValue = 0;

    int16_t scaledValue = 0;

    int i;

    for (int i = 0; i < 15; i++) {
        CS101_ASDU newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_SPONTANEOUS, 0, 1, false, false);

        InformationObject io = (InformationObject) MeasuredValueScaled_create(NULL, 110, scaledValue, IEC60870_QUALITY_GOOD);

        scaledValue++;

        CS101_ASDU_addInformationObject(newAsdu, io);

        InformationObject_destroy(io);

        CS104_Slave_enqueueASDU(slave, newAsdu);

        CS101_ASDU_destroy(newAsdu);
    }

    CS104_Connection con = CS104_Connection_create("127.0.0.1", 20004);

    CS104_Connection_setASDUReceivedHandler(con, test_CS104SlaveEventQueue1_asduReceivedHandler, &info);

    bool result = CS104_Connection_connect(con);
    TEST_ASSERT_TRUE(result);

    CS104_Connection_sendStartDT(con);

    Thread_sleep(500);

    CS104_Connection_close(con);

    TEST_ASSERT_EQUAL_INT(14, info.lastScaledValue);

    result = CS104_Connection_connect(con);
    TEST_ASSERT_TRUE(result);

    CS104_Connection_sendStartDT(con);

    for (int i = 0; i < 15; i++) {
        CS101_ASDU newAsdu = CS101_ASDU_create(alParams, false, CS101_COT_SPONTANEOUS, 0, 1, false, false);

        InformationObject io = (InformationObject) MeasuredValueScaled_create(NULL, 110, scaledValue, IEC60870_QUALITY_GOOD);

        scaledValue++;

        CS101_ASDU_addInformationObject(newAsdu, io);

        InformationObject_destroy(io);

        CS104_Slave_enqueueASDU(slave, newAsdu);

        CS101_ASDU_destroy(newAsdu);

        Thread_sleep(10);
    }

    Thread_sleep(500);

    CS104_Connection_close(con);

    TEST_ASSERT_EQUAL_INT(30, info.asduHandlerCalled);
    TEST_ASSERT_EQUAL_INT(30, info.spontCount);
    TEST_ASSERT_EQUAL_INT(29, info.lastScaledValue);

    CS104_Connection_destroy(con);

    CS104_Slave_destroy(slave);
}


void
test_IpAddressHandling(void)
{
    struct sCS104_IPAddress ipAddr1;

    CS104_IPAddress_setFromString(&ipAddr1, "192.168.34.25");

    TEST_ASSERT_EQUAL_INT(IP_ADDRESS_TYPE_IPV4, ipAddr1.type);
    TEST_ASSERT_EQUAL_UINT8(192, ipAddr1.address[0]);
    TEST_ASSERT_EQUAL_UINT8(168, ipAddr1.address[1]);
    TEST_ASSERT_EQUAL_UINT8(34, ipAddr1.address[2]);
    TEST_ASSERT_EQUAL_UINT8(25, ipAddr1.address[3]);

    CS104_IPAddress_setFromString(&ipAddr1, "1:22:333:aaaa:b:c:d:e");
    TEST_ASSERT_EQUAL_INT(IP_ADDRESS_TYPE_IPV6, ipAddr1.type);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[0]);
    TEST_ASSERT_EQUAL_UINT8(0x01, ipAddr1.address[1]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[2]);
    TEST_ASSERT_EQUAL_UINT8(0x22, ipAddr1.address[3]);
    TEST_ASSERT_EQUAL_UINT8(0x03, ipAddr1.address[4]);
    TEST_ASSERT_EQUAL_UINT8(0x33, ipAddr1.address[5]);
    TEST_ASSERT_EQUAL_UINT8(0xaa, ipAddr1.address[6]);
    TEST_ASSERT_EQUAL_UINT8(0xaa, ipAddr1.address[7]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[8]);
    TEST_ASSERT_EQUAL_UINT8(0x0b, ipAddr1.address[9]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[10]);
    TEST_ASSERT_EQUAL_UINT8(0x0c, ipAddr1.address[11]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[12]);
    TEST_ASSERT_EQUAL_UINT8(0x0d, ipAddr1.address[13]);
    TEST_ASSERT_EQUAL_UINT8(0x00, ipAddr1.address[14]);
    TEST_ASSERT_EQUAL_UINT8(0x0e, ipAddr1.address[15]);
}

void
test_BitString32(void)
{
    BitString32 bs32;

    bs32 = BitString32_createEx(NULL, 101, 0xaaaa, IEC60870_QUALITY_INVALID);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, BitString32_getQuality(bs32));

    BitString32_destroy(bs32);

	bs32 = BitString32_create(NULL, 101, 0xaaaa);

	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, BitString32_getQuality(bs32));

	BitString32_destroy(bs32);

    bs32 = BitString32_createEx(NULL, 101, 0xaaaa, IEC60870_QUALITY_INVALID | IEC60870_QUALITY_NON_TOPICAL);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID + IEC60870_QUALITY_NON_TOPICAL, BitString32_getQuality(bs32));

    TEST_ASSERT_EQUAL_UINT32(0xaaaa, BitString32_getValue(bs32));

    TEST_ASSERT_EQUAL_INT(101, InformationObject_getObjectAddress((InformationObject) bs32));

    BitString32_destroy(bs32);

    Bitstring32WithCP24Time2a bs32cp24;

    struct sCP24Time2a cp24;

    bs32cp24 = Bitstring32WithCP24Time2a_createEx(NULL, 100002, 0xbbbb, IEC60870_QUALITY_INVALID, &cp24);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID, BitString32_getQuality((BitString32)bs32cp24));

    TEST_ASSERT_EQUAL_UINT32(0xbbbb, BitString32_getValue((BitString32)bs32cp24));

    TEST_ASSERT_EQUAL_INT(100002, InformationObject_getObjectAddress((InformationObject) bs32cp24));

    Bitstring32WithCP24Time2a_destroy(bs32cp24);

    bs32cp24 = Bitstring32WithCP24Time2a_create(NULL, 100002, 0xbbbb, &cp24);

	TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, BitString32_getQuality((BitString32)bs32cp24));

	TEST_ASSERT_EQUAL_UINT32(0xbbbb, BitString32_getValue((BitString32)bs32cp24));

	TEST_ASSERT_EQUAL_INT(100002, InformationObject_getObjectAddress((InformationObject) bs32cp24));

	Bitstring32WithCP24Time2a_destroy(bs32cp24);

    Bitstring32WithCP56Time2a bs32cp56;

    struct sCP56Time2a cp56;

    bs32cp56 = Bitstring32WithCP56Time2a_createEx(NULL, 1000002, 0xcccc, IEC60870_QUALITY_INVALID | IEC60870_QUALITY_NON_TOPICAL, &cp56);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_INVALID + IEC60870_QUALITY_NON_TOPICAL, BitString32_getQuality((BitString32)bs32cp56));

    TEST_ASSERT_EQUAL_UINT32(0xcccc, BitString32_getValue((BitString32)bs32cp56));

    TEST_ASSERT_EQUAL_INT(1000002, InformationObject_getObjectAddress((InformationObject) bs32cp56));

    Bitstring32WithCP56Time2a_destroy(bs32cp56);

    bs32cp56 = Bitstring32WithCP56Time2a_create(NULL, 1000002, 0xcccc, &cp56);

    TEST_ASSERT_EQUAL_UINT8(IEC60870_QUALITY_GOOD, BitString32_getQuality((BitString32)bs32cp56));

    TEST_ASSERT_EQUAL_UINT32(0xcccc, BitString32_getValue((BitString32)bs32cp56));

    TEST_ASSERT_EQUAL_INT(1000002, InformationObject_getObjectAddress((InformationObject) bs32cp56));

    Bitstring32WithCP56Time2a_destroy(bs32cp56);
}

void
test_BitString32xx_encodeDecode(void)
{
#ifndef _WIN32
    uint8_t buffer[256];

    struct sBufferFrame bf;

    Frame f = BufferFrame_initialize(&bf, buffer, 0);

    CS101_ASDU asdu = CS101_ASDU_create(&defaultAppLayerParameters, false, CS101_COT_PERIODIC, 0, 1, false, false);

    BitString32 bs32_1 = BitString32_createEx(NULL, 101, (uint32_t) 0xaaaaaaaaaa, IEC60870_QUALITY_INVALID);
    BitString32 bs32_2 = BitString32_create(NULL, 102, (uint32_t) 0x0000000000);
    BitString32 bs32_3 = BitString32_create(NULL, 103, (uint32_t) 0xffffffffffUL);

    CS101_ASDU_addInformationObject(asdu, (InformationObject) bs32_1);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) bs32_2);
    CS101_ASDU_addInformationObject(asdu, (InformationObject) bs32_3);

    CS101_ASDU_encode(asdu, f);

    InformationObject_destroy((InformationObject) bs32_1);
    InformationObject_destroy((InformationObject) bs32_2);
    InformationObject_destroy((InformationObject) bs32_3);
    CS101_ASDU_destroy(asdu);

    CS101_ASDU asdu2 = CS101_ASDU_createFromBuffer(&defaultAppLayerParameters, buffer, Frame_getMsgSize(f));

    BitString32 bs32_1_dec = (BitString32) CS101_ASDU_getElement(asdu2, 0);
    BitString32 bs32_2_dec = (BitString32) CS101_ASDU_getElement(asdu2, 1);
    BitString32 bs32_3_dec = (BitString32) CS101_ASDU_getElement(asdu2, 2);

    TEST_ASSERT_EQUAL_UINT32(0xaaaaaaaaaaUL, BitString32_getValue(bs32_1_dec));
    TEST_ASSERT_EQUAL_INT(IEC60870_QUALITY_INVALID, BitString32_getQuality(bs32_1_dec));

    TEST_ASSERT_EQUAL_UINT32(0x0000000000UL, BitString32_getValue(bs32_2_dec));
    TEST_ASSERT_EQUAL_INT(IEC60870_QUALITY_GOOD, BitString32_getQuality(bs32_2_dec));

    TEST_ASSERT_EQUAL_UINT32(0xffffffffUL, BitString32_getValue(bs32_3_dec));
    TEST_ASSERT_EQUAL_INT(IEC60870_QUALITY_GOOD, BitString32_getQuality(bs32_3_dec));

    InformationObject_destroy((InformationObject)bs32_1_dec);
    InformationObject_destroy((InformationObject)bs32_2_dec);
    InformationObject_destroy((InformationObject)bs32_3_dec);

    CS101_ASDU_destroy(asdu2);
#endif
}

void
test_CS104_Slave_CreateDestroy(void)
{
	CS104_Slave slave = CS104_Slave_create(100, 100);

	TEST_ASSERT_NOT_NULL(slave);

	CS104_Slave_destroy(slave);
}

void
test_CS104_Connection_CreateDestroy(void)
{
	CS104_Connection con = CS104_Connection_create("127.0.0.1", 2404);

	TEST_ASSERT_NOT_NULL(con);

	CS104_Connection_destroy(con);
}

void
test_CS104_MasterSlave_CreateDestroy(void)
{
	CS104_Slave slave = CS104_Slave_create(100, 100);

	TEST_ASSERT_NOT_NULL(slave);

	CS104_Slave_setLocalPort(slave, 20004);

	CS104_Connection con = CS104_Connection_create("127.0.0.1", 20004);

	TEST_ASSERT_NOT_NULL(con);

	CS104_Connection_connect(con);

	CS104_Slave_destroy(slave);

	CS104_Connection_destroy(con);
}

void
test_CS104_MasterSlave_CreateDestroyLoop(void)
{
	CS104_Slave slave = NULL;
	CS104_Connection con = NULL;

	for (int i = 0; i < 1000; i++) {
		slave = CS104_Slave_create(100, 100);

		TEST_ASSERT_NOT_NULL(slave);

		CS104_Slave_setLocalPort(slave, 20004);

		con = CS104_Connection_create("127.0.0.1", 20004);

		TEST_ASSERT_NOT_NULL(con);

		CS104_Connection_connect(con);

		CS104_Slave_destroy(slave);

		CS104_Connection_destroy(con);
	}
}


void
test_CS104_Connection_ConnectTimeout(void)
{
	CS104_Connection con = CS104_Connection_create("192.168.3.120", 2404);

	TEST_ASSERT_NOT_NULL(con);

	bool result = CS104_Connection_connect(con);

	TEST_ASSERT_FALSE(result);

	CS104_Connection_destroy(con);
}

int
main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_CS104_Slave_CreateDestroy);
    RUN_TEST(test_CS104_MasterSlave_CreateDestroyLoop);
    RUN_TEST(test_CS104_Connection_CreateDestroy);
    RUN_TEST(test_CS104_MasterSlave_CreateDestroy);
    RUN_TEST(test_CP56Time2a);
    RUN_TEST(test_CP56Time2aToMsTimestamp);
    RUN_TEST(test_StepPositionInformation);
    RUN_TEST(test_addMaxNumberOfIOsToASDU);
    RUN_TEST(test_SingleEventType);
    RUN_TEST(test_BitString32);
    RUN_TEST(test_BitString32xx_encodeDecode);
    RUN_TEST(test_EventOfProtectionEquipmentWithTime);
    RUN_TEST(test_IpAddressHandling);

    RUN_TEST(test_CS104SlaveConnectionIsRedundancyGroup);
    RUN_TEST(test_CS104SlaveSingleRedundancyGroup);

    RUN_TEST(test_CS104SlaveEventQueue1);

    RUN_TEST(test_CS104_Connection_ConnectTimeout);

    return UNITY_END();
}
