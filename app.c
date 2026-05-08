#include "src/ui/ui_i.h"

int32_t flipperham_app(void *p)
{
    UNUSED(p);

    FlipperHamApp *app = flipperham_app_alloc();

    while (1)
    {
        app->send_requested = false;
        view_dispatcher_switch_to_view(app->view_dispatcher, app->return_view);
        view_dispatcher_run(app->view_dispatcher);

        if(app->freq_edit_active) {
            flipperham_freq_edit_enter(app);
            app->freq_edit_active = false;
            settings_menu_build(app);
            app->return_view = FlipperHamViewSettings;
            continue;
        }

        if(app->coord_edit_pending) {
            bool is_lon = (app->text_mode == 8);
            char *buf = is_lon ? app->p_lon_edit : app->p_lat_edit;
            uint8_t buf_size = is_lon ? sizeof(app->p_lon_edit) : sizeof(app->p_lat_edit);
            flipperham_coord_edit(app, buf, buf_size, is_lon);
            position_save(app);
            if(app->return_view == FlipperHamViewPosAction)
                positionActionBuild(app);
            else
                pos_edit_menu_build(app);
            app->coord_edit_pending = false;
            continue;
        }

        if (app->gps_nofix_show)
        {
            flipperham_gps_nofix_show(app);
            app->gps_nofix_show = false;
            continue;
        }

        if (app->gps_debug_active)
        {
            flipperham_gps_debug_enter(app);
            app->gps_debug_active = false;
            gps_settings_menu_build(app);
            app->return_view = FlipperHamViewGpsSettings;
            continue;
        }

        if (app->beacon_active)
        {
            flipperham_beacon_enter(app);
            continue;
        }

        if (!app->send_requested)
            break;

        flipperham_send_hardcoded_message(app);
    }

    flipperham_app_free(app);
    return 0;
}
