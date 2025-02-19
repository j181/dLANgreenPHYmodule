/*
 * Copyright (c) 2016-2024, devolo solutions GmbH, Aachen, Germany.
 * All rights reserved.
 *
 * This Software is part of the devolo GreenPHY-SDK.
 *
 * Usage in source form and redistribution in binary form, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Usage in source form is subject to a current end user license agreement
 *    with the devolo solutions GmbH.
 * 2. Neither the name of the devolo solutions GmbH nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 3. Redistribution in binary form is limited to the usage on the GreenPHY
 *    module of the devolo solutions GmbH.
 * 4. Redistribution in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* Standard includes. */
#include <string.h>
#include <stdlib.h>

/* LPCOpen Includes. */
#include "board.h"

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* GreenPHY SDK includes. */
#include "GreenPhySDKConfig.h"
#include "http_query_parser.h"
#include "http_request.h"
#include "clickboard_config.h"
#include "thermo3click.h"

/* Task-Delay in ms, change to your preference */
#define TASKWAIT_THERMO3 1000 /* 1s */

/* Temperature offset used to calibrate the sensor. */
#define TEMP_OFFSET  -2.0

/*****************************************************************************/

int Get_Temperature(void)
{
	uint8_t tmp_data[2];
	int TemperatureSum;
	int Temperature;

	tmp_data[0] = 0;

	I2C_XFER_T xfer;
	xfer.slaveAddr = TMP102_I2C_ADDR;
	xfer.txBuff = tmp_data;
	xfer.rxBuff = tmp_data;
	xfer.txSz = 1;
	xfer.rxSz = 2;

	/*transfer data to T-chip via I2C and read new temperature data, check if error has occurred*/
	  if (Chip_I2C_MasterTransfer(I2C1, &xfer) != I2C_STATUS_DONE) return -450.0;

	  TemperatureSum = ((tmp_data[0] << 8) | tmp_data[1]) >> 4;  // Justify temperature values
	  if(TemperatureSum & (1 << 11))                             // Test negative bit
	    TemperatureSum |= 0xF800;                                // Set bits 11 to 15 to logic 1 to get this reading into real two complement

	  // Temperature = (float)TemperatureSum * 0.0625;              // Multiply temperature value with 0.0625 (value per bit)
	  Temperature = TemperatureSum * 625 / 100 + (TEMP_OFFSET * 100);    /* _ML_: Use an integer in hundredth of a degree instead of float. Also introduce an offset. */

	  return Temperature;                                        // Return temperature data
}

/*****************************************************************************/

/* Task handle used to identify the clickboard's task and check if the
clickboard is activated. */
static TaskHandle_t xClickTaskHandle = NULL;

/* Holds current temperature in hundredth of a degree. */
static int temp_cur;

/* Holds lowest measured temperature in hundredth of a degree.
Must be initialized to a high value.*/
static int temp_low = 0x7fffffff;
static int temp_low_time = 0;

/* Holds highest measured temperature in hundredth of a degree.
Must be initialized to a low value. */
static int temp_high = (~0x7fffffff);
static int temp_high_time = 0;

/*-----------------------------------------------------------*/

static void vClickTask(void *pvParameters)
{
const TickType_t xDelay = pdMS_TO_TICKS( TASKWAIT_THERMO3 );
BaseType_t xTime = ( portGET_RUN_TIME_COUNTER_VALUE() / 10000UL );

	for(;;)
	{
		/* Obtain Mutex. If not possible after xDelay, write debug message and proceed */
		if( xSemaphoreTake( xI2C1_Mutex, xDelay ) == pdTRUE )
		{
			/* I2C is now usable for this Task. Read temperature in hundredth of a degree. */
			temp_cur = Get_Temperature();

			/* Give Mutex back, so other Tasks can use I2C */
			xSemaphoreGive( xI2C1_Mutex );

			/* Check for lowest and highest temperatures. */
			if( temp_cur < temp_low ) {
				temp_low = temp_cur;
				temp_low_time = ( portGET_RUN_TIME_COUNTER_VALUE() / 10000UL );
			}
			if( temp_cur > temp_high ) {
				temp_high = temp_cur;
				temp_high_time = ( portGET_RUN_TIME_COUNTER_VALUE() / 10000UL );
			}

			/* Print a debug message once every 10 s. */
			if( ( portGET_RUN_TIME_COUNTER_VALUE() / 10000UL ) > xTime + 10 )
			{
				DEBUGOUT("Thermo3 - Temperature Current: %d, High: %d, Low: %d\n", temp_cur, temp_high, temp_low );
				xTime = ( portGET_RUN_TIME_COUNTER_VALUE() / 10000UL );
			}

			vTaskDelay( xDelay );
		}
		else
		{
			/* The mutex could not be obtained within xDelay. Write debug message. */
			DEBUGOUT( "Thermo3 - Error: Could not take I2C1 mutex within %d ms.\r\n", TASKWAIT_THERMO3 );
		}
	}
}
/*-----------------------------------------------------------*/

#if( includeHTTP_DEMO != 0 )
	static BaseType_t xClickHTTPRequestHandler( char *pcBuffer, size_t uxBufferLength, QueryParam_t *pxParams, BaseType_t xParamCount )
	{
	BaseType_t xCount = 0;
	QueryParam_t *pxParam;
	int time =( portGET_RUN_TIME_COUNTER_VALUE() / 10000UL );

		pxParam = pxFindKeyInQueryParams( "clear", pxParams, xParamCount );
		if( pxParam != NULL ) {
			temp_cur = Get_Temperature();
			temp_low = temp_cur;
			temp_high = temp_cur;
			temp_low_time = time;
			temp_high_time = time;

		}

		xCount += snprintf( pcBuffer, uxBufferLength,
				"{\"temp_cur\":%d,\"temp_high\":%d,\"temp_low\":%d,\"temp_high_time\":%d,\"temp_low_time\":%d}",
				temp_cur, temp_high, temp_low, ( time - temp_high_time ), ( time - temp_low_time ) );
		return xCount;
	}
#endif
/*-----------------------------------------------------------*/

BaseType_t xThermo3Click_Init ( const char *pcName, BaseType_t xPort )
{
BaseType_t xReturn = pdFALSE;

	/* Use the task handle to guard against multiple initialization. */
	if( xClickTaskHandle == NULL )
	{
		DEBUGOUT( "Initialize Thermo3Click on port %d.\r\n", xPort );

		/* Configure GPIOs depending on the microbus port. */
		if( xPort == eClickboardPort1 )
		{
			/* Set interrupt pin. */
			Chip_GPIO_SetPinDIRInput(LPC_GPIO, CLICKBOARD1_INT_GPIO_PORT_NUM, CLICKBOARD1_INT_GPIO_BIT_NUM );
		}
		else if( xPort == eClickboardPort2 )
		{
			/* Set interrupt pin. */
			Chip_GPIO_SetPinDIRInput(LPC_GPIO, CLICKBOARD2_INT_GPIO_PORT_NUM, CLICKBOARD2_INT_GPIO_BIT_NUM );
		}

		/* Initialize I2C. Both microbus ports are connected to the same I2C bus. */
		Board_I2C_Init( I2C1 );

		/* Create task. */
		xTaskCreate( vClickTask, pcName, 300, NULL, ( tskIDLE_PRIORITY + 1 ), &xClickTaskHandle );

		if( xClickTaskHandle != NULL )
		{
			#if( includeHTTP_DEMO != 0 )
			{
				/* Add HTTP request handler. */
				xAddRequestHandler( pcName, xClickHTTPRequestHandler );
			}
			#endif

			xReturn = pdTRUE;
		}
	}
	return xReturn;
}
/*-----------------------------------------------------------*/

BaseType_t xThermo3Click_Deinit ( void )
{
BaseType_t xReturn = pdFALSE;

	if( xClickTaskHandle != NULL )
	{
		DEBUGOUT( "Deinitialize Thermo3Click.\r\n" );

		#if( includeHTTP_DEMO != 0 )
		{
			/* Use the task's name to remove the HTTP Request Handler. */
			xRemoveRequestHandler( pcTaskGetName( xClickTaskHandle ) );
		}
		#endif

		/* Delete the task. */
		vTaskDelete( xClickTaskHandle );
		/* Set the task handle to NULL, so the clickboard can be reactivated. */
		xClickTaskHandle = NULL;

		/* TODO: Reset I2C and GPIOs. */
		xReturn = pdTRUE;
	}

	return xReturn;
}
/*-----------------------------------------------------------*/
