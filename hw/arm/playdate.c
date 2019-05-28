#include "pebble.h"
#include "hw/boards.h"
#include "hw/ssi.h"


const static PblBoardConfig s_board_config_robert_bb = {
    .dbgserial_uart_index = 2,       // USART3
    .pebble_control_uart_index = 1,  // USART2
    .button_map = {
        {STM32_GPIOG_INDEX, 6},
        {STM32_GPIOG_INDEX, 3},
        {STM32_GPIOG_INDEX, 5},
        {STM32_GPIOG_INDEX, 4},
    },
    .flash_size = 4096,  /* Kbytes - larger to aid in development and debugging */
    .ram_size = 512,  /* Kbytes */
    .num_rows = 240,
    .num_cols = 400,
    .num_border_rows = 0,
    .num_border_cols = 0,
    .row_major = true,
    .row_inverted = true,
    .col_inverted = true,
    .round_mask = false
};


void playdate_32f7xx_init(MachineState *machine, const PblBoardConfig *board_config)
{
    Stm32Gpio *gpio[STM32F7XX_GPIO_COUNT];
    Stm32F7xxUart *uart[STM32F7XX_UART_COUNT];
    Stm32Timer *timer[STM32F7XX_TIM_COUNT];
    Stm32F7xxLPTimer *lptimer;
    DeviceState *qspi_flash;
    DeviceState *rtc_dev;
    SSIBus *spi;
    SSIBus *qspi;
    struct stm32f7xx stm;
    ARMCPU *cpu;

    stm32f7xx_init(board_config->flash_size,
                   board_config->ram_size,
                   machine->kernel_filename,
                   gpio,
                   board_config->gpio_idr_masks,
                   uart,
                   timer,
                   &lptimer,
                   &rtc_dev,
                   8000000 /*osc_freq*/,
                   32768 /*osc2_freq*/,
                   &stm,
                   &cpu);


    // Set the Pebble specific QEMU settings on the target
    pebble_set_qemu_settings(rtc_dev);

    /* --- QSPI Flash ---------------------------------------------  */
    qspi = (SSIBus *)qdev_get_child_bus(stm.qspi_dev, "ssi");
    qspi_flash = ssi_create_slave_no_init(qspi, "mt25q256");
    qdev_init_nofail(qspi_flash);

    qemu_irq mt25q_cs = qdev_get_gpio_in_named(qspi_flash, SSI_GPIO_CS, 0);
    qdev_connect_gpio_out_named(stm.qspi_dev, "qspi-gpio-cs", 0, mt25q_cs);


    /* --- Display ------------------------------------------------  */
    spi = (SSIBus *)qdev_get_child_bus(stm.spi_dev[5], "ssi");
    DeviceState *display_dev = ssi_create_slave_no_init(spi, "sm-lcd");

    qdev_prop_set_int32(display_dev, "num_rows", board_config->num_rows);
    qdev_prop_set_int32(display_dev, "num_cols", board_config->num_cols);

    qdev_init_nofail(display_dev);

    // Connect up the uarts
    pebble_connect_uarts_stm32f7xx(uart, board_config);

    // Init the buttons
    pebble_init_buttons(gpio, board_config->button_map);

    // Create the board device and wire it up
    qemu_irq display_vibe;
    display_vibe = qdev_get_gpio_in_named(display_dev, "vibe_ctl", 0);
    DeviceState *board = pebble_init_board(gpio, display_vibe);

    // The GPIO from vibrate drives the vibe input on the board
    qemu_irq board_vibe_in;
    board_vibe_in = qdev_get_gpio_in_named(board, "pebble_board_vibe_in", 0);
    qdev_connect_gpio_out((DeviceState *)gpio[STM32_GPIOD_INDEX], 14, board_vibe_in);
}

static void playdate_init(MachineState *machine)
{
    playdate_32f7xx_init(machine, &s_board_config_robert_bb);
}

static void playdate_machine_init(MachineClass *mc)
{
    mc->desc = "Panic Playdate (sort of)";
    mc->init = playdate_init;
}

DEFINE_MACHINE("playdate", playdate_machine_init)
