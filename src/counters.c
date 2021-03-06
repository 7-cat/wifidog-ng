/*
 * Copyright (C) 2017 Jianhui Zhao <jianhuizhao329@gmail.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/sysinfo.h>
#include <uhttpd/uhttpd.h>
#include <libubox/ulog.h>
#include <libubox/blobmsg_json.h>

#include "http.h"
#include "config.h"
#include "auth.h"

static void counters(struct uloop_timeout *t)
{
    FILE *fp = NULL;
    char buf[1024], *p, *mac, *ip, *rx, *tx, *authed, *token;
    struct config *conf = get_config();
    struct blob_buf b;
    void *tbl, *array;

    uloop_timeout_set(t, 1000 * conf->checkinterval);

    fp = fopen("/proc/wifidog/term", "r");
    if (!fp) {
        ULOG_ERR("fopen:%s\n", strerror(errno));
        return;
    }

    memset(&b, 0, sizeof(b));
    blobmsg_buf_init(&b);
    array = blobmsg_open_array(&b, "");

    while (1) {
        if (!fgets(buf, sizeof(buf), fp))
            break;

        if (buf[0] == 'M')
            continue;

        p = buf;

        mac = strtok(p, " ");
        ip = strtok(NULL, " ");
        rx = strtok(NULL, " ");
        tx = strtok(NULL, " ");
        strtok(NULL, " ");
        authed = strtok(NULL, " ");
        token = strtok(NULL, " ");

        if (*authed != '1' || !token)
            continue;

        p = strrchr(token, '\n');
        if (p)
            *p = 0;

        tbl = blobmsg_open_table(&b, "");
        blobmsg_add_string(&b, "ip", ip);
        blobmsg_add_string(&b, "mac", mac);
        blobmsg_add_string(&b, "token", token);
        blobmsg_add_u64(&b, "incoming", atoll(rx));
        blobmsg_add_u64(&b, "outgoing", atoll(tx));
        blobmsg_close_table(&b, tbl);
    }

    blobmsg_close_table(&b, array);
    p = blobmsg_format_json(b.head, true);
    p[strlen(p) - 1] = 0;

    httppost(NULL, NULL, p + 1, "http://%s:%d%s%sstage=counters",
            conf->authserver.host, conf->authserver.port, conf->authserver.path, conf->authserver.auth_path);

    free(p);
    blob_buf_free(&b);
}

void start_counters()
{
    static struct uloop_timeout timeout = {
        .cb = counters
    };
    
    uloop_timeout_set(&timeout, 0);
}

