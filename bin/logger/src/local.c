/**
 * Implementation of local interface via libmicrohttpd
 *
 * @package vzlogger
 * @copyright Copyright (c) 2011, The volkszaehler.org project
 * @license http://www.gnu.org/licenses/gpl.txt GNU Public License
 * @author Steffen Vogel <info@steffenvogel.de>
 */
/*
 * This file is part of volkzaehler.org
 *
 * volkzaehler.org is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * volkzaehler.org is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with volkszaehler.org. If not, see <http://www.gnu.org/licenses/>.
 */

#include <json/json.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "vzlogger.h"
#include "channel.h"
#include "local.h"
#include "api.h"

int handle_request(void *cls, struct MHD_Connection *connection, const char *url, const char *method,
			const char *version, const char *upload_data, size_t *upload_data_size, void **con_cls) {

	int status;
	int response_code = MHD_HTTP_NOT_FOUND;

	list_t *assocs = (list_t *) cls;

	struct MHD_Response *response;
	const char *mode = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "mode");

	print(2, "Local request received: method=%s url=%s mode=%s", NULL, method, url, mode);
	
	if (strcmp(method, "GET") == 0) {
		struct timespec ts;
		struct timeval tp;
		
		struct json_object *json_obj = json_object_new_object();
		struct json_object *json_data = json_object_new_array();
		
		const char *uuid = url+1;
		const char *json_str;
	
		foreach(*assocs, it) {
			assoc_t *assoc = (assoc_t *) it->data;
		
			foreach(assoc->channels, it) {
				channel_t *ch = (channel_t *) it->data;
				
				if (strcmp(url, "/") == 0 || strcmp(ch->uuid, uuid) == 0) {
					response_code = MHD_HTTP_OK;

					/* blocking until new data arrives (comet-like blocking of HTTP response) */			
					if (strcmp(url, "/") != 0 && mode && strcmp(mode, "comet") == 0) {
						/* convert from timeval to timespec */
						gettimeofday(&tp, NULL);
						ts.tv_sec  = tp.tv_sec + COMET_TIMEOUT;
						ts.tv_nsec = tp.tv_usec * 1000;
						
						pthread_mutex_lock(&ch->buffer.mutex);
						pthread_cond_timedwait(&ch->condition, &ch->buffer.mutex, &ts);
						pthread_mutex_unlock(&ch->buffer.mutex);
					}
					
					struct json_object *json_ch = json_object_new_object();

					json_object_object_add(json_ch, "uuid", json_object_new_string(ch->uuid));
					json_object_object_add(json_ch, "middleware", json_object_new_string(ch->middleware));
					json_object_object_add(json_ch, "interval", json_object_new_int(ch->interval));

					struct json_object *json_tuples = api_json_tuples(&ch->buffer, ch->buffer.head, ch->buffer.tail);
					json_object_object_add(json_ch, "tuples", json_tuples);
					
					json_object_array_add(json_data, json_ch);
				}
			}
		}

		json_object_object_add(json_obj, "version", json_object_new_string(VERSION));
		json_object_object_add(json_obj, "generator", json_object_new_string(PACKAGE));
		json_object_object_add(json_obj, "data", json_data);

		json_str = json_object_to_json_string(json_obj);
		response = MHD_create_response_from_data(strlen(json_str), (void *) json_str, FALSE, TRUE);
		json_object_put(json_obj);

		MHD_add_response_header(response, "Content-type", "application/json");
	}
	else {
		char *response_str = strdup("not implemented\n");

		response = MHD_create_response_from_data(strlen(response_str), (void *) response_str, TRUE, FALSE);
		response_code = MHD_HTTP_METHOD_NOT_ALLOWED;

		MHD_add_response_header(response, "Content-type", "text/text");
	}
	
	
	status = MHD_queue_response(connection, response_code, response);
	
	MHD_destroy_response(response);

	return status;
}
