#include "mongoose.h"
#include <wiringPi.h>
#include <systemd/sd-daemon.h>
#include <stdio.h>
#include <argon2.h>
#include <math.h>
#include <string.h>

char* headers_json = "Content-Type: application/json\r\n";
char* format_error = "{\"ok\": false, \"error\": \"%s\"}";

unsigned int relayOutputCount = 4;
int relayOutputs[] = {17, 18, 27, 22};

struct user {
  const char *name, *pass;
};

struct usernode {
    struct usernode* next;
    struct user* user;
};

struct usernode* users;

int min(int a, int b) {
    return a < b ? a : b;
}

// Parse HTTP requests, return authenticated user or NULL
static struct user *getuser(struct mg_http_message *hm) {
  char user[256], pass[256];
  struct usernode *u;
  mg_http_creds(hm, user, sizeof(user), pass, sizeof(pass));
  if (user[0] != '\0' && pass[0] != '\0') {
    u = users;
    while (u != NULL) {
      if (strcmp(user, u->user->name) == 0 && strcmp(pass, u->user->pass) == 0) {
        return u->user;
      }
    }
  }
  return NULL;
}

bool verifyHash(char* password, char* hash) {
    if (strncmp("$argon2id", hash, 9) == 0) {
        return argon2id_verify(hash.c_str(), password.c_str(), password.size()) == ARGON2_OK;
    } else if (strncmp("$argon2i", hash, 8) == 0) {
        return argon2i_verify(hash.c_str(), password.c_str(), password.size()) == ARGON2_OK;
    } else if (strncmp("$argon2d", hash, 8) == 0) {
        return argon2d_verify(hash.c_str(), password.c_str(), password.size()) == ARGON2_OK;
    } else {
        return false; // the hash is not valid
    }
}

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

        struct user *u = getuser(hm);
        if (u == NULL) {
          // WWW-Authenticate: Bearer
          mg_http_reply(c,
                        401, "Content-Type: application/json\r\n"
                             "WWW-Authenticate: realm=\"Relays\", charset=\"UTF-8\"\r\n",
                        "{\"ok\": false}");
          return;
        }
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
    wiringPiSetupGpio();
    for (int i = 0; i < relayOutputCount; i++) {
        pinMode(relayOutputs[i], OUTPUT);
	digitalWrite(relayOutputs[i], LOW);
    }
}

void readUserDatabase() {
    FILE* file = fopen("users.csv", "r");
    if (file == NULL) {
        printf("User database users.csv is missing\n");
        exit(1);
    }
    char user[256];
    char password[256];
    char buf[1024];
    struct usernode* lastNode = NULL;
    while (fscanf(file, "%1023s", buf) == 1) {
        char* passwordOffset = strchr(buf, ':');
	if (passwordOffset == NULL) {
            printf("Line is not readable: %s", buf);
	    exit(1);
	}
	strncpy(user, buf, min(passwordOffset - buf, sizeof(user) - 1));
	strncpy(password, passwordOffset + 1, sizeof(password) - 1);
        struct usernode* un = malloc(sizeof(struct usernode));
	un->user = calloc(1, sizeof(user));
	if (lastNode != NULL) {
            un->next = lastNode;
	} else {
            users = un;
	}
        lastNode = un;
	un->user->name = strdup(user);
	un->user->pass = strdup(password);
    }
    fclose(file);
    users = lastNode;
}

int main(void) {
    readUserDatabase();
    prepareWPI();
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    mg_http_listen(&mgr, "http://[::]:8000", ev_handler, NULL);
    sd_notify(0, "READY=1");
    for (;;) {
        mg_mgr_poll(&mgr, 1000);
    }
    return 0;
}

