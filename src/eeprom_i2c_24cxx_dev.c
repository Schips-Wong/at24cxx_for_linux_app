/*
 * File      : eeprom_i2c_24cxx_dev.c
 *
 * This file is part of eeprom device for 24cxx,such as at24c02/at24c16.
 * COPYRIGHT (C) 2016
 *
 * Change Logs:
 * Date           Author       Version			Notes
 * 2018-12-25     Acuity      	1.00		first version.
 *
 * Note:
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "eeprom_i2c_24cxx_dev.h"

/**
 * @brief  get page size
 * @param  model:eeprom model
 * @retval page size
 */
static uint16_t get_eeprom_pagesize(eeprom_model_t model)
{
    switch(model)
    {
        case EEPROM_24C01:
        case EEPROM_24C02:
            return 8;

        case EEPROM_24C04:
        case EEPROM_24C08:
        case EEPROM_24C16:
            return 16;

        case EEPROM_24C32:
        case EEPROM_24C64:
        case EEPROM_24C128:
        case EEPROM_24C256:
        case EEPROM_24C512:
        case EEPROM_24C1024:
            return 32;
        default:
            return 0;
    }
}

/**
 * @brief  get chip size
 * @param  model:eeprom model
 * @retval chip size,uinit:byte
 */
static uint32_t get_eeprom_chipsize(eeprom_model_t model)
{
    switch(model)
    {
        case EEPROM_24C01:
            return 128u;
        case EEPROM_24C02:
            return 256u;
        case EEPROM_24C04:
            return 512u;
        case EEPROM_24C08:
            return 1024u;
        case EEPROM_24C16:
            return 2048u;
        case EEPROM_24C32:
            return 4096u;
        case EEPROM_24C64:
            return 8192u;
        case EEPROM_24C128:
            return 16384u;
        case EEPROM_24C256:
            return 32768u;
        case EEPROM_24C512:
            return 65536u;
        case EEPROM_24C1024:
            return 131072u;
        default:
            return 0;
    }
}

/**
 * @brief  write one page.
 * @param  pdev pointer to the eeprom device struct.
 * @param  addr the address of write to.
 * @param  pbuf the data to write.
 * @param  size number of bytes to write..
 * @retval return 0 if 0k,anything else is considered an error.
 */
static int16_t eeprom_write_page(int fd, eeprom_dev_t *pdev,uint32_t addr, uint8_t *pbuf,uint32_t size)
{
    struct i2c_dev_message 		ee24_msg;
    struct i2c_rdwr_ioctl_data 	ee24_ioctl_data;
    uint8_t		*buf = NULL;
    uint16_t 	slave_addr;
    uint16_t	ee_page_size = 0;
    int			ret_size = 0;

    if(pdev == 0)
    {
        return EEPROM_ERR_DEV_NONE;
    }

    ee_page_size = get_eeprom_pagesize(pdev->model);
    if(((addr % ee_page_size) + size) > ee_page_size) 	/*the over flow of page size*/
    {
        return EEPROM_ERR_PAGE_SIZE;
    }

    buf = (uint8_t*)malloc(ee_page_size+2);
    if(buf==NULL)
    {
        return EEPROM_ERR_MEM;
    }

    if(pdev->model > EEPROM_24C16)
    {/*24c32-24c1024*/
        slave_addr = pdev->slave_addr;
        buf[0] = (addr >>8)& 0xff;
        buf[1] = addr & 0xff;
        memcpy(buf+2,pbuf,size);		/*copy data*/
        ee24_msg.size  = size + 2;
    }
    else
    {/*24c01-24c16*/
        slave_addr = pdev->slave_addr | (addr>>8);
        buf[0] = addr & 0xff;
        memcpy(buf+1,pbuf,size);		/*copy data*/
        ee24_msg.size  = size + 1;
    }
    ee24_msg.addr  = slave_addr;
    ee24_msg.flags = I2C_BUS_WR;
    ee24_msg.buff  = buf;

    ee24_ioctl_data.nmsgs = 1;
    ee24_ioctl_data.msgs  = &ee24_msg;
    ret_size = ioctl(fd, I2C_RDWR,(unsigned long)&ee24_ioctl_data);

    free(buf);

    if(ret_size < 0)
    {
        return EEPROM_ERR_I2C_WRITE;
    }
    else
    {
        return EEPROM_OK;
    }
}


/**
 * @brief  write data to eeprom.
 * @param  pdev pointer to the eeprom device struct.
 * @param  addr the address of write to.
 * @param  pbuf the data to write.
 * @param  size number of bytes to write..
 * @retval return 0 if 0k,anything else is considered an error.
 */
int16_t eeprom_write(eeprom_dev_t *pdev,uint32_t addr, uint8_t *pbuf, uint32_t size)
{
    uint8_t  	write_len,page_offset;
    int8_t 		error = 0;
    uint16_t	ee_page_size = 0;
    int			fd = -1;

    if(pdev == 0)
    {
        return EEPROM_ERR_DEV_NONE;
    }

    if((addr+size) > get_eeprom_chipsize(pdev->model))	/*the over flow of chip size*/
    {
        return EEPROM_ERR_CHIP_SIZE;
    }

    fd = open(pdev->i2c_dev,O_WRONLY);		/*Write only*/
    if(fd < 0)
    {
        return EEPROM_ERR_DEV_NONE;
    }

    if(pdev->callback_write_lock)	/*release write protect*/
    {
        pdev->callback_write_lock(0);
    }

    ee_page_size = get_eeprom_pagesize(pdev->model);
    while(size > 0)
    {
        page_offset = ee_page_size - (addr % ee_page_size);
        write_len   = size > page_offset ? page_offset : size;
        error = eeprom_write_page(fd,pdev,addr,pbuf,write_len);
        if(error)
        {
            break;
        }
        size = size - write_len;
        if(size > 0)
        {
            pbuf = pbuf + write_len;
            addr = addr + write_len;
        }
        if(pdev->callback_page_write_delay)
        {
            pdev->callback_page_write_delay();		/*eeprom need wait*/
        }
    }
    if(pdev->callback_write_lock)
    {
        pdev->callback_write_lock(1);/*write protect*/
    }

    close(fd);

    return error;
}

/**
 * @brief  read from the eeprom.
 * @param  pdev pointer to the eeprom device struct.
 * @param  addr the address to read from.
 * @param  pbuf where to put read data.
 * @param  size number of bytes to read.
 * @retval return 0 if 0k,anything else is considered an error.
 */
int16_t eeprom_read(eeprom_dev_t *pdev,uint32_t addr, uint8_t *pbuf, uint32_t size)
{
    struct i2c_dev_message 		ee24_msg[2];
    struct i2c_rdwr_ioctl_data 	ee24_ioctl_data;
    uint8_t	 buf[2];
    uint8_t  slave_addr,ret_size = 0;
    int		 fd = -1;

    if(pdev == 0)
    {
        return EEPROM_ERR_DEV_NONE;
    }

    if((addr+size) > get_eeprom_chipsize(pdev->model))
    {
        return EEPROM_ERR_CHIP_SIZE;
    }

    fd = open(pdev->i2c_dev,O_RDONLY);		/*Read only*/
    if(fd < 0)
    {
        return EEPROM_ERR_DEV_NONE;
    }

    if(pdev->model > EEPROM_24C16)
    {
        slave_addr 	= pdev->slave_addr;
        buf[0] 		= (addr >>8)& 0xff;
        buf[1] 		= addr & 0xff;
        ee24_msg[0].size  = 2;
    }
    else
    {
        slave_addr = pdev->slave_addr | (addr>>8);
        buf[0] = addr & 0xff;
        ee24_msg[0].size  = 1;
    }
    ee24_msg[0].buff  = buf;
    ee24_msg[0].addr  = slave_addr;
    ee24_msg[0].flags = I2C_BUS_WR;
    ee24_msg[1].addr  = slave_addr;
    ee24_msg[1].flags = I2C_BUS_RD;
    ee24_msg[1].buff  = pbuf;
    ee24_msg[1].size  = size;

    ee24_ioctl_data.nmsgs = 2;
    ee24_ioctl_data.msgs  = ee24_msg;
    ret_size = ioctl(fd,I2C_RDWR,(unsigned long)&ee24_ioctl_data);

    close(fd);
    if(ret_size < 0)
    {
        return EEPROM_ERR_I2C_WRITE;
    }
    else
    {
        return EEPROM_OK;
    }
}

/**
 * @brief  erase the eeprom.
 * @param  pdev pointer to the eeprom device struct.
 * @param  addr the address to erase from.
 * @param  data padding data.
 * @param  size number of bytes to erase.
 * @retval return 0 if 0k,anything else is considered an error.
 */
int16_t eeprom_erase(eeprom_dev_t *pdev,uint32_t addr, uint8_t data, uint32_t size)
{
    uint8_t *buf = NULL;
    int16_t error = EEPROM_OK;

    if(pdev == 0)
    {
        return EEPROM_ERR_DEV_NONE;
    }

    if((addr+size) > get_eeprom_chipsize(pdev->model))
    {
        return EEPROM_ERR_CHIP_SIZE;
    }

    if(size == 0)
    {
        return EEPROM_OK;
    }

    buf = (uint8_t*)malloc(size);
    if(buf == NULL)
    {
        return EEPROM_ERR_MEM;
    }
    memset(buf,data,size);

    /*write page by page, the address increment, the page write delay and the
      write protect are all handled in eeprom_write*/
    error = eeprom_write(pdev,addr,buf,size);

    free(buf);

    return error;
}

