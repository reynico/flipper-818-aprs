#include "ui_i.h"
#include "ui_splash.h"
#include "../version.h"
#include "../afsk.h"
#include "../dra818v.h"
#include "../ax25_decode.h"

#include <furi_hal.h>
#include <furi_hal_resources.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#include <storage/storage.h>
#include <furi_hal_rtc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RX_LOG_FILE "/ext/ham/rx_log.txt"

static void status_input(InputEvent *event, void *context);
static bool dra818v_ensure_ready(FlipperHamApp *app);

void flipperham_status_view_alloc(FlipperHamApp *app)
{
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, flipperham_draw_callback, app);
    view_port_input_callback_set(app->view_port, status_input, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
}

void flipperham_status_view_free(FlipperHamApp *app)
{
    if (!app->view_port)
    {
        return;
    }

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    app->view_port = NULL;
}


void flipperham_menu_free(FlipperHamApp *app)
{
    if (app->view_dispatcher)
    {
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewSplash);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewMenu);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewSend);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewSettings);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewBulletin);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewStatus);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewMessage);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewMessageEdit);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewPosition);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewCall);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewBook);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewC2);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewPosEdit);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewPosAction);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewSsid);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewHam);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewHamTx);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewTextInput);
        view_dispatcher_remove_view(app->view_dispatcher, FlipperHamViewReadme);
        view_dispatcher_free(app->view_dispatcher);
        app->view_dispatcher = NULL;
    }

    if (app->submenu)
    {
        submenu_free(app->submenu);
        app->submenu = NULL;
    }

    if (app->send_menu)
    {
        submenu_free(app->send_menu);
        app->send_menu = NULL;
    }

    if (app->settings_menu)
    {
        variable_item_list_free(app->settings_menu);
        app->settings_menu = NULL;
    }

    if (app->ham_menu)
    {
        variable_item_list_free(app->ham_menu);
        app->ham_menu = NULL;
    }

    if (app->ham_tx_menu)
    {
        variable_item_list_free(app->ham_tx_menu);
        app->ham_tx_menu = NULL;
    }

    if (app->bulletin_menu)
    {
        submenu_free(app->bulletin_menu);
        app->bulletin_menu = NULL;
    }

    if (app->status_menu)
    {
        submenu_free(app->status_menu);
        app->status_menu = NULL;
    }

    if (app->message_menu)
    {
        submenu_free(app->message_menu);
        app->message_menu = NULL;
    }

    if (app->message_edit_menu)
    {
        submenu_free(app->message_edit_menu);
        app->message_edit_menu = NULL;
    }

    if (app->position_menu)
    {
        submenu_free(app->position_menu);
        app->position_menu = NULL;
    }

    if (app->pos_action_menu)
    {
        submenu_free(app->pos_action_menu);
        app->pos_action_menu = NULL;
    }

    if (app->call_menu)
    {
        submenu_free(app->call_menu);
        app->call_menu = NULL;
    }

    if (app->book_menu)
    {
        submenu_free(app->book_menu);
        app->book_menu = NULL;
    }

    if (app->readme_widget)
    {
        widget_free(app->readme_widget);
        app->readme_widget = NULL;
    }

    if (app->c2_menu)
    {
        submenu_free(app->c2_menu);
        app->c2_menu = NULL;
    }

    if (app->pos_edit_menu)
    {
        variable_item_list_free(app->pos_edit_menu);
        app->pos_edit_menu = NULL;
    }

    if (app->ssid_menu)
    {
        variable_item_list_free(app->ssid_menu);
        app->ssid_menu = NULL;
    }

    if (app->text_input)
    {
        text_input_free(app->text_input);
        app->text_input = NULL;
    }

    splash_view_free(app);
}

static void status_input(InputEvent *event, void *context)
{
    FlipperHamApp *app = context;

    if (event->type != InputTypeShort) return;
    if (app->debug_tx)
    {
        if (app->show_done)
        {
            if (event->key == InputKeyOk) app->repeat_more = true;
            else if (event->key == InputKeyBack) app->repeat_cancel = true;
            return;
        }
    }
    if (event->key != InputKeyBack) return;

    app->repeat_cancel = true;
}

static void tx_blink_green(void)
{
    furi_hal_light_blink_stop();
    furi_hal_light_set(LightBlue, 0);
    furi_hal_light_set(LightRed, 255);
    furi_hal_light_set(LightGreen, 72);
}

uint32_t repeat_scale(FlipperHamApp *app)
{
    UNUSED(app);
    return 1;
}

FlipperHamApp *flipperham_app_alloc(void)
{
    FlipperHamApp *app = malloc(sizeof(FlipperHamApp));

    gapp = app;
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    app->submenu = submenu_alloc();
    app->send_menu = submenu_alloc();
    app->bulletin_menu = submenu_alloc();
    app->status_menu = submenu_alloc();
    app->message_menu = submenu_alloc();
    app->message_edit_menu = submenu_alloc();
    app->position_menu = submenu_alloc();
    app->pos_action_menu = submenu_alloc();
    app->call_menu = submenu_alloc();
    app->book_menu = submenu_alloc();
    app->c2_menu = submenu_alloc();
    app->settings_menu = variable_item_list_alloc();
    app->ham_menu = variable_item_list_alloc();
    app->ham_tx_menu = variable_item_list_alloc();
    app->ssid_menu = variable_item_list_alloc();
    app->pos_edit_menu = variable_item_list_alloc();
    app->text_input = text_input_alloc();
    app->readme_widget = widget_alloc();
    app->splash_view = NULL;
    app->splash_timer = NULL;
    app->splash_cycle_timer = NULL;

    app->view_port = NULL;
    app->tx_started = false;
    app->tx_allowed = true;
    app->tx_done = false;
    app->show_done = false;
    app->send_requested = false;
    app->ham_ok = false;
    app->ham_n = 0;
    app->repeat_n = 1;
    app->leadin_ms = 50;
    app->preamble_ms = 350;
    app->repeat_i = 1;
    app->repeat_wait = false;
    app->repeat_cancel = false;
    app->repeat_more = false;
    app->tx_msg_index = 0;
    app->tx_type = 0;
    memset(app->ham_ssid, 0, sizeof(app->ham_ssid));
    memset(app->ham_has_ssid, 0, sizeof(app->ham_has_ssid));
    memset(app->ham_pass, 0, sizeof(app->ham_pass));
    memset(app->ham_calls, 0, sizeof(app->ham_calls));
    app->status_index = 0;
    app->message_index = 0;
    app->message_last_tx = 0xff;
    app->pos_index = 0;
    app->dst_call_index = 0;
    app->dst_ssid = 0;
    app->edit_call_index = 0;
    app->book_call_index = 0;
    app->ham_index = 0;
    app->ham_tx_index = 0;
    app->bulletin_sel = FlipperHamBulletinIndexAdd;
    app->status_sel = FlipperHamStatusIndexAdd;
    app->message_sel = FlipperHamMessageIndexAdd;
    app->position_sel = FlipperHamPositionIndexAdd;
    app->call_sel = FlipperHamCallIndexAdd;
    app->book_sel = FlipperHamBookIndexAdd;
    app->book_action_sel = FlipperHamC2IndexEdit;
    app->ham_sel = 0;
    app->ham_tx_sel = 0;
    app->c2_h[0] = 0;
    app->aprs_path_index = 0;
    app->aprs_path_edit[0] = 0;
    app->debug_tx = false;
    app->return_view = FlipperHamViewMenu;
    app->splash_mode = 0;
    app->splash_next_view = FlipperHamViewMenu;
    app->text_mode = 0;
    app->text_view = FlipperHamViewMenu;
    app->pkt = NULL;
    app->wave = NULL;

    app->dra.ptt_pin = &gpio_ext_pb3;
    app->dra.pd_pin = &gpio_ext_pb2;
    app->dra.sq_pin = &gpio_ext_pc3;
    app->dra.serial = NULL;
    app->dra.rx_buf = NULL;
    app->dra.ready = false;
    app->dra_freq = 144.3900f;
    app->dra_freq_index = 0;
    app->dra_volume = 8;
    app->dra_squelch = 4;
    app->rx_debug = true;
    app->rx_notify = true;
    app->has_decoded = false;
    app->rx_active = false;
    app->rx_count = 0;
    app->rx_view_port = NULL;

    cfgload(app);

    {
        static const float vhf_freqs[] = {
            144.3900f, 144.8000f, 145.1750f, 144.6400f,
            144.6600f, 145.5250f, 432.5000f,
        };
        if(app->dra_freq_index < sizeof(vhf_freqs) / sizeof(vhf_freqs[0]))
            app->dra_freq = vhf_freqs[app->dra_freq_index];
        else if(app->custom_freq_edit[0]) {
            float f = strtof(app->custom_freq_edit, NULL);
            if(f > 100.0f && f < 500.0f)
                app->dra_freq = f;
        }
    }

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    splash_view_alloc(app);
    app->return_view = splash_startup_view(app);

    submenu_add_item(app->submenu, "Send", FlipperHamMenuIndexSend, flipperham_menu_callback, app);
    submenu_add_item(
        app->submenu, "Receive", FlipperHamMenuIndexRx, flipperham_menu_callback, app);
    submenu_add_item(app->submenu, "Settings", FlipperHamMenuIndexSettings,
                     flipperham_menu_callback, app);
    submenu_add_item(app->submenu, "Callbook", FlipperHamMenuIndexCallbook,
                     flipperham_menu_callback, app);
    if (app->ham_ok)
        submenu_add_item(app->submenu, "Ham Radio", FlipperHamMenuIndexHam,
                         flipperham_menu_callback, app);
    submenu_add_item(app->submenu, "About", FlipperHamMenuIndexReadme,
                     flipperham_menu_callback, app);

    submenu_add_item(app->send_menu, "Message", FlipperHamSendIndexMessage,
                     flipperham_send_callback, app);
    submenu_add_item(app->send_menu, "Position", FlipperHamSendIndexPosition,
                     flipperham_send_callback, app);
    submenu_add_item(app->send_menu, "Status", FlipperHamSendIndexStatus, flipperham_send_callback,
                     app);
    submenu_add_item(app->send_menu, "Bulletin", FlipperHamSendIndexBulletin,
                     flipperham_send_callback, app);

    bulletin_menu_build(app);
    status_menu_build(app);
    message_menu_build(app);
    position_menu_build(app);
    call_menu_build(app);
    book_menu_build(app);
    settings_menu_build(app);
    ssidfix(app);
    ham_menu_build(app);
    ham_tx_menu_build(app);

    snprintf(
        app->readme_h, sizeof(app->readme_h),
        "818 APRS Transceiver\n"
        "by LU3ARN\n\n"
        "Version: %s\nCommit %s\nBuilt: %s\nHost: %s\n\n"
        "APRS transceiver for Flipper Zero using DRA818/SA818 VHF/UHF modules. "
        "Supports TX and RX with Bell 202 AFSK at 1200 baud.\n\n"
        "Based on flipper-ham by YO3GND\ngithub.com/yo3gnd\n\n"
        "Source code:\ngithub.com/reynico/flipper-818-aprs\n\n"
        "Ham radio license required for transmission.\n",
        APP_VER_TEXT, APP_BUILD_COMMIT, APP_BUILD_TIME, APP_BUILD_HOST);

    widget_add_text_scroll_element(
        app->readme_widget, 0, 0, 128, 52, app->readme_h);
    widget_add_button_element(app->readme_widget, GuiButtonTypeLeft, "Back", readme_back, app);

    view_set_previous_callback(submenu_get_view(app->submenu), flipperham_exit_callback);
    view_set_previous_callback(submenu_get_view(app->send_menu), flipperham_send_exit_callback);
    view_set_previous_callback(variable_item_list_get_view(app->settings_menu),
                               flipperham_settings_exit_callback);
    view_set_previous_callback(submenu_get_view(app->bulletin_menu),
                               flipperham_bulletin_exit_callback);
    view_set_previous_callback(submenu_get_view(app->status_menu), flipperham_status_exit_callback);
    view_set_previous_callback(submenu_get_view(app->message_menu),
                               flipperham_message_exit_callback);
    view_set_previous_callback(submenu_get_view(app->message_edit_menu),
                               flipperham_message_edit_exit_callback);
    view_set_previous_callback(submenu_get_view(app->position_menu),
                               flipperham_position_exit_callback);
    view_set_previous_callback(submenu_get_view(app->call_menu), flipperham_call_exit_callback);
    view_set_previous_callback(submenu_get_view(app->book_menu), book_exit);
    view_set_previous_callback(submenu_get_view(app->c2_menu), book_action_exit);
    view_set_previous_callback(variable_item_list_get_view(app->pos_edit_menu),
                               flipperham_pos_edit_exit_callback);
    view_set_previous_callback(submenu_get_view(app->pos_action_menu),
                               flipperham_pos_action_exit_callback);
    view_set_previous_callback(variable_item_list_get_view(app->ssid_menu),
                               flipperham_ssid_exit_callback);
    view_set_previous_callback(variable_item_list_get_view(app->ham_menu),
                               flipperham_ham_exit_callback);
    view_set_previous_callback(variable_item_list_get_view(app->ham_tx_menu),
                               flipperham_ham_tx_exit_callback);
    view_set_previous_callback(text_input_get_view(app->text_input), flipperham_text_exit_callback);
    view_set_previous_callback(widget_get_view(app->readme_widget),
                               flipperham_readme_exit_callback);
    variable_item_list_set_enter_callback(app->ssid_menu, ssid_enter, app);
    variable_item_list_set_enter_callback(app->settings_menu, settings_enter, app);
    variable_item_list_set_enter_callback(app->ham_menu, ham_enter, app);
    variable_item_list_set_enter_callback(app->ham_tx_menu, ham_tx_enter, app);
    variable_item_list_set_enter_callback(app->pos_edit_menu, pos_edit_enter, app);

    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewSplash, app->splash_view);
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewMenu,
                             submenu_get_view(app->submenu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewSend,
                             submenu_get_view(app->send_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewSettings,
                             variable_item_list_get_view(app->settings_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewBulletin,
                             submenu_get_view(app->bulletin_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewStatus,
                             submenu_get_view(app->status_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewMessage,
                             submenu_get_view(app->message_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewMessageEdit,
                             submenu_get_view(app->message_edit_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewPosition,
                             submenu_get_view(app->position_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewCall,
                             submenu_get_view(app->call_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewBook,
                             submenu_get_view(app->book_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewC2,
                             submenu_get_view(app->c2_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewPosEdit,
                             variable_item_list_get_view(app->pos_edit_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewPosAction,
                             submenu_get_view(app->pos_action_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewSsid,
                             variable_item_list_get_view(app->ssid_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewHam,
                             variable_item_list_get_view(app->ham_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewHamTx,
                             variable_item_list_get_view(app->ham_tx_menu));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewTextInput,
                             text_input_get_view(app->text_input));
    view_dispatcher_add_view(app->view_dispatcher, FlipperHamViewReadme,
                             widget_get_view(app->readme_widget));
    view_dispatcher_switch_to_view(app->view_dispatcher, FlipperHamViewMenu);
    return app;
}

void flipperham_app_free(FlipperHamApp *app)
{
    if (!app)
        return;

    if(app->rx_active) {
        afsk_rx_stop(&app->afsk_rx);
        app->rx_active = false;
    }
    if(app->dra.ready) {
        dra818v_deinit(&app->dra);
    }

    if (gapp == app)
        gapp = NULL;
    flipperham_status_view_free(app);
    flipperham_menu_free(app);
    if (app->pkt)
        free(app->pkt);
    if (app->wave)
        free(app->wave);
    if (app->gui)
        furi_record_close(RECORD_GUI);
    free(app);
}

void flipperham_send_hardcoded_message(FlipperHamApp *app)
{
    static const uint32_t repeat_delay_ms[] = {0, 2000, 4000, 8000, 15000};
    uint8_t i, n;
    uint32_t dt, rk, wait_ms;
    bool was_cancelled;

    if (!app->pkt)
        app->pkt = malloc(sizeof(Packet));
    if (!app->wave)
        app->wave = malloc(sizeof(uint16_t) * WAVE_N);
    if (!app->pkt || !app->wave)
    {
        if (app->pkt)
        {
            free(app->pkt);
            app->pkt = NULL;
        }
        if (app->wave)
        {
            free(app->wave);
            app->wave = NULL;
        }
        return;
    }

    flipperham_status_view_alloc(app);
    app->repeat_i = 0;
    app->repeat_t0 = furi_get_tick();
    app->repeat_to = 0;
    app->repeat_wait = false;
    app->repeat_cancel = false;
    app->repeat_more = false;
    app->show_done = false;
    furi_hal_light_blink_stop();
    furi_hal_light_set(LightBlue, 0);
    furi_hal_light_set(LightRed, 0);
    furi_hal_light_set(LightGreen, 0);
    rk = repeat_scale(app);
    furi_hal_power_suppress_charge_enter();

    n = app->repeat_n;
again:
    for (i = 0; i < n; i++)
    {
        app->repeat_i++;

        txstart(app);
        if (!app->tx_ok)
        {
            furi_hal_power_suppress_charge_exit();
            furi_hal_light_blink_stop();
            furi_hal_light_set(LightBlue, 0);
            furi_hal_light_set(LightRed, 0);
            furi_hal_light_set(LightGreen, 0);
            flipperham_status_view_free(app);
            free(app->pkt);
            free(app->wave);
            app->pkt = NULL;
            app->wave = NULL;
            return;
        }
        app->tx_started = false;
        app->tx_allowed = true;
        app->repeat_wait = false;
        app->show_done = false;

        tx_blink_green();
        view_port_update(app->view_port);
        furi_delay_ms(100);

        if(dra818v_ensure_ready(app)) {
            dra818v_ptt_on(&app->dra);
            furi_delay_ms(50);
            afsk_tx_start(&app->afsk_tx, app->wave, app->wave_len);

            while(app->afsk_tx.active) {
                view_port_update(app->view_port);
                furi_delay_ms(20);
            }

            afsk_tx_stop(&app->afsk_tx);
            furi_delay_ms(50);
            dra818v_ptt_off(&app->dra);
            app->tx_done = true;
            app->tx_started = false;
            furi_hal_light_blink_stop();
            furi_hal_light_set(LightGreen, 0);
            furi_hal_light_set(LightRed, 0);
            furi_hal_light_set(LightBlue, 0);
        }
        furi_hal_light_blink_stop();
        furi_hal_light_set(LightBlue, 0);
        furi_hal_light_set(LightRed, 0);
        furi_hal_light_set(LightGreen, 0);

        if (i + 1 >= n)
            break;

        if (app->debug_tx)
        {
            app->repeat_t0 = furi_get_tick();
            wait_ms = 2000;
        }
        else
            wait_ms = repeat_delay_ms[i + 1] * rk;
        app->repeat_to = wait_ms;
        app->repeat_wait = true;
        app->tx_done = false;

        while (1)
        {
            if (app->repeat_cancel)
                break;

            dt = furi_get_tick() - app->repeat_t0;
            if (dt >= app->repeat_to) break;

            view_port_update(app->view_port);
            furi_delay_ms(50 * rk);
        }

        app->repeat_wait = false;
        if (app->repeat_cancel)
            break;
    }

    was_cancelled = app->repeat_cancel;
    app->repeat_wait = false;
    if (!was_cancelled)
    {
        app->show_done = true;
        app->tx_done = true;
        view_port_update(app->view_port);
        if (app->debug_tx)
            while (!app->repeat_cancel && !app->repeat_more)
            {
                view_port_update(app->view_port);
                furi_delay_ms(50);
            }
        else
            furi_delay_ms(750);
    }
    else
    {
        app->show_done = false;
        app->tx_done = true;
    }
    if (app->repeat_more)
    {
        app->repeat_more = false;
        app->repeat_cancel = false;
        app->show_done = false;
        n = 1;
        goto again;
    }
    app->repeat_cancel = false;
    furi_hal_power_suppress_charge_exit();
    furi_hal_light_blink_stop();
    furi_hal_light_set(LightBlue, 0);
    furi_hal_light_set(LightRed, 0);
    furi_hal_light_set(LightGreen, 0);

    app->repeat_cancel = false;
    app->repeat_more = false;
    flipperham_status_view_free(app);
    free(app->pkt);
    free(app->wave);
    app->pkt = NULL;
    app->wave = NULL;
}

static bool dra818v_ensure_ready(FlipperHamApp *app)
{
    if(app->dra.ready) return true;

    if(!dra818v_init(&app->dra)) return false;

    if(!dra818v_handshake(&app->dra)) {
        dra818v_deinit(&app->dra);
        return false;
    }

    furi_delay_ms(200);
    dra818v_set_group(&app->dra, app->dra_freq, app->dra_freq, app->dra_squelch);
    furi_delay_ms(200);
    dra818v_set_group(&app->dra, app->dra_freq, app->dra_freq, app->dra_squelch);
    furi_delay_ms(100);
    dra818v_set_volume(&app->dra, app->dra_volume);
    furi_delay_ms(100);
    dra818v_set_filter(&app->dra, true, true, true);
    return true;
}

static void rx_log_to_sd(AprsDecoded *dec)
{
    Storage *storage = furi_record_open(RECORD_STORAGE);
    File *file = storage_file_alloc(storage);

    storage_common_mkdir(storage, "/ext/ham");

    if(storage_file_open(file, RX_LOG_FILE, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        DateTime dt;
        furi_hal_rtc_get_datetime(&dt);

        char line[192];
        const char *msg = dec->has_msg ? dec->msg_text : dec->comment;
        int len = snprintf(line, sizeof(line),
            "%04u-%02u-%02u %02u:%02u:%02u %s",
            dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second,
            dec->src);

        if(dec->has_pos)
            len += snprintf(line + len, sizeof(line) - len,
                " [%.5f,%.5f]", (double)dec->lat, (double)dec->lon);

        if(msg && msg[0])
            len += snprintf(line + len, sizeof(line) - len, " %s", msg);

        line[len++] = '\n';
        storage_file_write(file, line, len);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void rx_frame_callback(AfskFrame *frame, void *ctx)
{
    FlipperHamApp *app = ctx;
    AprsDecoded dec;

    if(aprs_decode(frame, &dec)) {
        app->rx_led_state = 1;
        app->rx_led_tick = furi_get_tick();
        furi_hal_light_set(LightGreen, 255);
        furi_hal_light_set(LightRed, 0);
        NotificationApp *notify = furi_record_open(RECORD_NOTIFICATION);
        notification_message(notify, &sequence_display_backlight_on);
        if(app->rx_notify) {
            notification_message(notify, &sequence_single_vibro);
            notification_message(notify, &sequence_success);
        }
        furi_record_close(RECORD_NOTIFICATION);
        uint8_t slot = app->rx_msg_wr < RX_MSG_MAX ? app->rx_msg_wr : RX_MSG_MAX - 1;
        if(app->rx_msg_wr >= RX_MSG_MAX) {
            for(uint8_t i = 0; i < RX_MSG_MAX - 1; i++)
                app->rx_msgs[i] = app->rx_msgs[i + 1];
        }
        app->rx_msgs[slot] = dec;
        if(app->rx_msg_wr < RX_MSG_MAX) app->rx_msg_wr++;
        app->rx_msg_view = app->rx_msg_wr - 1;
        app->rx_msg_scroll = 0;
        app->has_decoded = true;
        app->rx_count++;
        rx_log_to_sd(&dec);
    }
}

static void rx_draw(Canvas *canvas, void *ctx)
{
    FlipperHamApp *app = ctx;
    char line[44];

    snprintf(line, sizeof(line), "%d%d%.0f%.0f%lu",
        app->afsk_rx.dbg_adc_min, app->afsk_rx.dbg_adc_max,
        (double)app->afsk_rx.dbg_mark, (double)app->afsk_rx.dbg_space,
        (unsigned long)app->afsk_rx.dbg_flags);

    if(app->afsk_rx.dbg_crc_fail != app->rx_last_crc_fail) {
        app->rx_last_crc_fail = app->afsk_rx.dbg_crc_fail;
        app->rx_led_state = 2;
        app->rx_led_tick = furi_get_tick();
        furi_hal_light_set(LightRed, 255);
        furi_hal_light_set(LightGreen, 0);
    }

    if(app->rx_led_state && (furi_get_tick() - app->rx_led_tick > 500)) {
        furi_hal_light_set(LightGreen, 0);
        furi_hal_light_set(LightRed, 0);
        app->rx_led_state = 0;
    }

    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    if(app->rx_debug) {
        snprintf(line, sizeof(line), "RX %.4f MHz", (double)app->dra_freq);
    } else {
        snprintf(line, sizeof(line), "RX %.4f PKTs:%u", (double)app->dra_freq, app->rx_count);
    }
    canvas_draw_str(canvas, 0, 10, line);

    canvas_set_font(canvas, FontSecondary);

    if(app->rx_debug) {
        snprintf(
            line, sizeof(line), "ADC:%d..%d V:%u S:%u",
            app->afsk_rx.dbg_adc_min, app->afsk_rx.dbg_adc_max,
            app->dra_volume, app->dra_squelch);
        canvas_draw_str(canvas, 0, 22, line);

        snprintf(
            line, sizeof(line), "OK:%u CRC:%lu LEN:%u FL:%lu",
            app->rx_count,
            (unsigned long)app->afsk_rx.dbg_crc_fail,
            app->afsk_rx.dbg_last_frame_len,
            (unsigned long)app->afsk_rx.dbg_flags);
        canvas_draw_str(canvas, 0, 32, line);

        snprintf(
            line, sizeof(line), "HDR:%02X %02X %02X %02X",
            app->afsk_rx.frame_buf[0], app->afsk_rx.frame_buf[1],
            app->afsk_rx.frame_buf[2], app->afsk_rx.frame_buf[3]);
        canvas_draw_str(canvas, 0, 42, line);

        if(app->has_decoded) {
            uint8_t vi = app->rx_msg_view < app->rx_msg_wr ? app->rx_msg_view : 0;
            if(vi >= RX_MSG_MAX) vi = RX_MSG_MAX - 1;
            AprsDecoded *d = &app->rx_msgs[vi];
            snprintf(line, sizeof(line), "%s: %.20s", d->src,
                d->has_msg ? d->msg_text : d->comment);
            canvas_draw_str(canvas, 0, 52, line);
        }
    } else {
        if(app->has_decoded) {
            uint8_t vi = app->rx_msg_view < app->rx_msg_wr ? app->rx_msg_view : 0;
            if(vi >= RX_MSG_MAX) vi = RX_MSG_MAX - 1;
            AprsDecoded *d = &app->rx_msgs[vi];
            uint8_t idx = vi + 1;
            uint8_t total = app->rx_msg_wr;

            uint8_t y = 22;
            snprintf(line, sizeof(line), "FROM: %s [%u/%u]", d->src, idx, total);
            canvas_draw_str(canvas, 0, y, line);
            y += 10;

            if(d->has_pos) {
                snprintf(
                    line, sizeof(line), "POS: %.5f, %.5f",
                    (double)d->lat, (double)d->lon);
                canvas_draw_str(canvas, 0, y, line);
                y += 10;
            }

            const char *msg = d->has_msg ? d->msg_text : d->comment;
            if(msg && msg[0]) {
                uint8_t lines_avail = (62 - y) / 10 + 1;
                app->rx_msg_visible_lines = lines_avail;
                uint8_t line_idx = 0;
                const char *p = msg;
                while(*p) {
                    uint8_t n = 0;
                    while(p[n] && n < 42) {
                        char test[44];
                        snprintf(test, n + 2, "%s", p);
                        if(canvas_string_width(canvas, test) > 126)
                            break;
                        n++;
                    }
                    if(!n) n = 1;
                    if(line_idx >= app->rx_msg_scroll &&
                       line_idx < app->rx_msg_scroll + lines_avail) {
                        snprintf(line, sizeof(line), "%.*s", n, p);
                        canvas_draw_str(canvas, 0, y, line);
                        y += 10;
                    }
                    line_idx++;
                    p += n;
                }
                app->rx_msg_total_lines = line_idx;
            }
        }
    }
}

static void rx_input(InputEvent *event, void *ctx)
{
    FlipperHamApp *app = ctx;

    if(event->type != InputTypeShort) return;

    if(event->key == InputKeyBack) {
        app->rx_active = false;
    } else if(event->key == InputKeyDown && app->has_decoded) {
        if(app->rx_msg_total_lines > app->rx_msg_visible_lines &&
           app->rx_msg_scroll + app->rx_msg_visible_lines < app->rx_msg_total_lines)
            app->rx_msg_scroll++;
    } else if(event->key == InputKeyUp && app->has_decoded) {
        if(app->rx_msg_scroll > 0)
            app->rx_msg_scroll--;
    } else if(event->key == InputKeyRight && app->has_decoded && app->rx_msg_wr > 0) {
        uint8_t last = app->rx_msg_wr - 1;
        if(last >= RX_MSG_MAX) last = RX_MSG_MAX - 1;
        if(app->rx_msg_view < last) {
            app->rx_msg_view++;
            app->rx_msg_scroll = 0;
        }
    } else if(event->key == InputKeyLeft && app->has_decoded && app->rx_msg_wr > 0) {
        if(app->rx_msg_view > 0) {
            app->rx_msg_view--;
            app->rx_msg_scroll = 0;
        }
    }
}

void flipperham_rx_enter(FlipperHamApp *app)
{
    if(!dra818v_ensure_ready(app)) return;

    app->rx_count = 0;
    app->rx_msg_wr = 0;
    app->rx_msg_view = 0;
    app->has_decoded = false;
    app->rx_active = true;

    app->rx_view_port = view_port_alloc();
    view_port_draw_callback_set(app->rx_view_port, rx_draw, app);
    view_port_input_callback_set(app->rx_view_port, rx_input, app);
    gui_add_view_port(app->gui, app->rx_view_port, GuiLayerFullscreen);

    afsk_rx_start(&app->afsk_rx, rx_frame_callback, app);

    while(app->rx_active) {
        view_port_update(app->rx_view_port);
        furi_delay_ms(500);
    }

    afsk_rx_stop(&app->afsk_rx);
    furi_hal_light_set(LightGreen, 0);
    furi_hal_light_set(LightRed, 0);

    gui_remove_view_port(app->gui, app->rx_view_port);
    view_port_free(app->rx_view_port);
    app->rx_view_port = NULL;
}

/* ── Frequency editor ───────────────────────────────────────────── */

static void freq_edit_draw(Canvas *canvas, void *ctx)
{
    FlipperHamApp *app = ctx;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignCenter, "Set Frequency (MHz)");

    canvas_set_font(canvas, FontBigNumbers);
    char ch[2] = {0, 0};
    uint8_t glyph_h = canvas_current_font_height(canvas);
    uint8_t x = 16;
    uint8_t y = 36;

    for(uint8_t i = 0; i < 8; i++) {
        uint8_t di = i < 3 ? i : i - 1;
        if(i == 3) {
            ch[0] = '.';
        } else {
            ch[0] = '0' + app->freq_digits[di];
        }
        uint8_t cw = canvas_string_width(canvas, ch);

        if(i != 3 && di == app->freq_cursor) {
            canvas_draw_box(canvas, x - 1, y - glyph_h, cw + 2, glyph_h + 2);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, x, y, ch);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, x, y, ch);
        }
        x += cw + 1;
    }
}

static void freq_edit_input(InputEvent *event, void *ctx)
{
    FlipperHamApp *app = ctx;

    if(event->type != InputTypeShort) return;

    if(event->key == InputKeyBack) {
        app->freq_edit_active = false;
    } else if(event->key == InputKeyOk) {
        app->custom_freq_edit[0] = '0' + app->freq_digits[0];
        app->custom_freq_edit[1] = '0' + app->freq_digits[1];
        app->custom_freq_edit[2] = '0' + app->freq_digits[2];
        app->custom_freq_edit[3] = '.';
        app->custom_freq_edit[4] = '0' + app->freq_digits[3];
        app->custom_freq_edit[5] = '0' + app->freq_digits[4];
        app->custom_freq_edit[6] = '0' + app->freq_digits[5];
        app->custom_freq_edit[7] = '0' + app->freq_digits[6];
        app->custom_freq_edit[8] = 0;
        float f = strtof(app->custom_freq_edit, NULL);
        if(f > 100.0f && f < 500.0f) {
            app->dra_freq = f;
            if(app->dra.ready)
                dra818v_set_group(&app->dra, app->dra_freq, app->dra_freq, app->dra_squelch);
        }
        cfgsave(app);
        app->freq_edit_active = false;
    } else if(event->key == InputKeyLeft) {
        if(app->freq_cursor > 0) app->freq_cursor--;
    } else if(event->key == InputKeyRight) {
        if(app->freq_cursor < 6) app->freq_cursor++;
    } else if(event->key == InputKeyUp) {
        if(app->freq_digits[app->freq_cursor] < 9)
            app->freq_digits[app->freq_cursor]++;
        else
            app->freq_digits[app->freq_cursor] = 0;
    } else if(event->key == InputKeyDown) {
        if(app->freq_digits[app->freq_cursor] > 0)
            app->freq_digits[app->freq_cursor]--;
        else
            app->freq_digits[app->freq_cursor] = 9;
    }
}

void flipperham_freq_edit_enter(FlipperHamApp *app)
{
    float f = app->dra_freq;
    uint32_t whole = (uint32_t)f;
    uint32_t frac = (uint32_t)((f - (float)whole) * 10000.0f + 0.5f);

    app->freq_digits[0] = (whole / 100) % 10;
    app->freq_digits[1] = (whole / 10) % 10;
    app->freq_digits[2] = whole % 10;
    app->freq_digits[3] = (frac / 1000) % 10;
    app->freq_digits[4] = (frac / 100) % 10;
    app->freq_digits[5] = (frac / 10) % 10;
    app->freq_digits[6] = frac % 10;
    app->freq_cursor = 0;

    ViewPort *vp = view_port_alloc();
    view_port_draw_callback_set(vp, freq_edit_draw, app);
    view_port_input_callback_set(vp, freq_edit_input, app);
    gui_add_view_port(app->gui, vp, GuiLayerFullscreen);

    while(app->freq_edit_active) {
        view_port_update(vp);
        furi_delay_ms(50);
    }

    gui_remove_view_port(app->gui, vp);
    view_port_free(vp);
}

/* ── Coordinate editor ──────────────────────────────────────────── */

static void coord_edit_draw(Canvas *canvas, void *ctx)
{
    FlipperHamApp *app = ctx;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignCenter,
        app->coord_is_lon ? "Longitude" : "Latitude");

    canvas_set_font(canvas, FontBigNumbers);
    uint8_t nd = app->coord_is_lon ? 8 : 7;
    uint8_t dot_pos = app->coord_is_lon ? 3 : 2;
    char ch[2] = {0, 0};
    uint8_t glyph_h = canvas_current_font_height(canvas);
    uint8_t x = app->coord_is_lon ? 4 : 10;
    uint8_t y = 36;

    ch[0] = app->coord_sign ? '-' : '+';
    canvas_set_font(canvas, FontPrimary);
    if(app->coord_cursor == 0) {
        uint8_t sw = canvas_string_width(canvas, ch);
        canvas_draw_box(canvas, x - 1, y - glyph_h + 4, sw + 2, glyph_h + 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, x, y, ch);
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_str(canvas, x, y, ch);
    }
    x += 10;

    canvas_set_font(canvas, FontBigNumbers);
    glyph_h = canvas_current_font_height(canvas);

    for(uint8_t i = 0; i < nd; i++) {
        if(i == dot_pos) {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, x, y, ".");
            x += 5;
            canvas_set_font(canvas, FontBigNumbers);
        }
        ch[0] = '0' + app->coord_digits[i];
        uint8_t cw = canvas_string_width(canvas, ch);

        if(i + 1 == app->coord_cursor) {
            canvas_draw_box(canvas, x - 1, y - glyph_h, cw + 2, glyph_h + 2);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, x, y, ch);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, x, y, ch);
        }
        x += cw + 1;
    }
}

static void coord_edit_input(InputEvent *event, void *ctx)
{
    FlipperHamApp *app = ctx;
    uint8_t nd = app->coord_is_lon ? 8 : 7;

    if(event->type != InputTypeShort) return;

    if(event->key == InputKeyBack) {
        app->coord_edit_active = false;
    } else if(event->key == InputKeyOk) {
        app->coord_edit_active = false;
    } else if(event->key == InputKeyLeft) {
        if(app->coord_cursor > 0) app->coord_cursor--;
    } else if(event->key == InputKeyRight) {
        if(app->coord_cursor < nd) app->coord_cursor++;
    } else if(event->key == InputKeyUp) {
        if(app->coord_cursor == 0) {
            app->coord_sign = !app->coord_sign;
        } else {
            uint8_t di = app->coord_cursor - 1;
            if(app->coord_digits[di] < 9) app->coord_digits[di]++;
            else app->coord_digits[di] = 0;
        }
    } else if(event->key == InputKeyDown) {
        if(app->coord_cursor == 0) {
            app->coord_sign = !app->coord_sign;
        } else {
            uint8_t di = app->coord_cursor - 1;
            if(app->coord_digits[di] > 0) app->coord_digits[di]--;
            else app->coord_digits[di] = 9;
        }
    }
}

void flipperham_coord_edit(FlipperHamApp *app, char *buf, uint8_t buf_size, bool is_lon)
{
    float val = strtof(buf, NULL);
    app->coord_sign = val < 0;
    if(val < 0) val = -val;
    app->coord_is_lon = is_lon;
    app->coord_cursor = 0;

    uint32_t whole = (uint32_t)val;
    uint32_t frac = (uint32_t)((val - (float)whole) * 100000.0f + 0.5f);

    if(is_lon) {
        app->coord_digits[0] = (whole / 100) % 10;
        app->coord_digits[1] = (whole / 10) % 10;
        app->coord_digits[2] = whole % 10;
        app->coord_digits[3] = (frac / 10000) % 10;
        app->coord_digits[4] = (frac / 1000) % 10;
        app->coord_digits[5] = (frac / 100) % 10;
        app->coord_digits[6] = (frac / 10) % 10;
        app->coord_digits[7] = frac % 10;
    } else {
        app->coord_digits[0] = (whole / 10) % 10;
        app->coord_digits[1] = whole % 10;
        app->coord_digits[2] = (frac / 10000) % 10;
        app->coord_digits[3] = (frac / 1000) % 10;
        app->coord_digits[4] = (frac / 100) % 10;
        app->coord_digits[5] = (frac / 10) % 10;
        app->coord_digits[6] = frac % 10;
    }

    app->coord_edit_active = true;

    ViewPort *vp = view_port_alloc();
    view_port_draw_callback_set(vp, coord_edit_draw, app);
    view_port_input_callback_set(vp, coord_edit_input, app);
    gui_add_view_port(app->gui, vp, GuiLayerFullscreen);

    while(app->coord_edit_active) {
        view_port_update(vp);
        furi_delay_ms(50);
    }

    gui_remove_view_port(app->gui, vp);
    view_port_free(vp);

    if(is_lon) {
        snprintf(buf, buf_size, "%s%d%d%d.%d%d%d%d%d",
            app->coord_sign ? "-" : "",
            (int)app->coord_digits[0], (int)app->coord_digits[1],
            (int)app->coord_digits[2], (int)app->coord_digits[3],
            (int)app->coord_digits[4], (int)app->coord_digits[5],
            (int)app->coord_digits[6], (int)app->coord_digits[7]);
    } else {
        snprintf(buf, buf_size, "%s%d%d.%d%d%d%d%d",
            app->coord_sign ? "-" : "",
            (int)app->coord_digits[0], (int)app->coord_digits[1],
            (int)app->coord_digits[2], (int)app->coord_digits[3],
            (int)app->coord_digits[4], (int)app->coord_digits[5],
            (int)app->coord_digits[6]);
    }
}
