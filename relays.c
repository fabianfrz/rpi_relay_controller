#include "mongoose.h"   // To build, run: cc main.c mongoose.c
/*#include <wiringPi.h>*/
#define HIGH 1
#define LOW 0
// dummy implementation
int digitalRead(int i) {
	return HIGH;
}
int digitalWrite(int i, int j) {
	return HIGH;
}

char* headers_json = "Content-Type: application/json\r\n";
char* format_error = "{\"ok\": false, \"error\": \"%s\"}";

unsigned int relayOutputCount = 1;
int relayOutputs[] = {17};


void ev_get_set_relays(struct mg_connection *c, struct mg_http_message *hm, struct mg_str* caps) {
	int relay_id;
	if (!mg_str_to_num(caps[0], 10, &relay_id, sizeof(relay_id))) {
		mg_http_reply(c, 400, headers_json, format_error, "Invalid Relay Id");
		return;
	}
		
	if (relay_id < 0 || relay_id >= relayOutputCount) {
		mg_http_reply(c, 404, headers_json, format_error, "Relay not found");
		return;
	}
	bool state;
	if (!mg_json_get_bool(hm->body, "$.enabled", &state)) {
		mg_http_reply(c, 400, headers_json, format_error, "Invalid state descriptor - must be 1 or 0");
		return;
	}
	if (mg_strcasecmp(mg_str("POST"), hm->method) == 0) {
		digitalWrite(relayOutputs[relay_id], state ? HIGH : LOW);
		mg_http_reply(c, 200, headers_json, "{\"ok\": true}");
		return;
	} else if (mg_strcasecmp(mg_str("GET"), hm->method) == 0) {
		int value = digitalRead(relayOutputs[relay_id]);
		mg_http_reply(c, 200, headers_json, "{\"ok\": true, \"state\": %s}", value == HIGH ? "true" : "false");
		return;
	} else {
		mg_http_reply(c, 405, headers_json, format_error, "Unsupported HTTP method");
	}
		
}

void ev_list_relays(struct mg_connection *c, struct mg_http_message *hm) {
	if (mg_strcasecmp(mg_str("GET"), hm->method) == 0) {
		int pos = 1;
		char buf[1024] = {0};
		buf[0] = '[';
		for (int i = 0; i < relayOutputCount; i++) {
			pos += sprintf(buf + pos, "%s%d", i == 1 ? "," : "", digitalRead(relayOutputs[i]));
		}
		buf[pos++] = ']';
		buf[pos++] = 0;
		
		mg_http_reply(c, 200, headers_json, "{\"ok\": true, \"states\": %s}", buf);
		return;
	} else {
		mg_http_reply(c, 405, headers_json, format_error, "Unsupported HTTP method");
		return;
	}
		
}

// HTTP server event handler function
void ev_handler(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *) ev_data;
        struct mg_str caps[2];
        if (mg_match(hm->uri, mg_str("/api/relays/*"), caps)) {
			ev_get_set_relays(c, hm, caps);
        } else if (mg_match(hm->uri, mg_str("/api/relays"), NULL)) {
			ev_list_relays(c, hm);
        } else {
            struct mg_http_serve_opts opts = { .root_dir = "./web_root/" };
            mg_http_serve_dir(c, hm, &opts);
        }
    }
}

void prepareWPI() {
	/*
    wiringPiSetupGpio();
    for (int i = 0; i < relayOutputCount; i++) {
        pinMode(relayOutputs[i], OUTPUT);
    }
	*/
}


int main(void) {
    prepareWPI();
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    mg_http_listen(&mgr, "http://[::]:8000", ev_handler, NULL);
    for (;;) {
        mg_mgr_poll(&mgr, 1000);
    }
  return 0;
}