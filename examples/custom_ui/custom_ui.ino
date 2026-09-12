/**
 * @file      custom_ui.ino
 * @brief     Minimal T-Deck-MAX UI scaffold: EPD + touch + LVGL + screen manager.
 *
 * Trimmed from examples/factory. Brings up only what a UI needs:
 *   XL9555 IO expander -> touch reset -> EPD -> LVGL.
 *
 * Deliberately NOT initialised here (copy from examples/factory when needed):
 *   LoRa, GPS, A7682E 4G, ES8311 audio, SD card, BHI260AP IMU, DRV2605 motor,
 *   TCA8418 keypad, SY6970/BQ27220 battery.
 */

#include <Arduino.h>
#include "utilities.h"
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include "lvgl.h"
#include "ExtensionIOXL9555.hpp"
#include <HynTouch.h>
#include "custom_ui.h"

/*********************************************************************************
 *                                  GLOBALS
 * *******************************************************************************/
// GDEQ031T10 240x320, UC8253
GxEPD2_BW<GxEPD2_310_GDEQ031T10, GxEPD2_310_GDEQ031T10::HEIGHT>
    display(GxEPD2_310_GDEQ031T10(BOARD_EPD_CS, BOARD_EPD_DC, BOARD_EPD_RST, BOARD_EPD_BUSY));

ExtensionIOXL9555 xl9555_io;

#define DISP_REFR_MODE_FULL 0
#define DISP_REFR_MODE_PART 1

static uint8_t   *decodebuffer = NULL;
static int        disp_refr_mode = DISP_REFR_MODE_PART;
static lv_disp_t *custom_ui_disp = NULL;
static uint32_t   disp_partial_refr_until_ms = 0;

/*********************************************************************************
 *                             EPD / LVGL PLUMBING
 * *******************************************************************************/
// LORA, SD and EPD share one SPI bus. Park every CS high before touching the bus
// so an unselected peripheral cannot corrupt the transfer.
static void epd_prepare_bus(void)
{
    pinMode(BOARD_LORA_CS, OUTPUT);
    digitalWrite(BOARD_LORA_CS, HIGH);
    pinMode(BOARD_SD_CS, OUTPUT);
    digitalWrite(BOARD_SD_CS, HIGH);
    pinMode(BOARD_EPD_CS, OUTPUT);
    digitalWrite(BOARD_EPD_CS, HIGH);
}

// LV_COLOR_DEPTH is 1 (see config/lv_conf.h), so each lv_color_t is one byte
// holding one pixel. GxEPD2 wants packed 1bpp rows, MSB first.
static uint32_t pack_lvgl_area_to_epd(const lv_color_t *color_p, uint32_t w, uint32_t h)
{
    const uint32_t bytes_per_row = (w + 7U) / 8U;
    uint32_t epd_idx = 0;

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; x += 8U) {
            uint8_t byte = 0;

            for (uint32_t bit = 0; bit < 8U; ++bit) {
                const uint32_t src_x = x + bit;
                if ((src_x < w) && color_p[(y * w) + src_x].full) {
                    byte |= (0x80U >> bit);
                }
            }

            decodebuffer[epd_idx++] = byte;
        }
    }

    return bytes_per_row * h;
}

static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    const uint32_t w = (uint32_t)(area->x2 - area->x1 + 1);
    const uint32_t h = (uint32_t)(area->y2 - area->y1 + 1);

    (void)pack_lvgl_area_to_epd(color_p, w, h);

    epd_prepare_bus();

    if (disp_refr_mode == DISP_REFR_MODE_PART) {
        display.setPartialWindow(area->x1, area->y1, w, h);
    } else {
        display.setFullWindow();
    }

    display.firstPage();
    do {
        display.drawInvertedBitmap(area->x1, area->y1, decodebuffer, w, h, GxEPD_BLACK);
    } while (display.nextPage());

    // One full refresh per request: fall back to partial until asked again.
    disp_refr_mode = DISP_REFR_MODE_PART;

    /*IMPORTANT!!! Inform the graphics library that you are ready with the flushing*/
    lv_disp_flush_ready(disp_drv);
}

static void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;

    uint8_t touched = hyn_touch_get_point(&last_x, &last_y, 1);
    data->state = touched ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;

    /*Set the last pressed coordinates*/
    data->point.x = last_x;
    data->point.y = last_y;
}

static void lvgl_init(void)
{
    lv_init();

    static lv_disp_draw_buf_t draw_buf_dsc;
    lv_color_t *buf_1 = (lv_color_t *)ps_calloc(sizeof(lv_color_t), DISP_BUF_SIZE);
    lv_color_t *buf_2 = (lv_color_t *)ps_calloc(sizeof(lv_color_t), DISP_BUF_SIZE);
    lv_disp_draw_buf_init(&draw_buf_dsc, buf_1, buf_2, LCD_HOR_SIZE * LCD_VER_SIZE);
    decodebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), DISP_BUF_SIZE);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res     = LCD_HOR_SIZE;
    disp_drv.ver_res     = LCD_VER_SIZE;
    disp_drv.flush_cb    = disp_flush;
    disp_drv.draw_buf    = &draw_buf_dsc;
    disp_drv.full_refresh = 1;
    custom_ui_disp = lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    lv_indev_drv_register(&indev_drv);
}

/*********************************************************************************
 *                              REFRESH CONTROL
 * *******************************************************************************/
void disp_full_refr(void)
{
    lv_disp_t *disp = custom_ui_disp ? custom_ui_disp : lv_disp_get_default();
    if (disp != NULL && disp->driver != NULL) {
        disp->driver->full_refresh = 1;
    }
    disp_partial_refr_until_ms = 0U;
    disp_refr_mode = DISP_REFR_MODE_FULL;
}

void disp_partial_refr_for(uint32_t duration_ms)
{
    lv_disp_t *disp = custom_ui_disp ? custom_ui_disp : lv_disp_get_default();
    if (disp != NULL && disp->driver != NULL) {
        disp->driver->full_refresh = 0;
    }
    disp_refr_mode = DISP_REFR_MODE_PART;
    disp_partial_refr_until_ms = millis() + duration_ms;
}

/*********************************************************************************
 *                                   SETUP
 * *******************************************************************************/
static bool ink_screen_init(void)
{
    epd_prepare_bus();
    display.init(115200, true, 2, false);
    display.setRotation(0);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
    } while (display.nextPage());
    display.powerOff();
    return true;
}

void setup()
{
    setCpuFrequencyMhz(240);
    Serial.begin(115200);
#if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
    // Keep the UI responsive after the USB serial monitor is closed.
    Serial.setTxTimeoutMs(0);
#endif

    // LORA, SD and EPD use the same SPI, in order to avoid mutual influence;
    // before powering on, all CS signals should be pulled high and in an unselected state;
    epd_prepare_bus();
    pinMode(BOARD_LORA_RST, OUTPUT);
    digitalWrite(BOARD_LORA_RST, HIGH);

    Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);

    // Every power rail and reset line on this board hangs off the XL9555, so it
    // has to come up before anything else can be talked to.
    Serial.println(" -------------------------- XL9555 -------------------------- ");
    if (!xl9555_io.init(Wire, BOARD_I2C_SDA, BOARD_I2C_SCL, XL9555_SLAVE_ADDRESS0)) {
        while (1) {
            Serial.println("Failed to find XL9555 - check your wiring!");
            delay(1000);
        }
    }

    const uint8_t expands[] = {
        BOARD_XL9555_00_6609_EN,
        BOARD_XL9555_01_LORA_EN,
        BOARD_XL9555_02_GPS_EN,
        BOARD_XL9555_03_1V8_EN,
        BOARD_XL9555_04_LORA_SEL,
        BOARD_XL9555_05_MOTOR_EN,
        BOARD_XL9555_06_AMPLIFIER,
        BOARD_XL9555_07_TOUCH_RST,
        BOARD_XL9555_10_PWRKEY_EN,
        BOARD_XL9555_11_KEY_RST,
        BOARD_XL9555_12_AUDIO_SEL,
    };
    for (auto pin : expands) {
        xl9555_io.pinMode(pin, OUTPUT);
        xl9555_io.digitalWrite(pin, HIGH);
        delay(1);
    }

    // Ensure touch controller exits any "half-powered" state after a power cut:
    // provide a deterministic reset pulse via XL9555 IO07 before touch init.
    xl9555_io.digitalWrite(BOARD_XL9555_07_TOUCH_RST, LOW);
    delay(20);
    xl9555_io.digitalWrite(BOARD_XL9555_07_TOUCH_RST, HIGH);
    delay(60);

    hyn_touch_attach_xl9555(&xl9555_io);

    // Front light. Drop to 0 to save power once your UI is readable without it.
    analogWrite(BOARD_EPD_BL, 50);

    SPI.begin(BOARD_SPI_SCK, BOARD_SPI_MISO, BOARD_SPI_MOSI);

    Serial.println(" -------------------------- EPD -------------------------- ");
    if (!ink_screen_init()) {
        Serial.println("Failed to init the e-paper display");
    }

    Serial.println(" -------------------------- TOUCH -------------------------- ");
    if (!hyn_touch_init()) {
        Serial.println("Failed to find the touch controller - UI will be display only");
    }

    lvgl_init();
    custom_ui_entry();
    disp_full_refr();

    Serial.println("custom_ui ready");
}

void loop()
{
    lv_task_handler();

    // Partial-refresh windows are time limited: once the window expires, go back
    // to full refreshes so accumulated ghosting gets cleared.
    if (disp_partial_refr_until_ms > 0U && millis() >= disp_partial_refr_until_ms) {
        lv_disp_t *disp = custom_ui_disp ? custom_ui_disp : lv_disp_get_default();
        if (disp != NULL && disp->driver != NULL) {
            disp->driver->full_refresh = 1;
        }
        disp_partial_refr_until_ms = 0U;
    }

    delay(1);
}
