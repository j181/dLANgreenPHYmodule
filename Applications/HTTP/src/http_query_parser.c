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

/* FreeRTOS includes. */
#include <FreeRTOS.h>
#include "task.h"

/* FreeRTOS+TCP includes. */
#include <FreeRTOS_IP.h>

#include "http_query_parser.h"

BaseType_t xParseQuery( char *pcQuery, QueryParam_t *pxParams, BaseType_t xMaxParams )
{
BaseType_t x = 0;

	if( ( pcQuery != NULL ) && ( *pcQuery != '\0' ) )
	{
		pxParams[x++].pcKey = pcQuery;             /* First key is at begin of query. */
		while ( ( x < xMaxParams ) && ( ( pcQuery = strchr( pcQuery, ipconfigHTTP_REQUEST_DELIMITER ) ) != NULL ) )
		{
			*pcQuery = '\0';                     /* Replace delimiter with '\0'. */
			pxParams[x].pcKey = ++pcQuery;         /* Set next parameter key. */

			/* Look for previous parameter value. */
			if( ( pxParams[x - 1].pcValue = strchr( pxParams[x - 1].pcKey, '=' ) ) != NULL) {
				*(pxParams[x - 1].pcValue)++ = '\0'; /* Replace '=' with '\0'. */
			}
			x++;
		}

		/* Look for last parameter value. */
		if ((pxParams[x - 1].pcValue = strchr(pxParams[x - 1].pcKey, '=')) != NULL) {
			*(pxParams[x - 1].pcValue)++ = '\0';     /* Replace '=' with '\0'. */
		}
	}

	return x;
}

QueryParam_t *pxFindKeyInQueryParams( const char *pcKey, QueryParam_t *pxParams, BaseType_t xParamCount )
{
BaseType_t x;
QueryParam_t *pxParam = NULL;

	for( x = 0; x < xParamCount; x++ )
	{
		if( strcmp( pxParams[ x ].pcKey, pcKey ) == 0 )
		{
			pxParam = &pxParams[ x ];
			break;
		}
	}

	return pxParam;
}
