
#include <esp_err.h>
#include <esp_log.h>
#include <driver/i2c.h>
#include <stdint.h>
#include "pca9555.h"

#define REG_INPUT_PORT0      0
#define REG_INPUT_PORT1      1

#define REG_OUTPUT_PORT0     2
#define REG_OUTPUT_PORT1     3

#define REG_INVERT_PORT0     4
#define REG_INVERT_PORT1     5

#define REG_CONFIG_PORT0     6
#define REG_CONFIG_PORT1     7

static const int EPDIY_PCA9555_ADDR = 0x20;

#define CHIP_NOT_SET 		(I2C_NUM_MAX+1)
static i2c_port_t pca9555_i2c_num = CHIP_NOT_SET;

static esp_err_t i2c_master_read_slave(uint8_t *data_rd, size_t size, int reg) {
	if (pca9555_i2c_num == CHIP_NOT_SET){
		return ESP_ERR_NOT_FOUND;
	}
	if (size == 0) {
		return ESP_OK;
	}
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (EPDIY_PCA9555_ADDR << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, reg, true);
	i2c_master_stop(cmd);

	esp_err_t ret = i2c_master_cmd_begin(pca9555_i2c_num, cmd, 1000 / portTICK_PERIOD_MS);
	if (ret != ESP_OK) {
		return ret;
	}
	i2c_cmd_link_delete(cmd);

	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (EPDIY_PCA9555_ADDR << 1) | I2C_MASTER_READ, true);
	if (size > 1) {
		i2c_master_read(cmd, data_rd, size - 1, I2C_MASTER_ACK);
	}
	i2c_master_read_byte(cmd, data_rd + size - 1, I2C_MASTER_NACK);
	i2c_master_stop(cmd);

	ret = i2c_master_cmd_begin(pca9555_i2c_num, cmd, 1000 / portTICK_PERIOD_MS);
	if (ret != ESP_OK) {
		return ret;
	}
	i2c_cmd_link_delete(cmd);

	return ESP_OK;
}

static esp_err_t i2c_master_write_slave(uint8_t ctrl, uint8_t *data_wr, size_t size) {
	if (pca9555_i2c_num == CHIP_NOT_SET){
		return ESP_ERR_NOT_FOUND;
	}
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (EPDIY_PCA9555_ADDR << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, ctrl, true);

	i2c_master_write(cmd, data_wr, size, true);
	i2c_master_stop(cmd);
	esp_err_t ret = i2c_master_cmd_begin(pca9555_i2c_num, cmd, 1000 / portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);
	return ret;
}

static esp_err_t pca9555_write_single(int reg, uint8_t value) {
	uint8_t w_data[1] = { value };
	return i2c_master_write_slave(reg, w_data, sizeof(w_data));
}

esp_err_t pca9555_set_inversion(pca_port_num_t pca_port, uint8_t value) {
	return pca9555_write_single(REG_INVERT_PORT0 + pca_port, value);
}

esp_err_t pca9555_set_value(pca_port_num_t pca_port, uint8_t mask, uint8_t value) {
	static uint8_t current_port0 = 0, current_port1 = 0;
	uint8_t set_port;
	switch (pca_port) {
		case pca_port_0:
			set_port = current_port0 = (current_port0 & ~mask) | (value & mask);
			break;
		case pca_port_1:
			set_port = current_port1 = (current_port1 & ~mask) | (value & mask);
			break;
		default:
			return ESP_ERR_INVALID_ARG;
			break;
	}
	return pca9555_write_single(REG_OUTPUT_PORT0 + pca_port, set_port);
}

esp_err_t pca9555_read_input(pca_port_num_t pca_port, uint8_t *value) {
	esp_err_t ret = ESP_ERR_INVALID_ARG;
	uint8_t r_data[1];
	if (value) {
		ret = i2c_master_read_slave(r_data, 1, REG_INPUT_PORT0 + pca_port);
		if (ret == ESP_OK) {
			*value = r_data[0];
		}
	}
	return ret;
}

esp_err_t pca9555_init(i2c_port_t port, uint8_t port0, uint8_t port1) {
	esp_err_t ret = ESP_ERR_INVALID_ARG;
	if (port <  CHIP_NOT_SET){
		pca9555_i2c_num = port;
		ret = pca9555_write_single(REG_CONFIG_PORT0, port0);
		if (ret == ESP_OK) {
			ret = pca9555_write_single(REG_CONFIG_PORT1, port1);
		}
	}
	return ret;
}
