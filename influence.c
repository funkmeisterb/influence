
#include <stdio.h>
#include <time.h>

#include <mapper/mapper.h>

#include "influence_opengl.h"

mapper_device dev = 0;
mapper_signal sigpos[maxAgents];
mapper_signal sigobs[maxAgents];

void on_draw()
{
    mdev_poll(dev, 0);

    int i;
    for (i=0; i < maxAgents; i++)
    {
        if (agentPos[i][0] > -1 && agentPos[i][1] > -1)
            msig_update(sigobs[i], agentObs[i]);
    }
}

void on_signal_x(mapper_signal msig,
               mapper_db_signal props,
               mapper_timetag_t *timetag,
               void *value)
{
    int *pos = (int*)value;
    mapper_db_signal p = msig_properties(msig);
    int agent = (int)(long)p->user_data;
    agentPos[agent][0] = pos[0];
}

void on_signal_y(mapper_signal msig,
                 mapper_db_signal props,
                 mapper_timetag_t *timetag,
                 void *value)
{
    int *pos = (int*)value;
    mapper_db_signal p = msig_properties(msig);
    int agent = (int)(long)p->user_data;
    agentPos[agent][1] = pos[0];
}

void initMapper()
{
    long i;
    int mn = 0, mxx = WIDTH, mxy = HEIGHT;
    float fmn = 0, fmx = 1.0;

    dev = mdev_new("vector", 9000, 0);

    for (i = 0; i < maxAgents; i++)
    {
        char str[256];
        sprintf(str, "/node/%ld/position/x", i+1);
        sigpos[i] = mdev_add_input(dev, str, 1, 'i', 0,
                                   &mn, &mxx, on_signal_x, (void*)(i));
        sprintf(str, "/node/%ld/position/y", i+1);
        sigpos[i] = mdev_add_input(dev, str, 1, 'i', 0,
                                   &mn, &mxy, on_signal_y, (void*)(i));
        sprintf(str, "/node/%ld/observation", i+1);
        sigobs[i] = mdev_add_output(dev, str, 4, 'f', 0,
                                    &fmn, &fmx);
    }
}

int main(int argc, char** argv)
{
    initMapper();

    vfgl_Init(argc, argv);
    vfgl_DrawCallback = on_draw;
    vfgl_Run();

    return 0;
}
