# at24cxx_for_linux_app

在 Linux 应用层通过 `/dev/i2c-N` 读写 24Cxx 系列 I2C EEPROM 的小工具 + 可复用驱动库。

- 驱动：`src/eeprom_i2c_24cxx_dev.c/.h`，直接走 `ioctl(fd, I2C_RDWR, ...)`（即 i2c-dev 的 combined transfer），不依赖任何第三方 I2C 库。
- 工具：`src/main.c`，把驱动包装成一个命令行程序，可指定总线、从机地址、型号，也可把整个文件烧进 EEPROM。

## 一、使用场景

1. **板端写入小容量参数**：MAC 地址、序列号、设备 ID、校准数据、开机 logo、配置块等，容量通常在几十字节到几十 KB。
2. **产线 / 售后批量烧写**：同一条命令换 `-d`、`-a`、`-m` 即可适配不同板子，适合放进脚本批量执行。
3. **调试阶段自检**：不带 `-f` 时程序会跑一遍“读 → 写递增数据 → 回读打印 → 写 0 → 回读打印”的测试流程，可快速确认总线、地址、容量是否正确。
4. **作为库集成**：把 `.c/.h` 直接拷进自己的工程，用 `eeprom_dev_t` 配置好参数后调用 `eeprom_read/write/erase`。

支持型号（芯片标称容量）：

| 型号 | 容量 | 内部页大小 | 寻址方式 |
|---|---|---|---|
| 24C01 | 128 B | 8 B | 1 字节地址 |
| 24C02 | 256 B | 8 B | 1 字节地址 |
| 24C04 | 512 B | 16 B | 1 字节地址 |
| 24C08 | 1 KB | 16 B | 1 字节地址 |
| 24C16 | 2 KB | 16 B | 1 字节地址 |
| 24C32 | 4 KB | 32 B | 2 字节地址 |
| 24C64 | 8 KB | 32 B | 2 字节地址 |
| 24C128 | 16 KB | 32 B | 2 字节地址 |
| 24C256 | 32 KB | 32 B | 2 字节地址 |
| 24C512 | 64 KB | 32 B | 2 字节地址 |
| 24C1024 | 128 KB | 32 B | 2 字节地址 |

> 24C01~24C16 的高位地址通过从机地址的 bit1~bit3 传递，因此 `-a` 只填基础 7 位地址（如 `0x50`），驱动会自己把 `addr >> 8` 或进从机地址。
> 表中页大小取自 `get_eeprom_pagesize()`；部分芯片实际页更大（如 AT24C256 为 64 B），按更小的页写只是多几次内部延时，数据依然正确；想更快可以改这个函数。

## 二、编译

依赖交叉/本地 gcc 与 Linux 头文件（需要在 Linux 环境下编译）：

```bash
cd src
make            # 默认 CC = aarch64-linux-gnu-gcc，产物为 output/at24
make CC=gcc     # 本机（x86 主机）编译
make clean
```

`Makefile` 用 `wildcard *.c` 收集源文件，`output/at24` 即最终程序。

## 三、命令行用法

```
Usage: ./output/at24 [options]
  -d <dev>    i2c bus device, default: /dev/i2c-0
  -a <addr>   eeprom i2c address, e.g. 0x50, default: 0x50
  -m <model>  eeprom model: 24c01~24c1024, default: 24c16
  -f <file>   write the content of the file to the eeprom, from address 0
  -h          show this help
```

| 选项 | 说明 | 缺省值 |
|---|---|---|
| `-d <dev>` | I2C 总线设备节点，如 `/dev/i2c-0`、`/dev/i2c-1` | `default_dev.i2c_dev`（`/dev/i2c-0`） |
| `-a <addr>` | EEPROM 的 7 位从机地址，十六进制或十进制均可（`0x50`、`80`），范围 0x00~0xFF | `0x50` |
| `-m <model>` | 芯片型号，`24c01`~`24c1024`；也接受带厂商前缀或大写的写法：`at24c02`、`AT24C256` | `24c16` |
| `-f <file>` | 把该文件内容写入 EEPROM，起始地址固定为 0 | 不指定则跑内置读写测试 |
| `-h` | 打印帮助并退出 | — |

未在命令行给出的选项，一律取自 `src/main.c` 里的 `default_dev`，修改它即可改默认行为。

### 示例

```bash
# 1. 查看帮助
./output/at24 -h

# 2. 用默认配置（/dev/i2c-0，0x50，24C16）跑读写自检
./output/at24

# 3. 换总线和型号跑自检
./output/at24 -d /dev/i2c-1 -a 0x50 -m 24c02

# 4. 把固件/参数文件烧进 24C256（从地址 0 开始）
./output/at24 -d /dev/i2c-1 -a 0x50 -m 24c256 -f app.bin

# 5. 烧写并检查返回码（脚本里用）
./output/at24 -m 24c512 -f config.bin && echo "burn ok" || echo "burn failed"
```

启动后先回显实际生效的配置，再执行动作：

```
i2c dev : /dev/i2c-1
addr7   : 0x50
model   : 24c256
write 12288 bytes from app.bin to 0x0000 done
```

`-f` 模式只写不校验；需要校验时请用驱动接口写一个自己的回读比对流程（`eeprom_read()`）。

返回值：`0` 成功，非 `0` 失败（参数错误、文件打不开、I2C 写失败、超出芯片容量等）。

## 四、作为库使用

```c
#include <unistd.h>
#include "eeprom_i2c_24cxx_dev.h"

static void my_page_write_delay(void)   /* EEPROM 页写内部需要 tWR，约 5ms */
{
    usleep(5000);
}

static const eeprom_dev_t eeprom_dev =
{
    .i2c_dev                   = "/dev/i2c-1",     /*i2c 总线设备*/
    .slave_addr                = 0x50,             /*7 位从机地址*/
    .model                     = EEPROM_24C256,    /*芯片型号*/
    .callback_set_wp_pin       = NULL,             /*写保护引脚控制，无则填 NULL*/
    .callback_page_write_delay = my_page_write_delay,
};

void demo(void)
{
    uint8_t buf[16] = {0};
    eeprom_dev_t *pdev = (eeprom_dev_t *)&eeprom_dev;   /*接口形参非 const*/

    eeprom_write(pdev, 0x0000, buf, sizeof(buf));       /*跨页会自动拆分*/
    eeprom_read (pdev, 0x0000, buf, sizeof(buf));
    eeprom_erase(pdev, 0x0000, 0xFF, sizeof(buf));      /*用 0xFF 填充*/
}
```

接口：

| 函数 | 说明 |
|---|---|
| `int16_t eeprom_read(eeprom_dev_t *pdev, uint32_t addr, uint8_t *pbuf, uint32_t size)` | 从 `addr` 读 `size` 字节 |
| `int16_t eeprom_write(eeprom_dev_t *pdev, uint32_t addr, uint8_t *pbuf, uint32_t size)` | 从 `addr` 写 `size` 字节，内部按页拆分，并调用写保护/延时回调 |
| `int16_t eeprom_erase(eeprom_dev_t *pdev, uint32_t addr, uint8_t data, uint32_t size)` | 用 `data` 填充指定区间（按页写入，内部复用 `eeprom_write()`） |

返回码（`eeprom_status_t`）：

| 值 | 宏 | 含义 |
|---|---|---|
| 0 | `EEPROM_OK` | 成功 |
| -1 | `EEPROM_ERR_MEM` | 内存分配失败 |
| 1 | `EEPROM_ERR_I2C_WRITE` | I2C 传输失败（无应答、地址错、总线异常） |
| 2 | `EEPROM_ERR_DEV_NONE` | 设备节点打不开，或 `pdev` 为空 |
| 3 | `EEPROM_ERR_PAGE_WRITE` | 保留，当前版本未使用 |
| 4 | `EEPROM_ERR_PAGE_SIZE` | 单次页写跨越页边界 |
| 5 | `EEPROM_ERR_CHIP_SIZE` | 读写区间超出芯片容量 |

`callback_set_wp_pin(ctrl)`：写之前以 `0` 调用（解除写保护），写完后以 `1` 调用（恢复写保护），内部已保证成对调用。
`callback_page_write_delay()`：每写完一页调用一次，给 EEPROM 留出内部写周期；FRAM 之类的器件可传 `NULL`。

## 五、注意事项

- 需要内核加载 `i2c-dev`（`modprobe i2c-dev`）并确认总线已注册：`i2cdetect -l`。`/dev/i2c-*` 一般只有 root 或 `i2c` 组成员可访问，必要时用 `sudo`。
- `-f` 目前固定从地址 0 开始写，文件大小不能超过芯片容量，否则返回 `EEPROM_ERR_CHIP_SIZE(5)`。
- 不带 `-f` 时的自检固定读写 2 KB（`WR_SIZE`），对 24C01/24C02/24C04/24C08/24C16 会因超出容量而失败，属预期现象，请改 `WR_SIZE` 或用 `-f` 烧指定文件。
- 写之前请确认目标地址的数据可以覆盖：EEPROM 只能按字节改写，本工具不做“先擦后写”以外的保护。
- `USE_24CXX_TEST` 宏（`src/main.c`）只控制是否内置自检流程；关闭后 `main` 仍可正常解析参数并烧写文件。
