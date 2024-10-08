/*
 *  Class TestAudioConvert
 *
 *  Copyright (C) Bubblestuff Pty Ltd 2013
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <QtTest/QtTest>

#include "libmythbase/mythcorecontext.h"
#include "libmyth/audio/audioconvert.h"

#define ISIZEOF(type) ((int)sizeof(type))

class TestAudioConvert: public QObject
{
    Q_OBJECT

  private slots:
    // called at the beginning of these sets of tests
    static void initTestCase(void)
    {
        gCoreContext = new MythCoreContext("bin_version", nullptr);
    }

    static void Identical_data(void)
    {
        QTest::addColumn<int>("SAMPLES");
        QTest::newRow("Full Range") << (INT16_MAX - INT16_MIN);
        QTest::newRow("8 bytes") << 8;
        QTest::newRow("24 bytes") << 24;
        QTest::newRow("0 bytes") << 0;
    }

    // test s16 -> float -> s16
    static void Identical(void)
    {
        QFETCH(int, SAMPLES);

        int   SIZEARRAY = SAMPLES;
        auto *arrays1   = (uint16_t*)av_malloc(SIZEARRAY * ISIZEOF(uint16_t));
        auto *arrays2   = (uint16_t*)av_malloc(SIZEARRAY * ISIZEOF(uint16_t));

        uint16_t j = INT16_MIN;
        for (int i = 0; i < SAMPLES; i++, j++)
        {
            arrays1[i] = j;
        }

        AudioConvert ac = AudioConvert(FORMAT_S16, FORMAT_S16);

        int val1 = ac.Process(arrays2, arrays1, SAMPLES * ISIZEOF(arrays1[0]));
        QCOMPARE(val1, SAMPLES * ISIZEOF(arrays2[0]));
        for (int i = 0; i < SAMPLES; i++)
        {
            QCOMPARE(arrays1[i], arrays2[i]);
        }
        av_free(arrays1);
        av_free(arrays2);
    }

    static void S16ToFloat_data(void)
    {
        QTest::addColumn<int>("SAMPLES");
        QTest::newRow("Full Range") << (INT16_MAX - INT16_MIN);
        QTest::newRow("8 bytes") << 8;
        QTest::newRow("24 bytes") << 24;
        QTest::newRow("0 bytes") << 0;
    }

    // test s16 -> float -> s16 is lossless
    static void S16ToFloat(void)
    {
        QFETCH(int, SAMPLES);

        int   SIZEARRAY = SAMPLES;
        auto *arrays1   = (uint16_t*)av_malloc(SIZEARRAY * ISIZEOF(uint16_t));
        auto *arrays2   = (uint16_t*)av_malloc(SIZEARRAY * ISIZEOF(uint16_t));
        auto *arrayf    = (float*)av_malloc(SIZEARRAY * ISIZEOF(float));

        uint16_t j = INT16_MIN;
        for (int i = 0; i < SAMPLES; i++, j++)
        {
            arrays1[i] = j;
        }

        AudioConvert acf = AudioConvert(FORMAT_S16, FORMAT_FLT);
        AudioConvert acs = AudioConvert(FORMAT_FLT, FORMAT_S16);

        int val1 = acf.Process(arrayf, arrays1, SAMPLES * ISIZEOF(arrays1[0]));
        QCOMPARE(val1, SAMPLES * ISIZEOF(arrayf[0]));
        int val2 = acs.Process(arrays2, arrayf, SAMPLES * ISIZEOF(arrayf[0]));
        QCOMPARE(val2, SAMPLES * ISIZEOF(arrays2[0]));
        for (int i = 0; i < SAMPLES; i++)
        {
            QCOMPARE(arrays1[i], arrays2[i]);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf);
    }

    static void S16ToS24LSB_data(void)
    {
        QTest::addColumn<int>("SAMPLES");
        QTest::newRow("Full Range") << (INT16_MAX - INT16_MIN);
        QTest::newRow("8 bytes") << 8;
        QTest::newRow("24 bytes") << 24;
        QTest::newRow("0 bytes") << 0;
    }

    // test S16 -> S24LSB -> S16 is lossless
    static void S16ToS24LSB(void)
    {
        QFETCH(int, SAMPLES);

        int SIZEARRAY  = SAMPLES;

        auto *arrays1  = (uint16_t*)av_malloc(SIZEARRAY * ISIZEOF(uint16_t));
        auto *arrays2  = (uint16_t*)av_malloc(SIZEARRAY * ISIZEOF(uint16_t));
        auto *arrays24 = (int32_t*)av_malloc(SIZEARRAY * ISIZEOF(int32_t));

        uint16_t j = INT16_MIN;
        for (int i = 0; i < SAMPLES; i++, j++)
        {
            arrays1[i] = j;
        }

        AudioConvert ac24   = AudioConvert(FORMAT_S16, FORMAT_S24LSB);
        AudioConvert acs    = AudioConvert(FORMAT_S24LSB, FORMAT_S16);

        int val1 = ac24.Process(arrays24, arrays1, SAMPLES * ISIZEOF(arrays1[0]));
        QCOMPARE(val1, SAMPLES * ISIZEOF(arrays24[0]));
        int val2 = acs.Process(arrays2, arrays24, SAMPLES * ISIZEOF(arrays24[0]));
        QCOMPARE(val2, SAMPLES * ISIZEOF(arrays2[0]));
        for (int i = 0; i < SAMPLES; i++)
        {
            QCOMPARE(arrays1[i], arrays2[i]);
            // Check we are indeed getting a 24 bits int
            QVERIFY(arrays24[i] >= -(1<<23));
            QVERIFY(arrays24[i] <= ((1<<23)-1));
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrays24);
    }

    static void S24LSBToS32_data(void)
    {
        QTest::addColumn<int>("SAMPLES");
        QTest::newRow("Full Range") << (INT16_MAX - INT16_MIN);
        QTest::newRow("8 bytes") << 8;
        QTest::newRow("24 bytes") << 24;
        QTest::newRow("0 bytes") << 0;
    }

    static void S24LSBToS32(void)
    {
        QFETCH(int, SAMPLES);

        int SIZEARRAY  = SAMPLES;

        auto *arrays1  = (int32_t*)av_malloc(SIZEARRAY * ISIZEOF(int32_t));
        auto *arrays2  = (int32_t*)av_malloc(SIZEARRAY * ISIZEOF(int32_t));
        auto *arrays32 = (int32_t*)av_malloc(SIZEARRAY * ISIZEOF(int32_t));

        uint16_t j = INT16_MIN;
        for (int i = 0; i < SAMPLES; i++, j++)
        {
            arrays1[i] = j;
        }

        AudioConvert ac24   = AudioConvert(FORMAT_S24LSB, FORMAT_S32);
        AudioConvert acs    = AudioConvert(FORMAT_S32, FORMAT_S24LSB);

        int val1 = ac24.Process(arrays32, arrays1, SAMPLES * ISIZEOF(arrays1[0]));
        QCOMPARE(val1, SAMPLES * ISIZEOF(arrays32[0]));
        int val2 = acs.Process(arrays2, arrays32, SAMPLES * ISIZEOF(arrays32[0]));
        QCOMPARE(val2, SAMPLES * ISIZEOF(arrays2[0]));
        for (int i = 0; i < SAMPLES; i++)
        {
            QCOMPARE(arrays1[i], arrays2[i]);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrays32);
    }

    static void S16ToS24_data(void)
    {
        QTest::addColumn<int>("SAMPLES");
        QTest::newRow("Full Range") << (INT16_MAX - INT16_MIN);
        QTest::newRow("8 bytes") << 8;
        QTest::newRow("24 bytes") << 24;
        QTest::newRow("0 bytes") << 0;
    }

    // test S16 -> S24 -> S16 is lossless
    static void S16ToS24(void)
    {
        QFETCH(int, SAMPLES);

        int SIZEARRAY  = SAMPLES;

        auto *arrays1  = (uint16_t*)av_malloc(SIZEARRAY * ISIZEOF(uint16_t));
        auto *arrays2  = (uint16_t*)av_malloc(SIZEARRAY * ISIZEOF(uint16_t));
        auto *arrays24 = (int32_t*)av_malloc(SIZEARRAY * ISIZEOF(int32_t));

        uint16_t j = INT16_MIN;
        for (int i = 0; i < SAMPLES; i++, j++)
        {
            arrays1[i] = j << 8;
        }

        AudioConvert ac24   = AudioConvert(FORMAT_S16, FORMAT_S24);
        AudioConvert acs    = AudioConvert(FORMAT_S24, FORMAT_S16);

        int val1 = ac24.Process(arrays24, arrays1, SAMPLES * ISIZEOF(arrays1[0]));
        QCOMPARE(val1, SAMPLES * ISIZEOF(arrays24[0]));
        int val2 = acs.Process(arrays2, arrays24, SAMPLES * ISIZEOF(arrays24[0]));
        QCOMPARE(val2, SAMPLES * ISIZEOF(arrays2[0]));
        for (int i = 0; i < SAMPLES; i++)
        {
            QCOMPARE(arrays1[i], arrays2[i]);
            // Check we are indeed getting a 24 bits int
            QCOMPARE(arrays24[i] & ~0xffff, arrays24[i]);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrays24);
    }

    static void S24ToS32_data(void)
    {
        QTest::addColumn<int>("SAMPLES");
        QTest::newRow("Full Range") << (INT16_MAX - INT16_MIN);
        QTest::newRow("8 bytes") << 8;
        QTest::newRow("24 bytes") << 24;
        QTest::newRow("0 bytes") << 0;
    }

    static void S24ToS32(void)
    {
        QFETCH(int, SAMPLES);

        int SIZEARRAY  = SAMPLES;

        auto *arrays1  = (int32_t*)av_malloc(SIZEARRAY * ISIZEOF(int32_t));
        auto *arrays2  = (int32_t*)av_malloc(SIZEARRAY * ISIZEOF(int32_t));
        auto *arrays32 = (int32_t*)av_malloc(SIZEARRAY * ISIZEOF(int32_t));

        uint16_t j = INT16_MIN;
        for (int i = 0; i < SAMPLES; i++, j++)
        {
            arrays1[i] = j << 8;
        }

        AudioConvert ac32   = AudioConvert(FORMAT_S24, FORMAT_S32);
        AudioConvert acs    = AudioConvert(FORMAT_S32, FORMAT_S24);

        int val1 = ac32.Process(arrays32, arrays1, SAMPLES * ISIZEOF(arrays1[0]));
        QCOMPARE(val1, SAMPLES * ISIZEOF(arrays32[0]));
        int val2 = acs.Process(arrays2, arrays32, SAMPLES * ISIZEOF(arrays32[0]));
        QCOMPARE(val2, SAMPLES * ISIZEOF(arrays2[0]));
        for (int i = 0; i < SAMPLES; i++)
        {
            QCOMPARE(arrays1[i], arrays2[i]);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrays32);
    }

    static void S16ToS32_data(void)
    {
        QTest::addColumn<int>("SAMPLES");
        QTest::newRow("Full Range") << (INT16_MAX - INT16_MIN);
        QTest::newRow("8 bytes") << 8;
        QTest::newRow("24 bytes") << 24;
        QTest::newRow("0 bytes") << 0;
    }

    // test S16 -> S24 -> S16 is lossless
    static void S16ToS32(void)
    {
        QFETCH(int, SAMPLES);

        int SIZEARRAY  = SAMPLES;

        auto *arrays1  = (uint16_t*)av_malloc(SIZEARRAY * ISIZEOF(uint16_t));
        auto *arrays2  = (uint16_t*)av_malloc(SIZEARRAY * ISIZEOF(uint16_t));
        auto *arrays32 = (int32_t*)av_malloc(SIZEARRAY * ISIZEOF(int32_t));

        uint16_t j = INT16_MIN;
        for (int i = 0; i < SAMPLES; i++, j++)
        {
            arrays1[i] = j;
        }

        AudioConvert ac32   = AudioConvert(FORMAT_S16, FORMAT_S32);
        AudioConvert acs    = AudioConvert(FORMAT_S32, FORMAT_S16);

        int val1 = ac32.Process(arrays32, arrays1, SAMPLES * ISIZEOF(arrays1[0]));
        QCOMPARE(val1, SAMPLES * ISIZEOF(arrays32[0]));
        int val2 = acs.Process(arrays2, arrays32, SAMPLES * ISIZEOF(arrays32[0]));
        QCOMPARE(val2, SAMPLES * ISIZEOF(arrays2[0]));
        for (int i = 0; i < SAMPLES; i++)
        {
            QCOMPARE(arrays1[i], arrays2[i]);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrays32);
    }

    static void U8ToS16_data(void)
    {
        QTest::addColumn<int>("SAMPLES");
        QTest::newRow("Full Range") << 256;
        QTest::newRow("8 bytes") << 8;
        QTest::newRow("24 bytes") << 24;
        QTest::newRow("0 bytes") << 0;
    }

    // test U8 -> S16 -> U8 is lossless
    static void U8ToS16(void)
    {
        QFETCH(int, SAMPLES);

        int SIZEARRAY  = 256;

        auto *arrays1  = (uint8_t*)av_malloc(SIZEARRAY * ISIZEOF(uint8_t));
        auto *arrays2  = (uint8_t*)av_malloc(SIZEARRAY * ISIZEOF(uint8_t));
        auto *arrays32 = (uint16_t*)av_malloc(SIZEARRAY * ISIZEOF(uint16_t));

        uint8_t j = 0;
        for (int i = 0; i < SAMPLES; i++, j++)
        {
            arrays1[i] = j;
        }

        AudioConvert ac32   = AudioConvert(FORMAT_U8, FORMAT_S16);
        AudioConvert acs    = AudioConvert(FORMAT_S16, FORMAT_U8);

        int val1 = ac32.Process(arrays32, arrays1, SAMPLES * ISIZEOF(arrays1[0]));
        QCOMPARE(val1, SAMPLES * ISIZEOF(arrays32[0]));
        int val2 = acs.Process(arrays2, arrays32, SAMPLES * ISIZEOF(arrays32[0]));
        QCOMPARE(val2, SAMPLES * ISIZEOF(arrays2[0]));
        for (int i = 0; i < SAMPLES; i++)
        {
            QCOMPARE(arrays1[i], arrays2[i]);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrays32);
    }

    static void S32ClipTest(void)
    {
        int SIZEARRAY       = 256;
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetint32_t   = 0;
        int offsetfloat1    = 1;
        int offsetfloat2    = 0;

        auto *arrays1 = (int32_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(int32_t));
        // has to be 16 int32_t for 16 bytes boundary * 2
        auto *arrays2 = (int32_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(int32_t));
        auto *arrayf1 = (float*)av_malloc((SIZEARRAY+offsetfloat1+4) * ISIZEOF(float));
        auto *arrayf2 = (float*)av_malloc((SIZEARRAY+offsetfloat2+4) * ISIZEOF(float));

        arrayf1[0+offsetfloat1] = -1.2;
        arrayf1[1+offsetfloat1] = -1.1;
        arrayf1[2+offsetfloat1] = -1.0;
        arrayf1[3+offsetfloat1] = -0.5;
        arrayf1[4+offsetfloat1] = 0.5;
        arrayf1[5+offsetfloat1] = 1.0;
        arrayf1[6+offsetfloat1] = 1.1;
        arrayf1[7+offsetfloat1] = 1.2;
        arrayf2[0+offsetfloat2] = -1.0;
        arrayf2[1+offsetfloat2] = -1.0;
        arrayf2[2+offsetfloat2] = -1.0;
        arrayf2[3+offsetfloat2] = -0.5;
        arrayf2[4+offsetfloat2] = 0.5;
        arrayf2[5+offsetfloat2] = 1.0;
        arrayf2[6+offsetfloat2] = 1.0;
        arrayf2[7+offsetfloat2] = 1.0;
        // arrays1 is produced by C-code
        // arrays2 is produced by SSE
        AudioConvert::fromFloat(FORMAT_S32, arrays1, arrayf1+offsetfloat1, SIZEARRAY * ISIZEOF(float));
        AudioConvert::fromFloat(FORMAT_S32, arrays2, arrayf2+offsetfloat2, SIZEARRAY * ISIZEOF(float));
        for (int i = 0; i < 8; i++)
        {
            QCOMPARE(arrays2[i], arrays1[i]);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf1);
        av_free(arrayf2);
    }

    static void FloatS32ClipTest3_data(void)
    {
        QTest::addColumn<int>("OFFSET");
        QTest::newRow("Use SSE accelerated code") << 0;
        QTest::newRow("Use C code") << 1;
    }

    static void FloatS32ClipTest3(void)
    {
        int SIZEARRAY       = 256;
        QFETCH(int, OFFSET);
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetint32_t   = 0;
        int offsetfloat1    = OFFSET;

        auto *arrays1 = (int32_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(int32_t));
        // has to be 16 int32_t for 16 bytes boundary * 2
        auto *arrays2 = (int32_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(int32_t));
        auto *arrayf1 = (float*)av_malloc((SIZEARRAY+offsetfloat1+4) * ISIZEOF(float));

        arrayf1[0+offsetfloat1] = -1.2;
        arrayf1[1+offsetfloat1] = -1.1;
        arrayf1[2+offsetfloat1] = -1.0;
        arrayf1[3+offsetfloat1] = -1.3;
        arrayf1[4+offsetfloat1] = 1.3;
        arrayf1[5+offsetfloat1] = 1.0;
        arrayf1[6+offsetfloat1] = 1.1;
        arrayf1[7+offsetfloat1] = 1.2;
        arrayf1[8+offsetfloat1] = 0;
        arrays2[0] = -2147483648;
        arrays2[1] = -2147483648;
        arrays2[2] = -2147483648;
        arrays2[3] = -2147483648;
        arrays2[4] = 2147483520;   // (1<<31)-128
        arrays2[5] = 2147483520;
        arrays2[6] = 2147483520;
        arrays2[7] = 2147483520;
        arrays2[8] = 0;
        AudioConvert::fromFloat(FORMAT_S32, arrays1, arrayf1+offsetfloat1, SIZEARRAY * ISIZEOF(float));
        for (int i = 0; i < 9; i++)
        {
            QCOMPARE(arrays2[i], arrays1[i]);
        }

        // Force C code (< 16)
        AudioConvert::fromFloat(FORMAT_S32, arrays1, arrayf1+offsetfloat1, 9 * ISIZEOF(float));
        for (int i = 0; i < 9; i++)
        {
            QCOMPARE(arrays2[i], arrays1[i]);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf1);
    }

    static void FloatS16ClipTest3_data(void)
    {
        QTest::addColumn<int>("OFFSET");
        QTest::newRow("Use SSE accelerated code") << 0;
        QTest::newRow("Use C code") << 1;
    }

    static void FloatS16ClipTest3(void)
    {
        int SIZEARRAY       = 256;
        QFETCH(int, OFFSET);
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetint32_t   = 0;
        int offsetfloat1    = OFFSET;

        auto *arrays1 = (uint16_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(uint16_t));
        // has to be 16 int32_t for 16 bytes boundary * 2
        auto *arrays2 = (uint16_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(uint16_t));
        auto *arrayf1 = (float*)av_malloc((SIZEARRAY+offsetfloat1+4) * ISIZEOF(float));

        arrayf1[0+offsetfloat1] = -1.2;
        arrayf1[1+offsetfloat1] = -1.1;
        arrayf1[2+offsetfloat1] = -1.0;
        arrayf1[3+offsetfloat1] = -1.3;
        arrayf1[4+offsetfloat1] = 1.3;
        arrayf1[5+offsetfloat1] = 1.0;
        arrayf1[6+offsetfloat1] = 1.1;
        arrayf1[7+offsetfloat1] = 1.2;
        arrayf1[8+offsetfloat1] = 0.0;
        arrays2[0] = -32768;
        arrays2[1] = -32768;
        arrays2[2] = -32768;
        arrays2[3] = -32768;
        arrays2[4] = 32767;
        arrays2[5] = 32767;
        arrays2[6] = 32767;
        arrays2[7] = 32767;
        arrays2[8] = 0;
        AudioConvert::fromFloat(FORMAT_S16, arrays1, arrayf1+offsetfloat1, SIZEARRAY * ISIZEOF(float));
        for (int i = 0; i < 9; i++)
        {
            QCOMPARE(arrays2[i], arrays1[i]);
        }

        // Force C code (< 16)
        AudioConvert::fromFloat(FORMAT_S16, arrays1, arrayf1+offsetfloat1, 9 * ISIZEOF(float));
        for (int i = 0; i < 9; i++)
        {
            QCOMPARE(arrays2[i], arrays1[i]);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf1);
    }

    static void FloatU8ClipTest3_data(void)
    {
        QTest::addColumn<int>("OFFSET");
        QTest::newRow("Use SSE accelerated code") << 0;
        QTest::newRow("Use C code") << 1;
    }

    static void FloatU8ClipTest3(void)
    {
        int SIZEARRAY       = 256;
        QFETCH(int, OFFSET);
        // +1 will never be 16-bytes aligned, forcing C-code
        int offsetint32_t   = 0;
        int offsetfloat1    = OFFSET;

        auto *arrays1 = (uint8_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(uint8_t));
        // has to be 16 int32_t for 16 bytes boundary * 2
        auto *arrays2 = (uint8_t*)av_malloc((SIZEARRAY+offsetint32_t+4) * ISIZEOF(uint8_t));
        auto *arrayf1 = (float*)av_malloc((SIZEARRAY+offsetfloat1+4) * ISIZEOF(float));

        arrayf1[0+offsetfloat1] = -1.2;
        arrayf1[1+offsetfloat1] = -1.1;
        arrayf1[2+offsetfloat1] = -1.0;
        arrayf1[3+offsetfloat1] = -1.3;
        arrayf1[4+offsetfloat1] = 1.3;
        arrayf1[5+offsetfloat1] = 1.0;
        arrayf1[6+offsetfloat1] = 1.1;
        arrayf1[7+offsetfloat1] = 1.2;
        arrayf1[8+offsetfloat1] = 0.0;
        arrays2[0] = 0;
        arrays2[1] = 0;
        arrays2[2] = 0;
        arrays2[3] = 0;
        arrays2[4] = 255;
        arrays2[5] = 255;
        arrays2[6] = 255;
        arrays2[7] = 255;
        arrays2[8] = 128;
        AudioConvert::fromFloat(FORMAT_U8, arrays1, arrayf1+offsetfloat1, SIZEARRAY * ISIZEOF(float));
        for (int i = 0; i < 9; i++)
        {
            QCOMPARE(arrays2[i], arrays1[i]);
        }

        // Force C code (< 16)
        AudioConvert::fromFloat(FORMAT_U8, arrays1, arrayf1+offsetfloat1, 9 * ISIZEOF(float));
        for (int i = 0; i < 9; i++)
        {
            QCOMPARE(arrays2[i], arrays1[i]);
        }

        av_free(arrays1);
        av_free(arrays2);
        av_free(arrayf1);
    }
};
