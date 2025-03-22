#ifndef PCA9555_H
#define PCA9555_H

#include <esp_err.h>
#include <driver/i2c.h>

typedef enum {
	pca_port_0 = 0,
	pca_port_1
} pca_port_num_t;

#define PCA_PIN_P00       0
#define PCA_PIN_P01       1
#define PCA_PIN_P02       2
#define PCA_PIN_P03       3
#define PCA_PIN_P04       4
#define PCA_PIN_P05       5
#define PCA_PIN_P06       6
#define PCA_PIN_P07       7
#define PCA_PIN_P10       0
#define PCA_PIN_P11    	  1
#define PCA_PIN_P12       2
#define PCA_PIN_P13       3
#define PCA_PIN_P14       4
#define PCA_PIN_P15       5
#define PCA_PIN_P16       6
#define PCA_PIN_P17       7
#define PCA_OUT_CFG_PIN   0
#define PCA_IN_CFG_PIN    1
#define PCA_IN_INVERT_PIN   1
#define PCA_IN_DIRECT_PIN   0


esp_err_t pca9555_read_input(pca_port_num_t pca_port, uint8_t *value);
esp_err_t pca9555_set_value(pca_port_num_t pca_port, uint8_t mask, uint8_t value);
esp_err_t pca9555_set_inversion(pca_port_num_t pca_port, uint8_t value);

/**
 * @brief Configure extend port
 * If a bit in this register is set (written with �1�), the corresponding port pin is enabled as an input with
 * high-impedance output driver.
 * If a bit in this register is cleared (written with �0�), the corresponding port pin is enabled as an output.
 */
esp_err_t pca9555_init(i2c_port_t port, uint8_t port0, uint8_t port1);

#endif // PCA9555_H
