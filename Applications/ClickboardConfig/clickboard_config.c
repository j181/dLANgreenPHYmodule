/* Standard includes. */
#include <stdio.h>
#include <string.h>

/* LPCOpen Includes. */
#include "board.h"

/* FreeRTOS includes. */
#include "FreeRTOS.h"

/* GreenPHY SDK includes. */
#include "GreenPhySDKConfig.h"

/* Project includes. */
#include "http_query_parser.h"
#include "http_request.h"
#include "clickboard_config.h"

#define ARRAY_SIZE(x) ( BaseType_t ) (sizeof( x ) / sizeof( x )[ 0 ] )

/***************************************
 *** ADD YOUR OWN CLICKBOARDS BELOW. ***
 ***************************************/

/*
 * The clickboard initializer and deinitializer functions.
 */
#if ( includeCOLOR2_CLICK != 0 )
	extern BaseType_t xColor2Click_Init( const char *pcName, BaseType_t xPort );
	extern BaseType_t xColor2Click_Deinit( void );
#endif
#if ( includeTHERMO3_CLICK != 0 )
	extern BaseType_t xThermo3Click_Init( const char *pcName, BaseType_t xPort );
	extern BaseType_t xThermo3Click_Deinit( void );
#endif
#if ( includeEXPAND2_CLICK != 0 )
	extern BaseType_t xExpand2Click_Init( const char *pcName, BaseType_t xPort );
	extern BaseType_t xExpand2Click_Deinit( void );
#endif

/* An array that holds all available clickboards, so they can be activated and
deactivated through the clickboard config interface. */
static Clickboard_t pxClickboards[ ] =
{
#if( includeCOLOR2_CLICK != 0 )
	{ "color2", xColor2Click_Init, xColor2Click_Deinit, eClickboardAllPorts, eClickboardInactive },
#endif
#if( includeTHERMO3_CLICK != 0 )
	{ "thermo3", xThermo3Click_Init, xThermo3Click_Deinit, eClickboardAllPorts, eClickboardPort2 },
#endif
#if( includeEXPAND2_CLICK != 0 )
	{ "expand2", xExpand2Click_Init, xExpand2Click_Deinit, eClickboardAllPorts, eClickboardPort1 },
#endif
};

/***************************************
 *** ADD YOUR OWN CLICKBOARDS ABOVE. ***
 ***************************************/

/*-----------------------------------------------------------*/

Clickboard_t *pxFindClickboard( char *pcName )
{
BaseType_t x;
Clickboard_t *pxClickboard = NULL;

	for( x = 0; x < ARRAY_SIZE( pxClickboards ); x++ )
	{
		if( strcmp( pxClickboards[ x ].pcName, pcName ) == 0 )
		{
			pxClickboard = &pxClickboards[ x ];
			break;
		}
	}

	return pxClickboard;
}
/*-----------------------------------------------------------*/

Clickboard_t *pxFindClickboardOnPort( BaseType_t xPort )
{
BaseType_t x;
Clickboard_t *pxClickboard = NULL;

	for( x = 0; x < ARRAY_SIZE( pxClickboards ); x++ )
	{
		if( pxClickboards[ x ].xPortsActive & xPort )
		{
			pxClickboard = &pxClickboards[ x ];
			break;
		}
	}

	return pxClickboard;
}
/*-----------------------------------------------------------*/

BaseType_t xClickboardActivate( Clickboard_t *pxClickboard, BaseType_t xPort )
{
BaseType_t xSuccess = pdFALSE;
Clickboard_t *pxClickboardOld;

	if( ( xPort > 0 ) && ( xPort <= NUM_CLICKBOARD_PORTS )
		&& ( pxClickboard != NULL ) && ( pxClickboard->xPortsAvailable & xPort ) && !( pxClickboard->xPortsActive & xPort ) )
	{
		/* Deactivate any active clickboard on the given port. */
		pxClickboardOld = pxFindClickboardOnPort( xPort );
		if( pxClickboardOld != NULL )
		{
			xClickboardDeactivate( pxClickboardOld );
		}

		/* At the moment it is not possible to have the same clickboard on
		multiple ports. Deactivate the clickboard if it is already active. */
		if( pxClickboard->xPortsActive != eClickboardInactive )
		{
			xClickboardDeactivate( pxClickboard );
		}

		/* Activate the clickboard. */
		pxClickboard->xPortsActive |= xPort;
		pxClickboard->fClickboardInit( pxClickboard->pcName, xPort );

		xSuccess = pdTRUE;
	}

	return xSuccess;
}
/*-----------------------------------------------------------*/

BaseType_t xClickboardDeactivate( Clickboard_t *pxClickboard )
{
	BaseType_t xSuccess = pdFALSE;

	if( ( pxClickboard != NULL ) && ( pxClickboard->xPortsActive != eClickboardInactive ) )
	{
		pxClickboard->fClickboardDeinit();
		pxClickboard->xPortsActive = eClickboardInactive;

		xSuccess = pdTRUE;
	}

	return xSuccess;
}
/*-----------------------------------------------------------*/

#if( includeHTTP_DEMO != 0 )

	BaseType_t xRequestHandler_Config( char *pcBuffer, size_t uxBufferLength, QueryParam_t *pxParams, BaseType_t xParamCount )
	{
	static const char * const ppcPorts[] = { "port1", "port2" };
	BaseType_t x, xCount = 0;
	QueryParam_t *pxParam;
	Clickboard_t *pxClickboard;

		/* Check if "port1" or "port2" GET parameters are set
		to activate the given clickboard. */
		for( x = 0; x < ARRAY_SIZE( ppcPorts ); x++ )
		{
			pxParam = pxFindKeyInQueryParams( ppcPorts[ x ], pxParams, xParamCount );
			if( pxParam != NULL )
			{
				/* Try to find a clickboard with passed name to activate it. */
				pxClickboard = pxFindClickboard( pxParam->pcValue );
				if( pxClickboard != NULL )
				{
					xClickboardActivate( pxClickboard, ( x + 1 ) );
				}
				/* If no clickboard could be found, check if 'none' was passed,
				to deactivate a currently active clickboard. */
				else if( strcmp( pxParam->pcValue, "none" ) == 0 )
				{
					pxClickboard = pxFindClickboardOnPort( ( x + 1 ) );
					if( pxClickboard != NULL ) {
						xClickboardDeactivate( pxClickboard );
					}
				}
			}
		}

		xCount += sprintf( pcBuffer + xCount, "{" );

		#if( netconfigUSEMQTT != 0 )
			/* _CD_ Check if "mqtt" parameter is set and activate or deactivate mqtt if requested */
			pxParam = pxFindKeyInQueryParams( "mqtt_active", pxParams, xParamCount );
			if( pxParam != NULL ) {
				if( strcmp( pxParam->pcValue, "true" ) == 0 ){
					/* _CD_ Initialize MQTT:
					 * 	-> Get TCP socket,
					 * 	-> establish connection to broker,
					 * 	-> start task to receive MQTT packages. */
					xTaskCreate( vInitMqttTask,
								 "MqttStarup",
								 240,
								 NULL,
								 ( tskIDLE_PRIORITY + 1 ),
								 NULL);
				}
				if( strcmp( pxParam->pcValue, "false" )) {
					xDeinitMqtt();
				}
			}

			xCount += sprintf( pcBuffer + xCount, "\"mqtt_online\":%d,", xIsActive() );

		#endif /* #if( netconfigUSEMQTT != 0 ) */

		/* Generate response containing all registered clickboards,
		their names and on which ports they are available and active. */
		xCount += sprintf( pcBuffer + xCount, "\"clickboards\":[" );

		for( x = 0; x < ARRAY_SIZE( pxClickboards ); x++ )
		{
			xCount += sprintf( pcBuffer + xCount , "{\"name\":\"%s\",\"available\":%d,\"active\":%d},",
						pxClickboards[ x ].pcName,
						pxClickboards[ x ].xPortsAvailable,
						pxClickboards[ x ].xPortsActive );
		}

		if( xCount > 1 )
		{
			/* Replace last (invalid) ',' with ']' */
			xCount--;
		}

		xCount += sprintf( pcBuffer + xCount, "]}" );

		return xCount;
	}

#endif /* includeHTTP_DEMO */
/*-----------------------------------------------------------*/

void xClickboardsInit()
{
BaseType_t x;

	for( x = 0; x < ARRAY_SIZE( pxClickboards ); x++ )
	{
		/* Ensure init and deinit handlers are set. */
		configASSERT( pxClickboards[ x ].fClickboardInit != NULL );
		configASSERT( pxClickboards[ x ].fClickboardDeinit != NULL );

		if( pxClickboards[ x ].xPortsActive != eClickboardInactive )
		{
			/* Ensure at max. one clickboard is active on a port. */
			configASSERT( pxFindClickboardOnPort( pxClickboards[ x ].xPortsActive ) == NULL );
			pxClickboards[ x ].fClickboardInit( pxClickboards[ x ].pcName, pxClickboards[ x ].xPortsActive );
		}
	}

	#if( includeHTTP_DEMO != 0 )
	{
		xAddRequestHandler( "config", xRequestHandler_Config );
	}
	#endif

}
/*-----------------------------------------------------------*/
