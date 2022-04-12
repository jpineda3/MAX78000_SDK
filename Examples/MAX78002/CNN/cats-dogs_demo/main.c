/*******************************************************************************
* Copyright (C) 2020-2022 Maxim Integrated Products, Inc., All rights Reserved.
*
* This software is protected by copyright laws of the United States and
* of foreign countries. This material may also be protected by patent laws
* and technology transfer regulations of the United States and of foreign
* countries. This software is furnished under a license agreement and/or a
* nondisclosure agreement and may only be used or reproduced in accordance
* with the terms of those agreements. Dissemination of this information to
* any party or parties not specified in the license agreement and/or
* nondisclosure agreement is expressly prohibited.
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
* OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*
* Except as contained in this notice, the name of Maxim Integrated
* Products, Inc. shall not be used except as stated in the Maxim Integrated
* Products, Inc. Branding Policy.
*
* The mere transfer of this software does not imply any licenses
* of trade secrets, proprietary technology, copyrights, patents,
* trademarks, maskwork rights, or any other form of intellectual
* property whatsoever. Maxim Integrated Products, Inc. retains all
* ownership rights.
*******************************************************************************/

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "mxc.h"
#include "cnn.h"
#include "weights.h"
#include "camera.h"

#include "sampledata.h"
#include "utils.h"


// Comment out USE_SAMPLEDATA to use Camera module
//#define USE_SAMPLEDATA

#define CAMERA_TO_LCD (1)
#define IMAGE_SIZE_X  (64)
#define IMAGE_SIZE_Y  (64)
#define CAMERA_FREQ   (10 * 1000 * 1000)

#ifdef TFT_ENABLE
#define TFT_BUFF_SIZE   30    // TFT buffer size
int image_bitmap_1 = (int)& img_1_rgb565[0];
int font_1 = (int)& SansSerif16x16[0];
int font_2 = (int)& SansSerif16x16[0];
#endif
const char classes[CNN_NUM_OUTPUTS][10] = {"Cat", "Dog"};

volatile uint32_t cnn_time; // Stopwatch
uint32_t input_0_camera[1024];
uint32_t input_1_camera[1024];
uint32_t input_2_camera[1024];

void fail(void)
{
    printf("\n*** FAIL ***\n\n");
    while (1);
}

#ifdef USE_SAMPLEDATA
// Data input: CHW 3x64x64 (12288 bytes total / 4096 bytes per channel):
static const uint32_t input_0[] = SAMPLE_INPUT_0;
static const uint32_t input_1[] = SAMPLE_INPUT_16;
static const uint32_t input_2[] = SAMPLE_INPUT_32;
#endif

/* **************************************************************************** */

void cnn_load_input(void)
{
#ifdef USE_SAMPLEDATA
    const uint32_t* in0 = input_0;
    const uint32_t* in1 = input_1;
    const uint32_t* in2 = input_2;
#else
    const uint32_t* in0 = input_0_camera;
    const uint32_t* in1 = input_1_camera;
    const uint32_t* in2 = input_2_camera;
#endif

   	memcpy32((uint32_t *) 0x51800000, in0, 1024);
   	memcpy32((uint32_t *) 0x52800000, in1, 1024);
   	memcpy32((uint32_t *) 0x53800000, in2, 1024);
}

// Classification layer:
static int32_t ml_data[CNN_NUM_OUTPUTS];
static q15_t ml_softmax[CNN_NUM_OUTPUTS];

void softmax_layer(void)
{
    cnn_unload((uint32_t*) ml_data);
    softmax_q17p14_q15((const q31_t*) ml_data, CNN_NUM_OUTPUTS, ml_softmax);
}

/* **************************************************************************** */
static uint8_t signed_to_unsigned(int8_t val)
{
    uint8_t value;

    if (val < 0) {
        value = ~val + 1;
        return (128 - value);
    }

    return val + 128;
}

/* **************************************************************************** */
int8_t unsigned_to_signed(uint8_t val)
{
    return val - 128;
}
#ifdef TFT_ENABLE
/* **************************************************************************** */
void TFT_Print(char* str, int x, int y, int font, int length)
{
    // fonts id
    text_t text;
    text.data = str;
    text.len = length;
    MXC_TFT_PrintFont(x, y, font, &text, NULL);
}

#define X_OFFSET    47
#define Y_OFFSET    15
#define SCALE       2.2

/* **************************************************************************** */
void lcd_show_sampledata(uint32_t* data0, uint32_t* data1, uint32_t* data2, int length)
{
    int i;
    int j;
    int x;
    int y;
    int r;
    int g;
    int b;
    int scale = SCALE;

    uint32_t color;
    uint8_t* ptr0;
    uint8_t* ptr1;
    uint8_t* ptr2;

    x = X_OFFSET;
    y = Y_OFFSET;

    for (i = 0; i < length; i++) {
        ptr0 = (uint8_t*)&data0[i];
        ptr1 = (uint8_t*)&data1[i];
        ptr2 = (uint8_t*)&data2[i];

        for (j = 0; j < 4; j++) {
            r = ptr0[j];
            g = ptr1[j];
            b = ptr2[j];

            color = RGB(r, g, b); // convert to RGB565

            MXC_TFT_WritePixel(x * scale, y * scale, scale, scale, color);
            x += 1;

            if (x >= (IMAGE_SIZE_X + X_OFFSET)) {
                x = X_OFFSET;
                y += 1;

                if ((y + 6) >= (IMAGE_SIZE_Y + Y_OFFSET)) {
                    return;
                }
            }
        }
    }
}
#endif
/* **************************************************************************** */
void process_camera_img(uint32_t* data0, uint32_t* data1, uint32_t* data2)
{
    uint8_t* frame_buffer;
    uint32_t imgLen;
    uint32_t w, h, x, y;
    uint8_t* ptr0;
    uint8_t* ptr1;
    uint8_t* ptr2;
    uint8_t* buffer;

    camera_get_image(&frame_buffer, &imgLen, &w, &h);
    ptr0 = (uint8_t*)data0;
    ptr1 = (uint8_t*)data1;
    ptr2 = (uint8_t*)data2;
    buffer = frame_buffer;

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++, ptr0++, ptr1++, ptr2++) {
            *ptr0 = (*buffer);
            buffer++;
            *ptr1 = (*buffer);
            buffer++;
            *ptr2 = (*buffer);
            buffer++;
        }
    }
}

/* **************************************************************************** */
void capture_camera_img(void)
{
    camera_start_capture_image();

    while (1) {
        if (camera_is_image_rcv()) {
            return;
        }
    }
}

/* **************************************************************************** */
void convert_img_unsigned_to_signed(uint32_t* data0, uint32_t* data1, uint32_t* data2)
{
    uint8_t* ptr0;
    uint8_t* ptr1;
    uint8_t* ptr2;
    ptr0 = (uint8_t*)data0;
    ptr1 = (uint8_t*)data1;
    ptr2 = (uint8_t*)data2;

    for (int i = 0; i < 4096; i++) {
        *ptr0 = unsigned_to_signed(*ptr0);
        ptr0++;
        *ptr1 = unsigned_to_signed(*ptr1);
        ptr1++;
        *ptr2 = unsigned_to_signed(*ptr2);
        ptr2++;
    }
}

/* **************************************************************************** */
void convert_img_signed_to_unsigned(uint32_t* data0, uint32_t* data1, uint32_t* data2)
{
    uint8_t* ptr0;
    uint8_t* ptr1;
    uint8_t* ptr2;
    ptr0 = (uint8_t*)data0;
    ptr1 = (uint8_t*)data1;
    ptr2 = (uint8_t*)data2;

    for (int i = 0; i < 4096; i++) {
        *ptr0 = signed_to_unsigned(*ptr0);
        ptr0++;
        *ptr1 = signed_to_unsigned(*ptr1);
        ptr1++;
        *ptr2 = signed_to_unsigned(*ptr2);
        ptr2++;
    }
}

/* **************************************************************************** */
int main(void)
{
    int i;
    int digs, tens;
    int result[CNN_NUM_OUTPUTS];
#ifndef USE_SAMPLEDATA
    int ret = 0, dma_channel;
#endif
#ifdef TFT_ENABLE
    char buff[TFT_BUFF_SIZE];
#endif

    printf("\n\nCats-vs-Dogs Evkit Demo\n");

    /* Enable cache */
    MXC_ICC_Enable(MXC_ICC0);

    /* Switch to 100 MHz clock */
    MXC_SYS_Clock_Select(MXC_SYS_CLOCK_IPO);
    MXC_GCR->ipll_ctrl |= MXC_F_GCR_IPLL_CTRL_EN; // Enable IPLL
    SystemCoreClockUpdate();

    printf("Waiting...\n");

    // DO NOT DELETE THIS LINE:
    MXC_Delay(SEC(2)); // Let debugger interrupt if needed

    /* Enable peripheral, enable CNN interrupt, turn on CNN clock */
    cnn_enable(MXC_S_GCR_PCLKDIV_CNNCLKSEL_IPLL, MXC_S_GCR_PCLKDIV_CNNCLKDIV_DIV1);


    /* Bring CNN state machine into consistent state */
    cnn_init();
    /* Load CNN kernels */
    cnn_load_weights();
    /* Load CNN bias */
    cnn_load_bias();
    /* Configure CNN state machine */
    cnn_configure();

#ifdef TFT_ENABLE
    /* Initialize TFT display */
    printf("Init LCD.\n");
    /* Initialize TFT display */
    MXC_TFT_Init(MXC_SPI0, 1, NULL, NULL);
    MXC_TFT_SetRotation(ROTATE_270);
    MXC_TFT_ShowImage(0, 0, image_bitmap_1);
    MXC_TFT_SetForeGroundColor(WHITE);   // set chars to white

    MXC_Delay(1000000);
#endif

#ifndef USE_SAMPLEDATA
    // Initialize DMA for camera interface
    MXC_DMA_Init();
    dma_channel = MXC_DMA_AcquireChannel();

    // Initialize camera.
    printf("Init Camera.\n");
    camera_init(CAMERA_FREQ);

    ret = camera_setup(IMAGE_SIZE_X, IMAGE_SIZE_Y, PIXFORMAT_RGB888, FIFO_THREE_BYTE, USE_DMA, dma_channel);

    if (ret != STATUS_OK) {
        printf("Error returned from setting up camera. Error %d\n", ret);
        return -1;
    }
#endif

#ifdef TFT_ENABLE

    MXC_TFT_SetBackGroundColor(4);

    memset(buff, 32, TFT_BUFF_SIZE);
    TFT_Print(buff, 55, 50, font_2, sprintf(buff, "ANALOG DEVICES"));
    TFT_Print(buff, 55, 90, font_1, sprintf(buff, "Cats-vs-Dogs Demo"));
    TFT_Print(buff, 55, 130, font_2, sprintf(buff, "PRESS PB1 TO START!"));
#endif

    int frame = 0;

    while (1) {
        printf("********** Press PB1 to capture an image **********\r\n");

        while (!PB_Get(0));

#ifdef TFT_ENABLE
        MXC_TFT_ClearScreen();

        TFT_Print(buff, 55, 110, font_2, sprintf(buff, "CAPTURING IMAGE...."));
#endif

#ifdef USE_SAMPLEDATA
        // Copy the sampledata reference to the camera buffer as a test.
        printf("\nCapturing sampledata %d times\n", ++frame);
        memcpy32(input_0_camera, input_0, 1024);
        memcpy32(input_1_camera, input_1, 1024);
        memcpy32(input_2_camera, input_2, 1024);
        convert_img_signed_to_unsigned(input_0_camera, input_1_camera, input_2_camera);
#else
        // Capture a single camera frame.
        printf("\nCapture a camera frame %d\n", ++frame);
        capture_camera_img();
        // Copy the image data to the CNN input arrays.
        printf("Copy camera frame to CNN input buffers.\n");
        process_camera_img(input_0_camera, input_1_camera, input_2_camera);
#endif

#ifdef TFT_ENABLE
        // Show the input data on the lcd.
        MXC_TFT_ClearScreen();
   //     MXC_TFT_ShowImage(1, 1, image_bitmap_2);
        printf("Show camera frame on LCD.\n");
        lcd_show_sampledata(input_0_camera, input_1_camera, input_2_camera, 1024);
#endif

        convert_img_unsigned_to_signed(input_0_camera, input_1_camera, input_2_camera);

        // Enable CNN clock
        MXC_SYS_ClockEnable(MXC_SYS_PERIPH_CLOCK_CNN);

        cnn_init(); // Bring state machine into consistent state
//        cnn_load_weights(); // No need to reload kernels
        cnn_load_bias(); // No need to reload bias
        cnn_configure(); // Configure state machine
        cnn_load_input();
        cnn_start();


        while (cnn_time == 0) {
            __WFI();    // Wait for CNN interrupt
        }

        // Unload CNN data
        softmax_layer();

        cnn_stop();
        // Disable CNN clock to save power
        MXC_SYS_ClockDisable(MXC_SYS_PERIPH_CLOCK_CNN);

        printf("Time for CNN: %d us\n\n", cnn_time);

        printf("Classification results:\n");

        for (i = 0; i < CNN_NUM_OUTPUTS; i++) {
            digs = (1000 * ml_softmax[i] + 0x4000) >> 15;
            tens = digs % 10;
            digs = digs / 10;
            result[i] = digs;
            printf("[%7d] -> Class %d %8s: %d.%d%%\r\n", ml_data[i], i, classes[i], result[i], tens);
        }

        printf("\n");

#ifdef TFT_ENABLE
        memset(buff, 32, TFT_BUFF_SIZE);
        TFT_Print(buff, 10, 150, font_1, sprintf(buff, "Image Detected : "));
        memset(buff, 0, TFT_BUFF_SIZE);
        TFT_Print(buff, 10, 180, font_1, sprintf(buff, "Probability : "));
        memset(buff, 32, TFT_BUFF_SIZE);

        if (result[0] > result[1]) {
            TFT_Print(buff, 195, 150, font_1, sprintf(buff, "CAT"));
            TFT_Print(buff, 135, 180, font_1, sprintf(buff, "%d%%", result[0]));
        }
        else if (result[1] > result[0]) {
            TFT_Print(buff, 195, 150, font_1, sprintf(buff, "DOG"));
            TFT_Print(buff, 135, 180, font_1, sprintf(buff, "%d%%", result[1]));
        }
        else {
            TFT_Print(buff, 195, 150, font_1, sprintf(buff, "Unknown"));
            memset(buff, 32, TFT_BUFF_SIZE);
            TFT_Print(buff, 135, 180, font_1, sprintf(buff, "NA"));
        }

        TFT_Print(buff, 10, 210, font_1, sprintf(buff, "PRESS PB1 TO CAPTURE IMAGE"));
#endif
#ifdef USE_SAMPLEDATA
	MXC_Delay(SEC(1));
#endif

    }

    return 0;
}
