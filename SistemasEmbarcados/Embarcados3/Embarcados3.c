#include "contiki.h"
#include "sys/etimer.h"
#include "dev/leds.h"
#include "dev/watchdog.h"
#include "dev/adc-sensor.h"
#include "random.h"
#include "button-sensor.h"
#include "board-peripherals.h"
#include "ti-lib.h"
#include <stdio.h>
#include <stdint.h>

/*---------------------------------------------------------------------------*/
int16_t pwminit(int32_t freq)
{
    uint32_t load = 0;

    ti_lib_ioc_pin_type_gpio_output(IOID_21);
    leds_off(LEDS_RED);

    /* Enable GPT0 clocks under active mode */
    ti_lib_prcm_peripheral_run_enable(PRCM_PERIPH_TIMER0);
    ti_lib_prcm_load_set();
    while(!ti_lib_prcm_load_get());

    /* Drive the I/O ID with GPT0 / Timer A */
    ti_lib_ioc_port_configure_set(IOID_21, IOC_PORT_MCU_PORT_EVENT0, IOC_STD_OUTPUT);

    /* GPT0 / Timer A: PWM, Interrupt Enable */
    ti_lib_timer_configure(GPT0_BASE,
    TIMER_CFG_SPLIT_PAIR | TIMER_CFG_A_PWM | TIMER_CFG_B_PWM);

    /* Stop the timers */
    ti_lib_timer_disable(GPT0_BASE, TIMER_A);
    ti_lib_timer_disable(GPT0_BASE, TIMER_B);

    if(freq > 0) {
        load = (GET_MCU_CLOCK / freq);
        ti_lib_timer_load_set(GPT0_BASE, TIMER_A, load);
        ti_lib_timer_match_set(GPT0_BASE, TIMER_A, load-1);

        /* Start */
        ti_lib_timer_enable(GPT0_BASE, TIMER_A);
    }
    return load;
}

/*---------------------------------------------------------------------------*/
static struct etimer et;
static struct etimer et_adc;
static struct etimer et_adc_sensor;
static struct etimer et_gpio;
static uint16_t single_adc_sample;

/*---------------------------------------------------------------------------*/
PROCESS(gpio_process, "gpio bare metal process");
PROCESS(adc_process_sensor, "adc driver process");
PROCESS(pwm_process, "pwm process");
AUTOSTART_PROCESSES(&gpio_process, &adc_process_sensor, &pwm_process);

/*---------------------------------------------------------------------------*/
/* GPIO */
PROCESS_THREAD(gpio_process, ev, data)
{
    static int8_t counter = 0;
    PROCESS_BEGIN();

    etimer_set(&et_gpio, CLOCK_SECOND * 1);

    /* initialization of GPIO output */
    ti_lib_rom_ioc_pin_type_gpio_output(IOID_27);
    ti_lib_rom_ioc_pin_type_gpio_output(IOID_30);
    ti_lib_gpio_clear_multi_dio(1<<IOID_27 | 1<<IOID_30);

    while(1) {
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et_gpio));
        counter++;
        GPIO_writeDio(IOID_27,counter&1);
        GPIO_writeDio(IOID_30,(counter>>1)&1);
        etimer_reset(&et_gpio);
    }
    PROCESS_END();
}

PROCESS_THREAD(adc_process_sensor, ev, data)
{
    static struct sensors_sensor *sensor;
    static int rv;
    PROCESS_BEGIN();

    etimer_set(&et_adc_sensor, CLOCK_SECOND * 4);
    while(1) {

        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et_adc_sensor));

        sensor = sensors_find(ADC_SENSOR);
        if(sensor) {
            SENSORS_ACTIVATE(*sensor);
            sensor->configure(ADC_SENSOR_SET_CHANNEL,ADC_COMPB_IN_AUXIO7);
            rv = sensor->value(ADC_SENSOR_VALUE);
            SENSORS_DEACTIVATE(*sensor);
            printf("ADC value is %d\n", rv);
        }
        else
        {
            printf("Sensor not found!\n");

        }
        etimer_reset(&et_adc_sensor);
    }

    PROCESS_END();
}


PROCESS_THREAD(pwm_process, ev, data)
{
    static int16_t current_duty = 80;
    static int16_t loadvalue, ticks;
    PROCESS_BEGIN();
    loadvalue = pwminit(5000);
    printf("LoadValue %d\n", loadvalue);
    while(1){
        PROCESS_WAIT_EVENT();
        ticks = (current_duty * loadvalue) / 100;
        ti_lib_timer_match_set(GPT0_BASE, TIMER_A, loadvalue - ticks);

    }
    PROCESS_END();
}

