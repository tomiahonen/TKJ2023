/* C Standard library */
#include <stdio.h>
#include <math.h>

/* XDCtools files */
#include <xdc/std.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/drivers/PIN.h>
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/I2C.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/UART.h>

#include <ti/drivers/i2c/I2CCC26XX.h>
/* Board Header files */
#include "Board.h"
#include "sensors/opt3001.h"
#include "sensors/mpu9250.h"
#include "sensors/buzzer.h"
//#include "lib/Pitches/pitches.h"


/* Task */
#define STACKSIZE 2048
Char sensorTaskStack[STACKSIZE];
Char uartTaskStack[STACKSIZE];

//----------------------------------------MPU9250
// MPU9250 power pin global variables
static PIN_Handle hMpuPin;
static PIN_State  MpuPinState;

// MPU9250 power pin
static PIN_Config MpuPinConfig[] = {
    Board_MPU_POWER  | PIN_GPIO_OUTPUT_EN | PIN_GPIO_HIGH | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

// MPU9250 uses its own I2C interface
static const I2CCC26XX_I2CPinCfg i2cMPUCfg = {
    .pinSDA = Board_I2C0_SDA1,
    .pinSCL = Board_I2C0_SCL1
};

//Buzzer PIN config
static PIN_Handle hBuzzer;
static PIN_State sBuzzer;
PIN_Config cBuzzer[] = {
  Board_BUZZER | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
  PIN_TERMINATE
};


float movavg(float *array, uint8_t array_size, uint8_t window_size);


//----------------------------------------




// JTKJ: Tehtava 3. Tilakoneen esittely
// JTKJ: Exercise 3. Definition of the state machine
enum state { WAITING=1, DATA_READY, MUSICPLAYER, UART_MESSAGE };
enum state programState = WAITING;

//Global variables
#define ARRAYSIZE 10

float ax[ARRAYSIZE] = {};
float ay[ARRAYSIZE] = {};
float az[ARRAYSIZE] = {};
float gx[ARRAYSIZE] = {};
float gy[ARRAYSIZE] = {};
float gz[ARRAYSIZE] = {};

int luxFlag[] ={0,1};

int i = 0;
int counter = 0;
int button_pressed = 0;
int pet_counter = 0;
int eat_counter = 0;
char input[60] = {};
int BEEP;
uint8_t uartBuffer[30]; //vastaanottopuskuri


// JTKJ: Tehtava 3. Valoisuuden globaali muuttuja
// JTKJ: Exercise 3. Global variable for ambient light
double lux = 0001.0;

double temperature = 0;

// JTKJ: Tehtava 1. Lisaa painonappien RTOS-muuttujat ja alustus
// JTKJ: Exercise 1. Add pins RTOS-variables and configuration here
static PIN_Handle buttonHandle;
static PIN_State buttonState;
static PIN_Handle ledHandle;
static PIN_State ledState;

PIN_Config buttonConfig[] = {
   Board_BUTTON0  | PIN_INPUT_EN | PIN_PULLUP | PIN_IRQ_NEGEDGE,
   PIN_TERMINATE // Asetustaulukko lopetetaan aina tällä vakiolla
};

PIN_Config ledConfig[] = {
   Board_LED0 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
   PIN_TERMINATE // Asetustaulukko lopetetaan aina tällä vakiolla
};



float movavg(float *array, uint8_t array_size, uint8_t window_size){

    float keskiarvo = 0.0;
    int i;

    for(i = 0; i<(array_size - window_size+1); i++) {
        keskiarvo = 0;
        int j;
        for(j=0; j<window_size; j++){
            keskiarvo = keskiarvo + array[i+j];
        }keskiarvo = keskiarvo/window_size;
        if(i<array_size -window_size) {
            return keskiarvo;

        } else {
            return keskiarvo;
        }
    }
    return keskiarvo;
}

void buttonFxn(PIN_Handle handle, PIN_Id pinId) {

    // JTKJ: Tehtava 1. Vilkuta jompaa kumpaa ledia
    // JTKJ: Exercise 1. Blink either led of the device
    // Vaihdetaan led-pinnin tilaa negaatiolla
    uint_t pinValue = PIN_getOutputValue( Board_LED0 );

    pinValue = !pinValue;
    PIN_setOutputValue( ledHandle, Board_LED0, pinValue );

    button_pressed = 1;
    programState = DATA_READY;
}

//kasittelijafunktio
static void uartFxn(UART_Handle handle, void *rxBuf, size_t len) {
    // Nyt meilla on siis haluttu maara merkkeja kaytettavissa
       // rxBuf-taulukossa, pituus len, jota voimme kasitella halutusti
       // Tassa ne annetaan argumentiksi toiselle funktiolle (esimerkin vuoksi)
       //(rxBuf,len);
    char msg[60];
    char alarm[60];
    if(programState == UART_MESSAGE) {
           sprintf(msg, "%c%c%c%c", input[0], input[1], input[2], input[3]);
           sprintf(alarm, "%c%c%c", input[4], input[5], input[6]);
           char compare[] = "3008";
           char versus[] = "Too";
           System_printf(msg);
           System_flush();
           System_printf(alarm);
           System_flush();

           if(strcmp(msg, compare) == 0) {
               BEEP = 1;
           }
           if(strcmp(alarm, versus) == 0) {
               BEEP = 2;
           }
           programState = WAITING;
           UART_read(handle, rxBuf, 60);
       }

       // Kasittelijan viimeisena asiana siirrytaan odottamaan uutta keskeytysta..
       //UART_read(handle, rxBuf, 1);
}


void buzzerFxn(int freq1, int freq2, int freq3, int sleep) {
    buzzerOpen(hBuzzer);

    buzzerSetFrequency(freq1);
    Task_sleep(sleep / Clock_tickPeriod);
    buzzerSetFrequency(freq2);
    Task_sleep(sleep / Clock_tickPeriod);
    buzzerSetFrequency(freq3);
    Task_sleep(sleep / Clock_tickPeriod);
    buzzerClose();
}

void musicplayer() {
    buzzerOpen(hBuzzer);

    buzzerSetFrequency(659); // E5
    Task_sleep(350000 / Clock_tickPeriod);
    buzzerSetFrequency(587); // D5
    Task_sleep(350000 / Clock_tickPeriod);
    buzzerSetFrequency(370); // FS4
    Task_sleep(175000 / Clock_tickPeriod);
    buzzerSetFrequency(415); // GS4
    Task_sleep(175000 / Clock_tickPeriod);
    buzzerSetFrequency(554); // CS5
    Task_sleep(350000 / Clock_tickPeriod);
    buzzerSetFrequency(494); // B4
    Task_sleep(350000 / Clock_tickPeriod);
    buzzerSetFrequency(294); // D4
    Task_sleep(175000 / Clock_tickPeriod);
    buzzerSetFrequency(330); // E4
    Task_sleep(175000 / Clock_tickPeriod);
    buzzerSetFrequency(494); // B4
    Task_sleep(350000 / Clock_tickPeriod);
    buzzerSetFrequency(440); // A4
    Task_sleep(350000 / Clock_tickPeriod);
    buzzerSetFrequency(277); // CS4
    Task_sleep(175000 / Clock_tickPeriod);
    buzzerSetFrequency(330); // E4
    Task_sleep(175000 / Clock_tickPeriod);
    buzzerSetFrequency(440); // A4
    Task_sleep(175000 / Clock_tickPeriod);
    buzzerSetFrequency(440); // A4
    Task_sleep(175000 / Clock_tickPeriod);
    buzzerSetFrequency(440); // A4
    Task_sleep(175000 / Clock_tickPeriod);
    buzzerSetFrequency(440); // A4
    Task_sleep(100000 / Clock_tickPeriod);


    buzzerClose();

}



/* Task Functions */
Void uartTaskFxn(UArg arg0, UArg arg1) {

    // JTKJ: tehtava 4. Lisaa UARTin alustus: 9600,8n1
    // JTKJ: Exercise 4. Setup here UART connection as 9600,8n1
       char input;
       char echo_msg[30];

       // UART-kirjaston asetukset

       /*
       UART_Handle uart;
       UART_Params uartParams;

       // Alustetaan sarjaliikenne
       UART_Params_init(&uartParams);
       uartParams.writeDataMode = UART_DATA_TEXT;
       uartParams.readDataMode = UART_DATA_TEXT;
       uartParams.readEcho = UART_ECHO_OFF;
       uartParams.readMode=UART_MODE_BLOCKING;
       uartParams.baudRate = 9600; // nopeus 9600baud
       uartParams.dataLength = UART_LEN_8; // 8
       uartParams.parityType = UART_PAR_NONE; // n
       uartParams.stopBits = UART_STOP_ONE; // 1
    */

       // TI dokumentaatiosta:
       UART_Handle uart;
       UART_Params params;

       UART_Params_init(&params);
       params.baudRate      = 9600;
       params.readMode      = UART_MODE_CALLBACK; // Keskeytyspohjainen vastaanotto
       params.readCallback  = &uartFxn; // Kasittelijafunktio
       params.readDataMode  = UART_DATA_TEXT;
       params.writeDataMode = UART_DATA_TEXT;
       params.readEcho = UART_ECHO_ON;

       // uart kayttoon ohjelmassa
       uart = UART_open(Board_UART0, &params);
          if (uart == NULL) {
             System_abort("Error opening the UART");
          }
       //UART_read(uart, uartBuffer, 1);

       System_flush();

       //float thresholdax = 1.3;


    while (1) {

        // JTKJ: Tehtava 3. Kun tila on oikea, tulosta sensoridata merkkijonossa debug-ikkunaan
        //       Muista tilamuutos
        // JTKJ: Exercise 3. Print out sensor data as string to debug window if the state is correct
        //       Remember to modify state
        if (programState == DATA_READY) {
            char debug_msg[100];
            char echo_msg3[30];

            if (lux < 150.0f && !luxFlag[0]){
                sprintf(echo_msg3,"id:3008, MSG1: I am scared it is dark in here");
                UART_write(uart, echo_msg3, strlen(echo_msg3)+1);

                char debug_msg_mpu[56];
                sprintf(debug_msg_mpu, "Ensimmainen tila. Lux on: %f  ja flag on %d \n", lux, luxFlag[0]);
                System_printf(debug_msg_mpu);
                System_flush();

                luxFlag[0]=1;

            }
            else if(lux > 200.0f && luxFlag[0]){
                sprintf(echo_msg3,"id:3008, MSG1: I'm naked!");
                UART_write(uart, echo_msg3, strlen(echo_msg3)+1);

                char debug_msg_mpu[56];
                sprintf(debug_msg_mpu, "Toinen tila. Lux on: %f  ja flag on %d \n", lux, luxFlag[0]);
                System_printf(debug_msg_mpu);

                luxFlag[0]=0;
             }

            UART_read(uart, &input, 60);
            programState = UART_MESSAGE;
            if(BEEP == 1) {

                musicplayer();
                BEEP = 0;
            }




            //sprintf(echo_msg3,"id:3008, MSG1: %f", lux);
            //UART_write(uart, echo_msg3, strlen(echo_msg3)+1);

            float movavgx = movavg(ax,ARRAYSIZE,3);
            float movavgy = movavg(ay,ARRAYSIZE,3);
            float movavgz = movavg(az,ARRAYSIZE,3);

            float movavggx = movavg(gx,ARRAYSIZE,3);
            float movavggy = movavg(gy,ARRAYSIZE,3);
            float movavggz = movavg(gz,ARRAYSIZE,3);

            //---------Collecting data
            //sprintf(debug_msg, "ax = %.3f, ay = %.3f, az = %.3f, gx = %f, gy = %f, gz = %f \n", movavgx, movavgy, movavgz, movavggx, movavggy, movavggz);
            sprintf(debug_msg, "%.3f %.3f %.3f %f %f %f \n", movavgx, movavgy, movavgz, movavggx, movavggy, movavggz);
            System_printf(debug_msg);
            System_flush();
            //-------------------------


            //----------------EAT
            if (movavggz > 80.0f && movavggx > 2.5f) {
                eat_counter++;
            }

            if(eat_counter > 4){
                sprintf(echo_msg3,"id:3008,EAT:2\0");
                UART_write(uart, echo_msg3, strlen(echo_msg3)+1);
                eat_counter=0;

                buzzerFxn(4000, 3000, 2000, 350000);

            }
            //--------------------

            //-----------------PET
            if(movavgx > 0.5f && movavggz > 10.0f) {
                pet_counter++;
            }

            if(pet_counter > 4) {
                sprintf(echo_msg3,"id:3008,PET:2\0");
                UART_write(uart, echo_msg3, strlen(echo_msg3)+1);

                buzzerFxn(2000, 3000, 4000, 350000);
                pet_counter = 0;


            }








           //-------EXERCISE
           if(movavgz > 2.0f) {
               sprintf(echo_msg3,"id:3008,EXERCISE:1\0");
               UART_write(uart, echo_msg3, strlen(echo_msg3)+1);

               buzzerFxn(5000, 5000, 5000, 350000);
               button_pressed = 0;

           }

           if (button_pressed) {

               button_pressed = 0;
               programState = MUSICPLAYER;
           }

           if(programState == MUSICPLAYER){
               musicplayer();
           }



           programState = WAITING;

        }
        // JTKJ: Tehtava 4. Laheta sama merkkijono UARTilla
        // JTKJ: Exercise 4. Send the same sensor data string with UART
        /*char echo_msg[30];
        sprintf(echo_msg, "uartTask: %f luksia\n\r", ambientLight);
        UART_write(uart, echo_msg, strlen(echo_msg));
        char echo_msg2[30];
        sprintf(echo_msg2, "id:3008,PET:1/0");
        UART_write(uart, echo_msg2, strlen(echo_msg2));
        */






        // Once per second, you can modify this
        Task_sleep(100000 / Clock_tickPeriod);
    }
}

Void sensorTaskFxn(UArg arg0, UArg arg1) {


    I2C_Handle      i2c;
    I2C_Params      i2cParams;

    I2C_Handle i2cMPU; // Own i2c-interface for MPU9250 sensor
    I2C_Params i2cMPUParams;

    I2C_Params_init(&i2cMPUParams);
       i2cMPUParams.bitRate = I2C_400kHz;
       // Note the different configuration below
       i2cMPUParams.custom = (uintptr_t)&i2cMPUCfg;

       // MPU power on
       PIN_setOutputValue(hMpuPin,Board_MPU_POWER, Board_MPU_POWER_ON);

       // Wait 100ms for the MPU sensor to power up
           Task_sleep(100000 / Clock_tickPeriod);
       System_printf("MPU9250: Power ON\n");
       System_flush();

       // MPU open i2c
       i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
       if (i2cMPU == NULL) {
           System_abort("Error Initializing I2CMPU\n");
       }

       // MPU setup and calibration
       System_printf("MPU9250: Setup and calibration...\n");
       System_flush();

       mpu9250_setup(&i2cMPU);

       System_printf("MPU9250: Setup and calibration OK\n");
       System_flush();

       I2C_close(i2cMPU);


    // JTKJ: Tehtava 2. Avaa i2c-vayla taskin kayttoon
    // JTKJ: Exercise 2. Open the i2c bus
    // RTOS:n i2c-muuttujat ja alustus

      // Muuttuja i2c-viestirakenteelle
    //I2C_Transaction i2cMessage;

    // Alustetaan i2c-väylä
    I2C_Params_init(&i2cParams);
    i2cParams.bitRate = I2C_400kHz;

       // Avataan yhteys
    i2c = I2C_open(Board_I2C_TMP, &i2cParams);
        if (i2c == NULL) {
          System_abort("Error Initializing I2C\n");
       }

    // JTKJ: Tehtava 2. Alusta sensorin OPT3001 setup-funktiolla
    //       Laita enne funktiokutsua eteen 100ms viive (Task_sleep)
    // JTKJ: Exercise 2. Setup the OPT3001 sensor for use
    //       Before calling the setup function, insertt 100ms delay with Task_sleep
        Task_sleep(10000 / Clock_tickPeriod);
        tmp007_setup(&i2c);
        opt3001_setup(&i2c);
        I2C_close(i2c);





        // JTKJ: Tehtava 2. Lue sensorilta dataa ja tulosta se Debug-ikkunaan merkkijonona
        // JTKJ: Exercise 2. Read sensor data and print it to the Debug window as string
        /*
        double lux  = opt3001_get_data(&i2c);
        char debug_msg[56];
        sprintf(debug_msg,"Luksia %f\n", lux);

        System_printf(debug_msg);
        System_flush();
        */

            while(1){

                if(counter == 5){

                    i2c = I2C_open(Board_I2C_TMP, &i2cParams);
                    //Task_sleep(10000);

                    double luxdata = opt3001_get_data(&i2c);
                    if(luxdata) {
                        lux = luxdata;
                    }


                    char debug_msg_mpu[56];
                    sprintf(debug_msg_mpu, "Newlux: %f .\n", lux);
                    System_printf(debug_msg_mpu);
                    System_flush();

                    I2C_close(i2c);
                }



        //------------------------------------MPU9250
                i2cMPU = I2C_open(Board_I2C, &i2cMPUParams);
                mpu9250_get_data(&i2cMPU, &ax[i], &ay[i], &az[i], &gx[i], &gy[i], &gz[i]);



        /*testing
        char debug_msg_mpu[56];
        sprintf(debug_msg_mpu, "sensorTask: %f suunta\n", ax[i]);
        System_printf(debug_msg_mpu);
        System_flush();
        */

//------------------------Data collection
        /*
        char debug_msg_mpu[56];
        //System_printf("ax = %.3f, ay = %.3f, az = %.3f, gx = %f, gy = %f, gz = %f \n", ax[i], ay[i], az[i], gx[i], gy[i], gz[i]);
        sprintf(debug_msg_mpu, "ax = %.3f, ay = %.3f, az = %.3f, gx = %f, gy = %f, gz = %f \n", ax[i], ay[i], az[i], gx[i], gy[i], gz[i]);
        System_printf(debug_msg_mpu);
        System_flush();
        */

                char debug_msg_mpu[56];


                I2C_close(i2cMPU);


        // JTKJ: Tehtava 3. Tallenna mittausarvo globaaliin muuttujaan
        //       Muista tilamuutos
        // JTKJ: Exercise 3. Save the sensor value into the global variable
        //       Remember to modify state
        //ambientLight = lux;

                if (counter == 9){
                    programState = DATA_READY;
                    i = 0;
                    counter = 0;
                    Task_sleep(50000 / Clock_tickPeriod);
                }

                i++;
                counter++;
        //programState = DATA_READY;
        // Just for sanity check for exercise, you can comment this out
        //System_printf("sensorTask\n");
        //System_flush();

        // Once per second, you can modify this
                Task_sleep(1000 / Clock_tickPeriod);
            }


}




Int main(void) {

    // Task variables
    Task_Handle sensorTaskHandle;
    Task_Params sensorTaskParams;
    Task_Handle uartTaskHandle;
    Task_Params uartTaskParams;

    // Initialize board
    Board_initGeneral();

    
    // JTKJ: Tehtava 2. Ota i2c-vayla kayttoon ohjelmassa
    // JTKJ: Exercise 2. Initialize i2c bus
    Board_initI2C();
    // JTKJ: Tehtava 4. Ota UART kayttoon ohjelmassa
    // JTKJ: Exercise 4. Initialize UART
    Board_initUART();

    //--------------------------------------- MPU9250
    // Open MPU power pin
    hMpuPin = PIN_open(&MpuPinState, MpuPinConfig);
    if (hMpuPin == NULL) {
        System_abort("Pin open failed!");
    }

    // JTKJ: Tehtava 1. Ota painonappi ja ledi ohjelman kayttoon
    //       Muista rekisteroida keskeytyksen kasittelija painonapille
    // JTKJ: Exercise 1. Open the button and led pins
    //       Remember to register the above interrupt handler for button

    // Otetaan pinnit käyttöön ohjelmassa
       buttonHandle = PIN_open(&buttonState, buttonConfig);
       if(!buttonHandle) {
          System_abort("Error initializing button pins\n");
       }
       ledHandle = PIN_open(&ledState, ledConfig);
       if(!ledHandle) {
          System_abort("Error initializing LED pins\n");
       }

       // Asetetaan painonappi-pinnille keskeytyksen käsittelijäksi
          // funktio buttonFxn
       if (PIN_registerIntCb(buttonHandle, &buttonFxn) != 0) {
           System_abort("Error registering button callback function");
          }



    /* Task */
    Task_Params_init(&sensorTaskParams);
    sensorTaskParams.stackSize = STACKSIZE;
    sensorTaskParams.stack = &sensorTaskStack;
    sensorTaskParams.priority=2;
    sensorTaskHandle = Task_create(sensorTaskFxn, &sensorTaskParams, NULL);
    if (sensorTaskHandle == NULL) {
        System_abort("Task create failed!");
    }

    Task_Params_init(&uartTaskParams);
    uartTaskParams.stackSize = STACKSIZE;
    uartTaskParams.stack = &uartTaskStack;
    uartTaskParams.priority=2;
    uartTaskHandle = Task_create(uartTaskFxn, &uartTaskParams, NULL);
    if (uartTaskHandle == NULL) {
        System_abort("Task create failed!");
    }



    /* Sanity check */
    System_printf("Hello world!\n");
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}
