
#include "shell/shell.h"
#include "console/console.h"

#include "thermocam.h"

static int query_cam_fn(int argc, char **argv)
{
    int i;
    console_printf("framecnt: \n");
    for(i = 0; i < 64; ++i) {
        console_printf("%d ", (int)(thermocam_raw_image[i]));
        if((i+1) % 8 == 0) {
            console_printf("\n");
        }
    }
    return 0;
}

static struct shell_cmd query_cam_cmd = {
    .sc_cmd = "query_cam",
    .sc_cmd_func = query_cam_fn
};

void thermocam_shell_init(void)
{
    THERMOCAM_LOG(INFO, "Shell command init\n");
    shell_cmd_register(&query_cam_cmd);
}