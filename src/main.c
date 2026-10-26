#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "eeprom_i2c_24cxx_dev.h"

#define USE_24CXX_TEST      /*test enable*/

/**
 * @brief  delay function
 * @param  none
 * @retval none
 */
static void callback_page_write_delay(void)
{
#if 1
    usleep(4000);
#else
    uint16_t i;

    i = 0xFFFF;
    while(i--);
#endif
}

/**
 * @brief  default device config, used for the options not given on the command line
 */
static const eeprom_dev_t default_dev =
{
    .i2c_dev                   = "/dev/i2c-0",     /*i2c device name*/
    .slave_addr                = 0x50,             /*eeprom address-bit-7*/
    .model                     = EEPROM_24C16,     /*eeprom model,eg AT24C02*/
    .callback_write_lock       = NULL,             /*no write lock*/
    .callback_page_write_delay = callback_page_write_delay, /*delay function*/
};

/*eeprom model table, used by the command line option -m*/
static const struct
{
    const char      *name;
    eeprom_model_t  model;
}model_table[] =
{
    {"24c01",   EEPROM_24C01},
    {"24c02",   EEPROM_24C02},
    {"24c04",   EEPROM_24C04},
    {"24c08",   EEPROM_24C08},
    {"24c16",   EEPROM_24C16},
    {"24c32",   EEPROM_24C32},
    {"24c64",   EEPROM_24C64},
    {"24c128",  EEPROM_24C128},
    {"24c256",  EEPROM_24C256},
    {"24c512",  EEPROM_24C512},
    {"24c1024", EEPROM_24C1024},
};

#define MODEL_TABLE_SIZE    (sizeof(model_table) / sizeof(model_table[0]))

/**
 * @brief  get the name of a eeprom model
 * @param  model:eeprom model
 * @retval model name,e.g. "24c16"
 */
static const char *model_name(eeprom_model_t model)
{
    size_t i;

    for(i = 0; i < MODEL_TABLE_SIZE; i++)
    {
        if(model_table[i].model == model)
            return model_table[i].name;
    }

    return "unknown";
}

/**
 * @brief  parse the eeprom model,e.g. "24c16","at24c02","AT24C256"
 * @param  str:string from the command line
 * @param  model:where to store the model
 * @retval return 0 if 0k,anything else is considered an error.
 */
static int parse_model(const char *str, eeprom_model_t *model)
{
    size_t i;

    if((str == NULL) || (*str == '\0'))
        return -1;

    if((str[0] == 'a' || str[0] == 'A') && (str[1] == 't' || str[1] == 'T'))
        str += 2;                   /*skip the "at" of at24c02*/

    for(i = 0; i < MODEL_TABLE_SIZE; i++)
    {
        if(strcasecmp(str, model_table[i].name) == 0)
        {
            *model = model_table[i].model;
            return 0;
        }
    }

    return -1;
}

/**
 * @brief  parse the i2c slave address,e.g. "0x50","80"
 * @param  str:string from the command line
 * @param  addr:where to store the address
 * @retval return 0 if 0k,anything else is considered an error.
 */
static int parse_slave_addr(const char *str, uint8_t *addr)
{
    char *end = NULL;
    unsigned long val = 0;

    if((str == NULL) || (*str == '\0'))
        return -1;

    val = strtoul(str, &end, 0);    /*base 0:0x50 / 80 */
    if((end == str) || (*end != '\0') || (val > 0xFF))
        return -1;

    *addr = (uint8_t)val;
    return 0;
}

/**
 * @brief  print the command line usage
 */
static void usage(const char *prog)
{
    printf("Usage: %s [options]\n", prog);
    printf("  -d <dev>    i2c bus device, default: %s\n", default_dev.i2c_dev);
    printf("  -a <addr>   eeprom i2c address, e.g. 0x50, default: 0x%02x\n", default_dev.slave_addr);
    printf("  -m <model>  eeprom model: 24c01~24c1024, default: %s\n", model_name(default_dev.model));
    printf("  -h          show this help\n");
    printf("e.g. %s -d /dev/i2c-1 -a 0x50 -m 24c02\n", prog);
}

/**
 * @brief  parse the command line and update the device config
 * @param  argc:number of the arguments
 * @param  argv:the arguments
 * @param  dev:device config,options not given keep the default value
 * @retval return 0 if 0k,anything else is considered an error.
 */
static int parse_args(int argc, char *argv[], eeprom_dev_t *dev)
{
    int opt;

    while((opt = getopt(argc, argv, "d:a:m:h")) != -1)
    {
        switch(opt)
        {
            case 'd':
                dev->i2c_dev = optarg;
                break;

            case 'a':
                if(parse_slave_addr(optarg, &dev->slave_addr) != 0)
                {
                    fprintf(stderr, "invalid i2c address: %s\n", optarg);
                    return -1;
                }
                break;

            case 'm':
                if(parse_model(optarg, &dev->model) != 0)
                {
                    fprintf(stderr, "invalid eeprom model: %s\n", optarg);
                    return -1;
                }
                break;

            case 'h':
                usage(argv[0]);
                exit(EXIT_SUCCESS);

            default:
                usage(argv[0]);
                return -1;
        }
    }

    if(optind < argc)
    {
        fprintf(stderr, "unexpected argument: %s\n", argv[optind]);
        usage(argv[0]);
        return -1;
    }

    return 0;
}

#ifdef USE_24CXX_TEST

/**
 * @brief  print the data in hex
 * @param  buf:data buffer
 * @param  addr:start address
 * @param  size:number of bytes to print
 * @retval none
 */
static void data_printf(uint8_t *buf, int addr, int size)
{
    int i;

    for(i = 0; i < size; i++)
    {
        if( (i % 16) == 0 )
        {
            printf("\n %.4x|  ", addr);
        }
        else if( (i % 8) == 0 )
        {
            printf("  ");
        }
        printf("%.2x ", *(buf+i));
        addr++;
    }
    printf("\r\n");
}

#define WR_SIZE     (2*1024)
#define WR_ADDR     0

static uint8_t write_buf[WR_SIZE];
static uint8_t read_buf[WR_SIZE];

/**
 * @brief  test
 * @param  pdev pointer to the eeprom device struct
 * @retval return 0 if 0k,anything else is considered an error.
 */
static int16_t eeprom_test(eeprom_dev_t *pdev)
{
    uint32_t i;
    int16_t  error = 0;

    eeprom_read(pdev, WR_ADDR, read_buf, WR_SIZE);
    printf("first read:\r\n");
    data_printf(read_buf, WR_ADDR, WR_SIZE);

    for( i = 0; i < WR_SIZE; i++)
    {
        write_buf[i] = i;
    }
    error = eeprom_write(pdev, WR_ADDR, write_buf, WR_SIZE);
    if(error != EEPROM_OK)
    {
        printf("write failed, error code: %d\r\n", error);
        return error;
    }
    eeprom_read(pdev, WR_ADDR, read_buf, WR_SIZE);
    printf("read write:\r\n");
    data_printf(read_buf, WR_ADDR, WR_SIZE);

    memset(write_buf, 0, sizeof(write_buf));
    error = eeprom_write(pdev, WR_ADDR, write_buf, WR_SIZE);
    if(error != EEPROM_OK)
    {
        printf("write failed, error code: %d\r\n", error);
        return error;
    }
    eeprom_read(pdev, WR_ADDR, read_buf, WR_SIZE);
    printf("read \'memset\':\r\n");
    data_printf(read_buf, WR_ADDR, WR_SIZE);

    return error;
}

#endif  /*USE_24CXX_TEST*/

int main(int argc, char *argv[])
{
    eeprom_dev_t dev = default_dev;

    if(parse_args(argc, argv, &dev) != 0)
        return EXIT_FAILURE;

    printf("i2c dev : %s\n", dev.i2c_dev);
    printf("addr7   : 0x%02x\n", dev.slave_addr);
    printf("model   : %s\n", model_name(dev.model));

#ifdef USE_24CXX_TEST
    return (eeprom_test(&dev) == EEPROM_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
#else
    return EXIT_SUCCESS;
#endif
}
