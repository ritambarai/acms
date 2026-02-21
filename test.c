#include <stdio.h>
#include <stdint.h>

#include "data_manager.h"
#include "hashmap.h"

/* ------------------------------------------------------------
 * External variables to be tracked
 * (must have static/global lifetime)
 * ------------------------------------------------------------ */
static float temperature = 25.5f;
static float humidity    = 60.0f;
static float pressure    = 101.3f;
static float status      = 0.0f; 
static float voltage     = 3.3f;

/* ------------------------------------------------------------
 * Test function
 * ------------------------------------------------------------ */
static void dm_test_basic(void)
{
    printf("=== DATA MANAGER TEST START ===\n");

    /* --------------------------------------------------------
     * INIT
     * -------------------------------------------------------- */
    dm_system_init();
    printf("System initialised\n");

    /* --------------------------------------------------------
     * SET VALUES (CLASS: SENSOR)
     * -------------------------------------------------------- */
    dm_set_value("sensor", "temperature",
                 "float", &temperature, temperature);

    dm_set_value("sensor", "humidity",
                 "float", &humidity, humidity);

    dm_set_value("sensor", "pressure",
                 "float", &pressure, pressure);

    printf("Sensor values registered\n");

    /* --------------------------------------------------------
     * UPDATE A VALUE
     * -------------------------------------------------------- */
    temperature = 26.8f;
    get_value(&temperature);
    dm_set_value("sensor", "temperature",
                 "float", &temperature, temperature);

    printf("Temperature updated\n");
    
    pressure = 40.8f;
    update_variable(&pressure);

    printf("Pressure updated\n");

    /* --------------------------------------------------------
     * SET VALUES (CLASS: POWER)
     * -------------------------------------------------------- */
    dm_set_value("power", "voltage",
                 "float", &voltage, voltage);

    printf("Power values registered\n");
    voltage = 240.0f;
    update_variable(&voltage);
    /* --------------------------------------------------------
     * RESOLVE INDICES USING HASHMAPS
     * -------------------------------------------------------- */
    uint16_t sensor_idx = dm_class_map_find("sensor");
    //uint16_t power_idx  = dm_class_map_find("power");

    //uint16_t temp_idx = dm_var_map_find(sensor_idx, "temperature");
    //uint16_t hum_idx  = dm_var_map_find(sensor_idx, "humidity");
    //uint16_t pres_idx = dm_var_map_find(sensor_idx, "pressure");
    //uint16_t volt_idx = dm_var_map_find(power_idx,  "voltage");

    /* --------------------------------------------------------
     * READ BACK VALUES
     * -------------------------------------------------------- */
    printf("\n--- GET VALUES ---\n");

    get_value(&temperature);
    get_value(&humidity);
    get_value(&pressure);
    get_value(&voltage);

    printf("\n--- GET CLASS VALUES ---\n");
    get_class_values("sensor");
    printf("=== DATA MANAGER TEST END ===\n");
    printf("=== DATA SYNC ===\n");
    sync_class(sensor_idx);
    pressure = 50.8f;
    update_variable(&pressure);
    temperature = 30.0f;
    update_variable(&temperature);
    sync_class(sensor_idx);
    dm_set_value("sensor", "status",
                 "bool", &status, status);
    get_class_values("sensor");  
}

/* ------------------------------------------------------------
 * MAIN
 * ------------------------------------------------------------ */
int main(void)
{
    dm_test_basic();
    return 0;
}

