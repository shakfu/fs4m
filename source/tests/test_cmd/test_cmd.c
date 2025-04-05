#include <fluidsynth.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>


int main(int argc, char **argv)
{
    fluid_settings_t *settings = NULL;
    fluid_synth_t *synth = NULL;
    fluid_player_t *player = NULL;
    fluid_midi_router_t *router = NULL;
    fluid_midi_driver_t *mdriver = NULL;
    fluid_cmd_handler_t *cmd_handler = NULL;
    fluid_audio_driver_t *adriver = NULL;

    int sfont_id;

    /* Create the settings. */
    settings = new_fluid_settings();
    if(settings == NULL)
    {
        puts("Failed to create the settings!");
        goto err;
    }

    /* Create the synthesizer. */
    synth = new_fluid_synth(settings);
    if(synth == NULL)
    {
        puts("Failed to create the synth!");
        goto err;
    }

    sfont_id = fluid_synth_sfload(synth, "examples/sf2/GM.sf2", 1);
    if(sfont_id == FLUID_FAILED)
    {
        puts("Loading the SoundFont failed!");
        goto err;
    }

    adriver = new_fluid_audio_driver(settings, synth);
    if(adriver == NULL)
    {
        puts("Failed to create the audio driver!");
        goto err;
    }

    router = new_fluid_midi_router(settings, fluid_synth_handle_midi_event, (void *)synth);
    if(router == NULL) {
        puts("Creating midi router failed.");
        goto err;
    }

    mdriver = new_fluid_midi_driver(settings, fluid_midi_router_handle_midi_event, (void *)router);
    if(mdriver == NULL) {
        puts("Creating midi driver failed.");
        goto err;
    }

    player  = new_fluid_player(synth);
    if(player == NULL) {
        puts("Creating midi player failed.");
        goto err;
    }
    fluid_player_set_playback_callback(player, fluid_midi_router_handle_midi_event, router);

    cmd_handler = new_fluid_cmd_handler2(settings, synth, router, player);
    if(cmd_handler == NULL) {
        puts("Creating cmd_handler failed.");
        goto err;
    }

    if (1) {

        if(1) {

            const char* template = "/tmp/temp.XXXXXX";
            char filename[64];
            strcpy(filename, template);

            mktemp(filename);
            int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600);
            int res = fluid_command(cmd_handler, "help\n", (fluid_ostream_t)fd);
            close(fd);

            fd = open(filename, O_RDONLY);
            if(fd == -1) {
                puts("Error Opening File!");
                goto err;
            } 

            char buffer[4096];
            memset(buffer, 0, 4096);
            size_t nbytes = sizeof(buffer);
            ssize_t bytes_read = read(fd, buffer, nbytes);
            close(fd);

            printf("%zd bytes read!\n", bytes_read);
            printf("File Contents: %s\n", buffer);

        } else {

            char *buf;
            size_t size;
            FILE* ostream = open_memstream(&buf, &size);
            dup2(fileno(ostream), STDOUT_FILENO);

            fluid_ostream_t MEMOUT_FILENO = fileno(ostream);

            int res = fluid_command(cmd_handler, "help\n", STDOUT_FILENO);
            if (res != FLUID_OK) {
                puts("cmd failed");
            }

            fclose(ostream);
            if (size)
                printf("size: %d buf: %s\n", size, buf);
            free(buf);
        }   

    } else {

        fluid_ostream_t ostream = fluid_get_stdout(); // STDOUT_FILENO
        int res = fluid_command(cmd_handler, "help\n", ostream);        
    }

    puts("done");

err:
    /* Clean up */

    if (adriver != NULL)
        delete_fluid_audio_driver(adriver);

    if (cmd_handler != NULL)
        delete_fluid_cmd_handler(cmd_handler);

    if (player != NULL)
        delete_fluid_player(player);

    if (mdriver != NULL)
        delete_fluid_midi_driver(mdriver);

    if (router != NULL)
        delete_fluid_midi_router(router);

    if (synth != NULL)
        delete_fluid_synth(synth);

    if (settings != NULL)
        delete_fluid_settings(settings);

    return 0;
}
