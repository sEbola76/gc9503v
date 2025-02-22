#include "gc9503cv.h"
#include "esphome/core/log.h"

namespace esphome {
namespace gc9503cv {

void GC9503CV::setup() {
	esph_log_config(TAG, "Setting up GC9503CV");
	if (this->reset_pin_ != nullptr) {
		esph_log_config(TAG, "reset pin INIT");
		this->reset_pin_->setup();
		this->reset_pin_->digital_write(true);
		delay(10);
		this->reset_pin_->digital_write(false);
		delay(50);
		this->reset_pin_->digital_write(true);
		delay(100);
	}

	spi_bus_config_t buscfg = {};
	buscfg.sclk_io_num = this->sclk_pin_->get_pin();
	buscfg.mosi_io_num = this->mosi_pin_->get_pin();
	buscfg.miso_io_num = -1;
	buscfg.quadwp_io_num = -1;
	buscfg.quadhd_io_num = -1;
	buscfg.max_transfer_sz = 10 * 1024;

	ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

	spi_device_interface_config_t devcfg = {};
	devcfg.clock_speed_hz = SPI_MASTER_FREQ_10M;   		//Clock out at 10 MHz
	devcfg.mode = 0;                               		//SPI mode 0
	devcfg.spics_io_num = this->enable_pin_->get_pin();	//CS pin
	devcfg.queue_size = 7;                         		//We want to be able to queue 7 transactions at a time


	ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &g_screen_spi));
	esph_log_config(TAG, "rgb_driver_init");
	rgb_driver_init();

	spi_bus_remove_device(g_screen_spi);
	spi_bus_free(SPI2_HOST);
	esph_log_config(TAG, "spi removed");
	if (this->enable_pin_ != nullptr) {
		esph_log_config(TAG, "enable pin INIT");
		this->enable_pin_->setup();
		this->enable_pin_->digital_write(true);
	}

	esp_lcd_rgb_panel_config_t config{};
	config.flags.refresh_on_demand = 0;   // Mannually control refresh operation

	config.flags.fb_in_psram = 1;
#if ESP_IDF_VERSION_MAJOR >= 5
 	config.bounce_buffer_size_px = this->width_ * 10;
	config.num_fbs = 1;
#endif  // ESP_IDF_VERSION_MAJOR
	config.timings.h_res = this->width_;
	config.timings.v_res = this->height_;
	config.timings.hsync_pulse_width = this->hsync_pulse_width_;
	config.timings.hsync_back_porch = this->hsync_back_porch_;
	config.timings.hsync_front_porch = this->hsync_front_porch_;
	config.timings.vsync_pulse_width = this->vsync_pulse_width_;
	config.timings.vsync_back_porch = this->vsync_back_porch_;
	config.timings.vsync_front_porch = this->vsync_front_porch_;
	config.timings.flags.pclk_active_neg = this->pclk_inverted_;
	config.timings.pclk_hz = this->pclk_frequency_;
	config.clk_src = LCD_CLK_SRC_PLL160M;
	config.psram_trans_align = 64;
	size_t data_pin_count = sizeof(this->data_pins_) / sizeof(this->data_pins_[0]);
	for (size_t i = 0; i != data_pin_count; i++) {
		config.data_gpio_nums[i] = this->data_pins_[i]->get_pin();
	}
	config.data_width = data_pin_count;
	config.disp_gpio_num = -1;
	config.hsync_gpio_num = this->hsync_pin_->get_pin();
	config.vsync_gpio_num = this->vsync_pin_->get_pin();
	config.de_gpio_num = this->de_pin_->get_pin();
	config.pclk_gpio_num = this->pclk_pin_->get_pin();
// 	esp_err_t err = esp_lcd_new_rgb_panel(&config, &this->handle_);
// 	if (err != ESP_OK) {
// 		ESP_LOGE(TAG, "lcd_new_rgb_panel failed: %s", esp_err_to_name(err));
// 		this->mark_failed();
// 		return;
// 	}
//   ESP_ERROR_CHECK(esp_lcd_panel_reset(this->handle_));
//   ESP_ERROR_CHECK(esp_lcd_panel_init(this->handle_));

    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&config, &this->handle_));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(this->handle_));
    ESP_ERROR_CHECK(esp_lcd_panel_init(this->handle_));
	
	esph_log_config(TAG, "GC9503CV setup complete");
}

void GC9503CV::__spi_send_cmd(uint8_t cmd)
{
    uint16_t tmp_cmd = (cmd | 0x0000);;
    spi_transaction_ext_t trans = (spi_transaction_ext_t)
    {
        .base =
        {
            .flags = SPI_TRANS_VARIABLE_CMD,
            .cmd = tmp_cmd,
        },
        .command_bits = 9,
    };
    spi_device_transmit(g_screen_spi, (spi_transaction_t *)&trans);
}

void GC9503CV::__spi_send_data(uint8_t data)
{
    uint16_t tmp_data = (data | 0x0100);
    spi_transaction_ext_t trans = (spi_transaction_ext_t){
        .base = {
            .flags = SPI_TRANS_VARIABLE_CMD,
            .cmd = tmp_data,
        },
        .command_bits = 9,
    };
    spi_device_transmit(g_screen_spi, (spi_transaction_t *)&trans);
}

void GC9503CV::loop() {
#if ESP_IDF_VERSION_MAJOR >= 5
	if (this->handle_ != nullptr)
		esp_lcd_rgb_panel_restart(this->handle_);
#endif  // ESP_IDF_VERSION_MAJOR
}

void GC9503CV::draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order, display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) {
	if (w <= 0 || h <= 0)
		return;
	// if color mapping is required, pass the buck.
	// note that endianness is not considered here - it is assumed to match!
	if (bitness != display::COLOR_BITNESS_565) {
		return display::Display::draw_pixels_at(x_start, y_start, w, h, ptr, order, bitness, big_endian, x_offset, y_offset, x_pad);
	}
	x_start += this->offset_x_;
	y_start += this->offset_y_;
	esp_err_t err = ESP_OK;
  // x_ and y_offset are offsets into the source buffer, unrelated to our own offsets into the display.
	if (x_offset == 0 && x_pad == 0 && y_offset == 0) {
    	// we could deal here with a non-zero y_offset, but if x_offset is zero, y_offset probably will be so don't bother
		err = esp_lcd_panel_draw_bitmap(this->handle_, x_start, y_start, x_start + w, y_start + h, ptr);
	} else {
		// draw line by line
		auto stride = x_offset + w + x_pad;
		for (int y = 0; y != h; y++) {
			err = esp_lcd_panel_draw_bitmap(this->handle_, x_start, y + y_start, x_start + w, y + y_start + 1, ptr + ((y + y_offset) * stride + x_offset) * 2);
			if (err != ESP_OK)
				break;
		}
	}
	if (err != ESP_OK)
	ESP_LOGE(TAG, "lcd_lcd_panel_draw_bitmap failed: %s", esp_err_to_name(err));
}

int GC9503CV::get_width() {
	switch (this->rotation_) {
		case display::DISPLAY_ROTATION_90_DEGREES:
		case display::DISPLAY_ROTATION_270_DEGREES:
			return this->get_height_internal();
		default:
			return this->get_width_internal();
	}
}

int GC9503CV::get_height() {
	switch (this->rotation_) {
		case display::DISPLAY_ROTATION_90_DEGREES:
		case display::DISPLAY_ROTATION_270_DEGREES:
			return this->get_width_internal();
		default:
			return this->get_height_internal();
	}
}


void GC9503CV::draw_pixel_at(int x, int y, Color color) {
	if (!this->get_clipping().inside(x, y))
		return;  // NOLINT

	switch (this->rotation_) {
		case display::DISPLAY_ROTATION_0_DEGREES:
			break;
		case display::DISPLAY_ROTATION_90_DEGREES:
			std::swap(x, y);
			x = this->width_ - x - 1;
			break;
		case display::DISPLAY_ROTATION_180_DEGREES:
			x = this->width_ - x - 1;
			y = this->height_ - y - 1;
			break;
		case display::DISPLAY_ROTATION_270_DEGREES:
			std::swap(x, y);
			y = this->height_ - y - 1;
			break;
	}
	auto pixel = convert_big_endian(display::ColorUtil::color_to_565(color));

	this->draw_pixels_at(x, y, 1, 1, (const uint8_t *) &pixel, display::COLOR_ORDER_RGB, display::COLOR_BITNESS_565, true, 0, 0, 0);
	App.feed_wdt();
}

void GC9503CV::dump_config() {
	ESP_LOGCONFIG("", "GC9503CV RGB LCD");
	ESP_LOGCONFIG(TAG, "  Height: %u", this->height_);
	ESP_LOGCONFIG(TAG, "  Width: %u", this->width_);
	LOG_PIN("  DE Pin: ", this->de_pin_);
	LOG_PIN("  Enable Pin: ", this->enable_pin_);
	LOG_PIN("  Reset Pin: ", this->reset_pin_);
	size_t data_pin_count = sizeof(this->data_pins_) / sizeof(this->data_pins_[0]);
	for (size_t i = 0; i != data_pin_count; i++)
		ESP_LOGCONFIG(TAG, "  Data pin %d: %s", i, (this->data_pins_[i])->dump_summary().c_str());
}

void GC9503CV::reset_display_() const {
	esph_log_config(TAG, "reset display");
	if (this->reset_pin_ != nullptr) {
		this->reset_pin_->setup();
		this->reset_pin_->digital_write(false);
		delay(1);
		this->reset_pin_->digital_write(true);
	}
}

void GC9503CV::rgb_driver_init()
{	//based on : https://github.com/VIEWESMART/UEDX48480021-MD80ESP32_2.1inch-Knob/blob/main/examples/ESP-IDF/UEDX48480021-MD80E-SDK/components/bsp/sub_board/bsp_lcd.c

	//__spi_send_cmd(0xB1); __spi_send_data(0x30);	//CMD_MADCTL (BGR:0x010 | 0x020 = 0x030 ?) (RGB: 0x010 & !0x20 = 0x10)

	__spi_send_cmd (0xF0); 
		__spi_send_data (0x55); __spi_send_data (0xAA); __spi_send_data (0x52); __spi_send_data (0x08); __spi_send_data (0x00);
	__spi_send_cmd (0xF6); __spi_send_data (0x5A); __spi_send_data (0x87); 
	__spi_send_cmd (0xC1); __spi_send_data (0x3F);

	__spi_send_cmd (0xCD); __spi_send_data (0x25);
	__spi_send_cmd (0xC9); __spi_send_data (0x10);
	__spi_send_cmd (0xF8); __spi_send_data (0x8A);
	__spi_send_cmd (0xAC); __spi_send_data (0x45);
	__spi_send_cmd (0xA7); __spi_send_data (0x47);
	__spi_send_cmd (0xA0); __spi_send_data (0xDD);
	__spi_send_cmd (0x87); 
		__spi_send_data (0x04); __spi_send_data (0x03); __spi_send_data (0x66);
	__spi_send_cmd (0x86); 
		__spi_send_data (0x99); __spi_send_data (0xa3); __spi_send_data (0xa3); __spi_send_data (0x51);
	__spi_send_cmd (0xFA);
		__spi_send_data (0x08); __spi_send_data (0x08); __spi_send_data (0x08); __spi_send_data (0x04);
	__spi_send_cmd (0x9A); __spi_send_data (0x8a);
	__spi_send_cmd (0x9B); __spi_send_data (0x62);
	__spi_send_cmd (0x82); 
		__spi_send_data (0x48); __spi_send_data (0x48);
	__spi_send_cmd (0xB1); __spi_send_data (0x10);
	__spi_send_cmd (0x7A); __spi_send_data (0x13); __spi_send_data (0x1a);
	__spi_send_cmd (0x7B); __spi_send_data (0x13); __spi_send_data (0x1a);
	
	__spi_send_cmd (0x6D); 
	/*
		0x1e, 0x1e, 0x1e, 0x1e, 
		0x1e, 0x1e, 0x02, 0x0b, 
		0x01, 0x00, 0x1f, 0x1e, 
		0x09, 0x0f, 0x1e, 0x1e, 
		
		0x1e, 0x1e, 0x10, 0x0a, 
		0x1e, 0x1f, 0x00, 0x08, 
		0x0b, 0x02, 0x1e, 0x1e, 
		0x1e, 0x1e, 0x1e, 0x1e
	*/
		__spi_send_data (0x1e); __spi_send_data (0x1e); __spi_send_data (0x1e); __spi_send_data (0x1e);
		__spi_send_data (0x1e); __spi_send_data (0x1e); __spi_send_data (0x02); __spi_send_data (0x0b);
		__spi_send_data (0x01); __spi_send_data (0x00); __spi_send_data (0x1f); __spi_send_data (0x1e);
		__spi_send_data (0x09); __spi_send_data (0x0f); __spi_send_data (0x1e); __spi_send_data (0x1e);

		__spi_send_data (0x1e); __spi_send_data (0x1e); __spi_send_data (0x10); __spi_send_data (0x0a);
		__spi_send_data (0x1e); __spi_send_data (0x1f); __spi_send_data (0x00); __spi_send_data (0x08);
		__spi_send_data (0x0b); __spi_send_data (0x02); __spi_send_data (0x1e); __spi_send_data (0x1e);
		__spi_send_data (0x1e); __spi_send_data (0x1e); __spi_send_data (0x1e); __spi_send_data (0x1e);
	
	__spi_send_cmd (0x64); 
	/*
		0x18, 0x07, 0x01, 0xE7, 
		0x03, 0x03, 0x18, 0x06, 
		0x01, 0xE6, 0x03, 0x03, 
		0x7a, 0x7a, 0x7a, 0x7a
	*/
		__spi_send_data (0x18); __spi_send_data (0x07); __spi_send_data (0x01); __spi_send_data (0xe7);
		__spi_send_data (0x03); __spi_send_data (0x03); __spi_send_data (0x18); __spi_send_data (0x06);
		__spi_send_data (0x01); __spi_send_data (0xe6); __spi_send_data (0x03); __spi_send_data (0x03);
		__spi_send_data (0x7a); __spi_send_data (0x7a);	__spi_send_data (0x7a); __spi_send_data (0x7a);
	
	__spi_send_cmd (0x65); 
	/*
		0x58, 0x26, 0x18, 0x2c, 
		0x03, 0x03, 0x58, 0x26, 
		0x18, 0x2c, 0x03, 0x03, 
		0x7a, 0x7a, 0x7a, 0x7a
	*/
		__spi_send_data (0x58); __spi_send_data (0x26); __spi_send_data (0x18); __spi_send_data (0x2c);
		__spi_send_data (0x03); __spi_send_data (0x03); __spi_send_data (0x58); __spi_send_data (0x26);
		__spi_send_data (0x18); __spi_send_data (0x2c); __spi_send_data (0x03); __spi_send_data (0x03);
		__spi_send_data (0x7a); __spi_send_data (0x7a); __spi_send_data (0x7a); __spi_send_data (0x7a);
	
	__spi_send_cmd (0x66); 
	/*
		0x58, 0x26, 0x18, 0x2c, 
		0x03, 0x03, 0x58, 0x26, 
		0x18, 0x2c, 0x03, 0x03, 
		0x7a, 0x7a, 0x7a, 0x7a	
	*/
		__spi_send_data (0x58); __spi_send_data (0x26); __spi_send_data (0x18); __spi_send_data (0x2c);
		__spi_send_data (0x03); __spi_send_data (0x03); __spi_send_data (0x58); __spi_send_data (0x26);
		__spi_send_data (0x18); __spi_send_data (0x2c); __spi_send_data (0x03); __spi_send_data (0x03);
		__spi_send_data (0x7a); __spi_send_data (0x7a); __spi_send_data (0x7a); __spi_send_data (0x7a);
	
	__spi_send_cmd (0x67); 
	/*
		0x18, 0x05, 0x01, 0xE5, 
		0x03, 0x03, 0x18, 0x04, 
		0x01, 0xE4, 0x03, 0x03, 
		0x7a, 0x7a, 0x7a, 0x7a
	*/
		__spi_send_data (0x18); __spi_send_data (0x05); __spi_send_data (0x01); __spi_send_data (0xe5);
		__spi_send_data (0x03); __spi_send_data (0x03); __spi_send_data (0x18); __spi_send_data (0x04);
		__spi_send_data (0x01); __spi_send_data (0xe4); __spi_send_data (0x03); __spi_send_data (0x03);
		__spi_send_data (0x7a); __spi_send_data (0x7a); __spi_send_data (0x7a); __spi_send_data (0x7a);
	
	__spi_send_cmd (0x60); 
		__spi_send_data (0x18); __spi_send_data (0x09); __spi_send_data (0x7a); __spi_send_data (0x7a);
		__spi_send_data (0x51); __spi_send_data (0xf1); __spi_send_data (0x7a); __spi_send_data (0x7a);
	__spi_send_cmd (0x63); 
		__spi_send_data (0x51); __spi_send_data (0xf1); __spi_send_data (0x7a); __spi_send_data (0x7a);
		__spi_send_data (0x18); __spi_send_data (0x08); __spi_send_data (0x7a); __spi_send_data (0x7a);
	
	__spi_send_cmd (0xD1); 
	/*
		0x00, 0x00, 0x00, 0x0E, 
		0x00, 0x31, 0x00, 0x4E, 
		0x00, 0x67, 0x00, 0x92, 
		0x00, 0xB5, 0x00, 0xED, 
		0x01, 0x1C, 0x01, 0x66, 
		0x01, 0xA4, 0x02, 0x04, 
		0x02, 0x53, 0x02, 0x56, 
		0x02, 0x9F, 0x02, 0xF3, 
		0x03, 0x29, 0x03, 0x73,
        0x03, 0xA1, 0x03, 0xB9, 
		0x03, 0xC8, 0x03, 0xDB, 
		0x03, 0xE7, 0x03, 0xF4, 
		0x03, 0xFB, 0x03, 0XFF
	*/
		__spi_send_data (0x00); __spi_send_data (0x00); __spi_send_data (0x00); __spi_send_data (0x0E);
		__spi_send_data (0x00); __spi_send_data (0x31); __spi_send_data (0x00); __spi_send_data (0x4E);
		__spi_send_data (0x00); __spi_send_data (0x67); __spi_send_data (0x00); __spi_send_data (0x92);
		__spi_send_data (0x00); __spi_send_data (0xB5); __spi_send_data (0x00); __spi_send_data (0xED);
		__spi_send_data (0x01); __spi_send_data (0x1C); __spi_send_data (0x01); __spi_send_data (0x66);
		__spi_send_data (0x01); __spi_send_data (0xA4); __spi_send_data (0x02); __spi_send_data (0x04);
		__spi_send_data (0x02); __spi_send_data (0x53); __spi_send_data (0x02); __spi_send_data (0x56);
		__spi_send_data (0x02); __spi_send_data (0x9F); __spi_send_data (0x02); __spi_send_data (0xF3);
		__spi_send_data (0x03); __spi_send_data (0x29); __spi_send_data (0x03); __spi_send_data (0x73);
		__spi_send_data (0x03); __spi_send_data (0xA1); __spi_send_data (0x03); __spi_send_data (0xB9);
		__spi_send_data (0x03); __spi_send_data (0xC8); __spi_send_data (0x03); __spi_send_data (0xDB);
		__spi_send_data (0x03); __spi_send_data (0xE7); __spi_send_data (0x03); __spi_send_data (0xF4);
		__spi_send_data (0x03); __spi_send_data (0xFB); __spi_send_data (0x03); __spi_send_data (0xFF);
		
	__spi_send_cmd (0xD2);
	/*
		0x00, 0x00, 0x00, 0x0E, 
		0x00, 0x31, 0x00, 0x4E, 
		0x00, 0x67, 0x00, 0x92, 
		0x00, 0xB5, 0x00, 0xED, 
		
		0x01, 0x1C, 0x01, 0x66,
		0x01, 0xA4, 0x02, 0x04, 
		0x02, 0x53, 0x02, 0x56, 
		0x02, 0x9F, 0x02, 0xF3, 
		
		0x03, 0x29, 0x03, 0x73,
        0x03, 0xA1, 0x03, 0xB9, 
		0x03, 0xC8, 0x03, 0xDB, 
		0x03, 0xE7, 0x03, 0xF4, 
		
		0x03, 0xFB, 0x03, 0XFF
	*/
		__spi_send_data (0x00); __spi_send_data (0x00); __spi_send_data (0x00); __spi_send_data (0x0E);
		__spi_send_data (0x00); __spi_send_data (0x31); __spi_send_data (0x00); __spi_send_data (0x4E);
		__spi_send_data (0x00); __spi_send_data (0x67); __spi_send_data (0x00); __spi_send_data (0x92);
		__spi_send_data (0x00); __spi_send_data (0xB5); __spi_send_data (0x00); __spi_send_data (0xED);

		__spi_send_data (0x01); __spi_send_data (0x1C); __spi_send_data (0x01); __spi_send_data (0x66);
		__spi_send_data (0x01); __spi_send_data (0xA4); __spi_send_data (0x02); __spi_send_data (0x04);
		__spi_send_data (0x02); __spi_send_data (0x53); __spi_send_data (0x02); __spi_send_data (0x56);
		__spi_send_data (0x02); __spi_send_data (0x9F); __spi_send_data (0x02); __spi_send_data (0xF3);

		__spi_send_data (0x03); __spi_send_data (0x29); __spi_send_data (0x03); __spi_send_data (0x73);
		__spi_send_data (0x03); __spi_send_data (0xA1); __spi_send_data (0x03); __spi_send_data (0xB9);
		__spi_send_data (0x03); __spi_send_data (0xC8); __spi_send_data (0x03); __spi_send_data (0xDB);
		__spi_send_data (0x03); __spi_send_data (0xE7); __spi_send_data (0x03); __spi_send_data (0xF4);

		__spi_send_data (0x03); __spi_send_data (0xFB); __spi_send_data (0x03); __spi_send_data (0xFF);

	__spi_send_cmd (0xD3); 
	/*
		0x00, 0x00, 0x00, 0x0E, 
		0x00, 0x31, 0x00, 0x4E, 
		0x00, 0x67, 0x00, 0x92, 
		0x00, 0xB5, 0x00, 0xED, 
		0x01, 0x1C, 0x01, 0x66, 
		0x01, 0xA4, 0x02, 0x04, 
		0x02, 0x53, 0x02, 0x56, 
		0x02, 0x9F, 0x02, 0xF3, 
		0x03, 0x29, 0x03, 0x73,
        0x03, 0xA1, 0x03, 0xB9, 
		0x03, 0xC8, 0x03, 0xDB, 
		0x03, 0xE7, 0x03, 0xF4, 
		0x03, 0xFB, 0x03, 0XFF
	*/
		__spi_send_data (0x00); __spi_send_data (0x00); __spi_send_data (0x00); __spi_send_data (0x0E);
		__spi_send_data (0x00); __spi_send_data (0x31); __spi_send_data (0x00); __spi_send_data (0x4E);
		__spi_send_data (0x00); __spi_send_data (0x67); __spi_send_data (0x00); __spi_send_data (0x92);
		__spi_send_data (0x00); __spi_send_data (0xB5); __spi_send_data (0x00); __spi_send_data (0xED);
		__spi_send_data (0x01); __spi_send_data (0x1C); __spi_send_data (0x01); __spi_send_data (0x66);
		__spi_send_data (0x01); __spi_send_data (0xA4); __spi_send_data (0x02); __spi_send_data (0x04);
		__spi_send_data (0x02); __spi_send_data (0x53); __spi_send_data (0x02); __spi_send_data (0x56);
		__spi_send_data (0x02); __spi_send_data (0x9F); __spi_send_data (0x02); __spi_send_data (0xF3);
		__spi_send_data (0x03); __spi_send_data (0x29); __spi_send_data (0x03); __spi_send_data (0x73);
		__spi_send_data (0x03); __spi_send_data (0xA1); __spi_send_data (0x03); __spi_send_data (0xB9);
		__spi_send_data (0x03); __spi_send_data (0xC8); __spi_send_data (0x03); __spi_send_data (0xDB);
		__spi_send_data (0x03); __spi_send_data (0xE7); __spi_send_data (0x03); __spi_send_data (0xF4);
		__spi_send_data (0x03); __spi_send_data (0xFB); __spi_send_data (0x03); __spi_send_data (0xFF);
	
	__spi_send_cmd (0xD4); 
	/*
		0x00, 0x00, 0x00, 0x0E, 
		0x00, 0x31, 0x00, 0x4E, 
		0x00, 0x67, 0x00, 0x92, 
		0x00, 0xB5, 0x00, 0xED, 
		0x01, 0x1C, 0x01, 0x66, 
		0x01, 0xA4, 0x02, 0x04, 
		0x02, 0x53, 0x02, 0x56, 
		0x02, 0x9F, 0x02, 0xF3, 
		0x03, 0x29, 0x03, 0x73,
        0x03, 0xA1, 0x03, 0xB9, 
		0x03, 0xC8, 0x03, 0xDB, 
		0x03, 0xE7, 0x03, 0xF4, 
		0x03, 0xFB, 0x03, 0XFF
	*/
		__spi_send_data (0x00); __spi_send_data (0x00); __spi_send_data (0x00); __spi_send_data (0x0E);
		__spi_send_data (0x00); __spi_send_data (0x31); __spi_send_data (0x00); __spi_send_data (0x4E);
		__spi_send_data (0x00); __spi_send_data (0x67); __spi_send_data (0x00); __spi_send_data (0x92);
		__spi_send_data (0x00); __spi_send_data (0xB5); __spi_send_data (0x00); __spi_send_data (0xED);
		__spi_send_data (0x01); __spi_send_data (0x1C); __spi_send_data (0x01); __spi_send_data (0x66);
		__spi_send_data (0x01); __spi_send_data (0xA4); __spi_send_data (0x02); __spi_send_data (0x04);
		__spi_send_data (0x02); __spi_send_data (0x53); __spi_send_data (0x02); __spi_send_data (0x56);
		__spi_send_data (0x02); __spi_send_data (0x9F); __spi_send_data (0x02); __spi_send_data (0xF3);
		__spi_send_data (0x03); __spi_send_data (0x29); __spi_send_data (0x03); __spi_send_data (0x73);
		__spi_send_data (0x03); __spi_send_data (0xA1); __spi_send_data (0x03); __spi_send_data (0xB9);
		__spi_send_data (0x03); __spi_send_data (0xC8); __spi_send_data (0x03); __spi_send_data (0xDB);
		__spi_send_data (0x03); __spi_send_data (0xE7); __spi_send_data (0x03); __spi_send_data (0xF4);
		__spi_send_data (0x03); __spi_send_data (0xFB); __spi_send_data (0x03); __spi_send_data (0xFF);

	__spi_send_cmd (0xD5); 
	/*
		0x00, 0x00, 0x00, 0x0E, 
		0x00, 0x31, 0x00, 0x4E, 
		0x00, 0x67, 0x00, 0x92, 
		0x00, 0xB5, 0x00, 0xED, 
		0x01, 0x1C, 0x01, 0x66, 
		0x01, 0xA4, 0x02, 0x04, 
		0x02, 0x53, 0x02, 0x56, 
		0x02, 0x9F, 0x02, 0xF3, 
		0x03, 0x29, 0x03, 0x73,
        0x03, 0xA1, 0x03, 0xB9, 
		0x03, 0xC8, 0x03, 0xDB, 
		0x03, 0xE7, 0x03, 0xF4, 
		0x03, 0xFB, 0x03, 0XFF
	*/
		__spi_send_data (0x00); __spi_send_data (0x00); __spi_send_data (0x00); __spi_send_data (0x0E);
		__spi_send_data (0x00); __spi_send_data (0x31); __spi_send_data (0x00); __spi_send_data (0x4E);
		__spi_send_data (0x00); __spi_send_data (0x67); __spi_send_data (0x00); __spi_send_data (0x92);
		__spi_send_data (0x00); __spi_send_data (0xB5); __spi_send_data (0x00); __spi_send_data (0xED);
		__spi_send_data (0x01); __spi_send_data (0x1C); __spi_send_data (0x01); __spi_send_data (0x66);
		__spi_send_data (0x01); __spi_send_data (0xA4); __spi_send_data (0x02); __spi_send_data (0x04);
		__spi_send_data (0x02); __spi_send_data (0x53); __spi_send_data (0x02); __spi_send_data (0x56);
		__spi_send_data (0x02); __spi_send_data (0x9F); __spi_send_data (0x02); __spi_send_data (0xF3);
		__spi_send_data (0x03); __spi_send_data (0x29); __spi_send_data (0x03); __spi_send_data (0x73);
		__spi_send_data (0x03); __spi_send_data (0xA1); __spi_send_data (0x03); __spi_send_data (0xB9);
		__spi_send_data (0x03); __spi_send_data (0xC8); __spi_send_data (0x03); __spi_send_data (0xDB);
		__spi_send_data (0x03); __spi_send_data (0xE7); __spi_send_data (0x03); __spi_send_data (0xF4);
		__spi_send_data (0x03); __spi_send_data (0xFB); __spi_send_data (0x03); __spi_send_data (0xFF);

	__spi_send_cmd (0xD6); 
		/*
		0x00, 0x00, 0x00, 0x0E, 
		0x00, 0x31, 0x00, 0x4E, 
		0x00, 0x67, 0x00, 0x92, 
		0x00, 0xB5, 0x00, 0xED, 
		0x01, 0x1C, 0x01, 0x66, 
		0x01, 0xA4, 0x02, 0x04, 
		0x02, 0x53, 0x02, 0x56, 
		0x02, 0x9F, 0x02, 0xF3, 
		0x03, 0x29, 0x03, 0x73,
        0x03, 0xA1, 0x03, 0xB9, 
		0x03, 0xC8, 0x03, 0xDB, 
		0x03, 0xE7, 0x03, 0xF4, 
		0x03, 0xFB, 0x03, 0XFF
		*/
		__spi_send_data (0x00); __spi_send_data (0x00); __spi_send_data (0x00); __spi_send_data (0x0E);
		__spi_send_data (0x00); __spi_send_data (0x31); __spi_send_data (0x00); __spi_send_data (0x4E);
		__spi_send_data (0x00); __spi_send_data (0x67); __spi_send_data (0x00); __spi_send_data (0x92);
		__spi_send_data (0x00); __spi_send_data (0xB5); __spi_send_data (0x00); __spi_send_data (0xED);
		__spi_send_data (0x01); __spi_send_data (0x1C); __spi_send_data (0x01); __spi_send_data (0x66);
		__spi_send_data (0x01); __spi_send_data (0xA4); __spi_send_data (0x02); __spi_send_data (0x04);
		__spi_send_data (0x02); __spi_send_data (0x53); __spi_send_data (0x02); __spi_send_data (0x56);
		__spi_send_data (0x02); __spi_send_data (0x9F); __spi_send_data (0x02); __spi_send_data (0xF3);
		__spi_send_data (0x03); __spi_send_data (0x29); __spi_send_data (0x03); __spi_send_data (0x73);
		__spi_send_data (0x03); __spi_send_data (0xA1); __spi_send_data (0x03); __spi_send_data (0xB9);
		__spi_send_data (0x03); __spi_send_data (0xC8); __spi_send_data (0x03); __spi_send_data (0xDB);
		__spi_send_data (0x03); __spi_send_data (0xE7); __spi_send_data (0x03); __spi_send_data (0xF4);
		__spi_send_data (0x03); __spi_send_data (0xFB); __spi_send_data (0x03); __spi_send_data (0xFF);

	//__spi_send_cmd (0x3a); __spi_send_data (0x66);

	__spi_send_cmd (0x11);
	delay(120);
	__spi_send_cmd (0x29);
}
}
}