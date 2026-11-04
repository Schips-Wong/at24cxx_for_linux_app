#ifndef EEPROM_I2C_24CXX_DEV_H
#define EEPROM_I2C_24CXX_DEV_H

#include <stdint.h>

/*operation state*/
typedef enum
{
    EEPROM_OK				= 0,
    EEPROM_ERR_MEM			= -1,
    EEPROM_ERR_I2C_WRITE	= 1,
    EEPROM_ERR_DEV_NONE		= 2,
    EEPROM_ERR_PAGE_WRITE	= 3,
    EEPROM_ERR_PAGE_SIZE	= 4,
    EEPROM_ERR_CHIP_SIZE	= 5,
}eeprom_status_t;

/*eeprom model*/
typedef enum
{
    EEPROM_24C01 = 1,
    EEPROM_24C02,
    EEPROM_24C04,
    EEPROM_24C08,
    EEPROM_24C16,
    EEPROM_24C32,
    EEPROM_24C64,
    EEPROM_24C128,
    EEPROM_24C256,
    EEPROM_24C512,
    EEPROM_24C1024,
}eeprom_model_t;

/*linux ioctl cmd*/
#define I2C_RETRIES     		0x0701     	/*设置收不到ACK时的重试次数*/
#define I2C_TIMEOUT    		 	0x0702     	/*设置超时时限的jiffies*/
#define I2C_SLAVE       		0x0703		/*设置从机地址*/
#define I2C_SLAVE_FORCE 		0x0706      /*强制设置从机地址*/
#define I2C_TENBIT      		0x0704      /*选择地址位长:=0 for 7bit , != 0 for 10 bit*/
#define I2C_FUNCS       		0x0705      /*获取适配器支持的功能*/
#define I2C_RDWR        		0x0707      /*Combined R/W transfer (one STOP only) */
#define I2C_PEC         		0x0708    	/*!= 0 to use PEC with SMBus*/
#define I2C_SMBUS       		0x0720      /*SMBus transfer*/

/*i2c flag*/
#define I2C_BUS_WR            0x0000
#define I2C_BUS_RD            (1u << 0)
#define I2C_BUS_ADDR_10BIT    (1u << 2)  /* this is a ten bit chip address */
#define I2C_BUS_NO_START      (1u << 4)
#define I2C_BUS_IGNORE_NACK   (1u << 5)
#define I2C_BUS_NO_READ_ACK   (1u << 6)  /* when I2C reading, we do not ACK */

//the message of i2c device
struct i2c_dev_message
{
    unsigned short  addr;
    unsigned short  flags;
    unsigned short  size;
    unsigned char   *buff;
};

struct i2c_rdwr_ioctl_data
{
    struct i2c_dev_message	*msgs;
    int nmsgs;
};

/*24cxx eeprom devcie struct*/
typedef struct
{
    char			*i2c_dev;					            /*i2c bus device*/
    uint8_t			slave_addr;				            	/*eeprom i2c addr-bit-7*/
    eeprom_model_t	model;						            /*eeprom model*/
    void			(*callback_set_wp_pin)(uint8_t ctrl);   /*protect of write function*/
    void			(*callback_page_write_delay)(void);	    /*delay after page write, EEPROM needs it,FRAM not need*/

}eeprom_dev_t;

/*extern function*/
extern uint16_t eeprom_page_size(eeprom_model_t model);
extern int16_t eeprom_read(eeprom_dev_t *pdev, uint32_t addr, uint8_t *pbuf, uint32_t size);
extern int16_t eeprom_write(eeprom_dev_t *pdev, uint32_t addr, uint8_t *pbuf, uint32_t size);
extern int16_t eeprom_erase(eeprom_dev_t *pdev, uint32_t addr, uint8_t data, uint32_t size);

#endif

