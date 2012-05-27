
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <mapper/mapper.h>

struct _autoConnectState
{
    int seen_link;
    char *influence_device_name;
    char *xagora_device_name;
    int connected;

    mapper_device dev;
    mapper_monitor mon;
} autoConnectState;

mapper_signal sig_pos = 0,
              sig_gain = 0,
              sig_spin = 0,
              sig_fade = 0,
              sig_dir = 0,
              sig_flow = 0;

float obs[5] = {0,0,0,0,0};

#define WIDTH  500
#define HEIGHT 500

int id = 0;
int done = 0;

void make_connections()
{
    char signame1[1024], signame2[1024];
    struct _autoConnectState *acs = &autoConnectState;

    sprintf(signame1, "%s/node/observation", acs->influence_device_name);

    sprintf(signame2, "%s/observation", mdev_name(acs->dev));

    mapper_monitor_connect(acs->mon, signame1, signame2, 0, 0);

    sprintf(signame1, "%s/position", mdev_name(acs->dev));

    sprintf(signame2, "%s/node/position", acs->influence_device_name);

    mapper_monitor_connect(acs->mon, signame1, signame2, 0, 0);

    if (acs->xagora_device_name) {
        sprintf(signame2, "%s/Butterfly%d",
                acs->xagora_device_name, mdev_ordinal(acs->dev));

        mapper_monitor_connect(acs->mon, signame1, signame2, 0, 0);
    }
}

void signal_handler(mapper_signal msig,
                    int instance_id,
                    mapper_db_signal props,
                    mapper_timetag_t *timetag,
                    void *value)
{
    memcpy(obs, value, sizeof(float)*2);
    printf("observation: %f, %f\n",
           obs[0], obs[1]);
}

void link_db_callback(mapper_db_link record,
                      mapper_db_action_t action,
                      void *user)
{
    struct _autoConnectState *acs = (struct _autoConnectState*)user;

    if (acs->connected)
        return;
    if (!acs->influence_device_name || !mdev_name(acs->dev))
        return;

    if (action == MDB_NEW || action == MDB_MODIFY) {
        if (strcmp(record->src_name, mdev_name(acs->dev))==0
            &&
            strcmp(record->dest_name, acs->influence_device_name)==0)
        {
            acs->seen_link = 1;
        }
    }

    if (acs->seen_link)
    {
        float mn=-1, mx=1;
        mdev_add_input(acs->dev, "observation", 2, 'f', "norm", &mn, &mx,
                       signal_handler, 0);
        int imn=0, imx=WIDTH;
        sig_pos = mdev_add_output(acs->dev, "position", 2, 'i', 0, &imn, &imx);
        sig_gain = mdev_add_output(acs->dev, "gain", 1, 'f',
                                   "normalized", &mn, &mx);
        mx = 0.9;
        sig_fade = mdev_add_output(acs->dev, "fade", 1, 'f', "normalized", &mn, &mx);
        mn = -1.5;
        mx = 1.5;
        sig_spin = mdev_add_output(acs->dev, "spin", 1, 'f', "radians", &mn, &mx);
        mn = -3.1415926;
        mx = 3.1315926;
        sig_dir = mdev_add_output(acs->dev, "direction", 1, 'f', "radians", &mn, &mx);
        mn = -1;
        sig_flow = mdev_add_output(acs->dev, "flow", 1, 'f', "noramlized", &mn, &mx);
        

        if (sig_pos)
            acs->connected = 1;
    }
}

mapper_device autoConnect()
{
    struct _autoConnectState *acs = &autoConnectState;
    memset(acs, 0, sizeof(struct _autoConnectState));

    acs->dev = mdev_new("agent", 9000 + id, 0);

    while (!mdev_ready(acs->dev)) {
        mdev_poll(acs->dev, 100);
    }

    printf("ordinal: %d\n", mdev_ordinal(acs->dev));
    fflush(stdout);

    acs->mon = mapper_monitor_new(0, 0);
    mapper_db db = mapper_monitor_get_db(acs->mon);

    mapper_db_add_link_callback(db, link_db_callback, acs);
    int i=0;
    while (i++ < 10) {
        mdev_poll(acs->dev, 100);
        mapper_monitor_poll(acs->mon, 0);
    }

    mapper_db_device *dbdev = mapper_db_match_devices_by_name(db, "influence");
    if (dbdev) {
        acs->influence_device_name = strdup((*dbdev)->name);
        mapper_monitor_link(acs->mon, mdev_name(acs->dev), (*dbdev)->name);

        mapper_monitor_request_links_by_name(acs->mon, (*dbdev)->name);
    }
    mapper_db_device_done(dbdev);

    dbdev = mapper_db_match_devices_by_name(db, "XAgora_receiver");
    if (dbdev) {
        acs->xagora_device_name = strdup((*dbdev)->name);
        mapper_monitor_link(acs->mon, mdev_name(acs->dev), (*dbdev)->name);
        
        mapper_monitor_request_links_by_name(acs->mon, (*dbdev)->name);
    }
    mapper_db_device_done(dbdev);

    i=0;
    while (i++ < 1000 && !acs->connected) {
        mdev_poll(acs->dev, 100);
        mapper_monitor_poll(acs->mon, 0);
    }

    if (!acs->connected) {
        mdev_free(acs->dev);
        mapper_monitor_free(acs->mon);
        return 0;
    }

    make_connections();

    return acs->dev;
}

void autoDisconnect()
{
    struct _autoConnectState *acs = &autoConnectState;
    if (acs->influence_device_name) {
        mapper_monitor_unlink(acs->mon,
                              acs->influence_device_name,
                              mdev_name(acs->dev));
        free(acs->influence_device_name);
    }
    if (acs->xagora_device_name) {
        mapper_monitor_unlink(acs->mon,
                              mdev_name(acs->dev),
                              acs->xagora_device_name);
        free(acs->xagora_device_name);
    }
    if (acs->dev)
        mdev_free(acs->dev);
    memset(acs, 0, sizeof(struct _autoConnectState));
}

void ctrlc(int sig)
{
    done = 1;
}

int main(int argc, char *argv[])
{
    if (argc > 1)
        id = atoi(argv[1]);

    signal(SIGINT, ctrlc);

    mapper_device dev = autoConnect();
    if (!dev)
        goto done;

    float pos[2];
    pos[0] = rand()%WIDTH/2+WIDTH/4;
    pos[1] = rand()%HEIGHT/2+HEIGHT/4;
    float gain = 0.2;
    float damping = 0.9;
    float limit = 1;
    float vel[2] = {0,0};

    while (!done) {
        if (mdev_poll(dev, 10)) {
            vel[0] += obs[0] * gain;
            vel[1] += obs[1] * gain;
            pos[0] += vel[0];
            pos[1] += vel[1];
            vel[0] *= damping;
            vel[1] *= damping;

            if (vel[0] >  limit) vel[0] =  limit;
            if (vel[0] < -limit) vel[0] = -limit;
            if (vel[1] >  limit) vel[1] =  limit;
            if (vel[1] < -limit) vel[1] = -limit;

            if (pos[0] < 0) {
                pos[0] = 0;
                vel[0] *= -0.95;
            }

            if (pos[0] >= WIDTH) {
                pos[0] = WIDTH-1;
                vel[0] *= -0.95;
            }

            if (pos[1] < 0) {
                pos[1] = 0;
                vel[1] *= -0.95;
            }

            if (pos[1] >= HEIGHT) {
                pos[1] = HEIGHT-1;
                vel[1] *= -0.95;
            }

            int p[2];
            p[0] = (int)pos[0];
            p[1] = (int)pos[1];
            msig_update_instance(sig_pos, 0, p);
        }
    }

done:
    autoDisconnect();
    return 0;
}
