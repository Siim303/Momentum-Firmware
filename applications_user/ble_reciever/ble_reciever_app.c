// #include "ble_reciever_app.h"
#include <gui/gui.h>
#include <targets/furi_hal_include/furi_hal_bt.h>
#include <gui/elements.h>
#include <gui/view_dispatcher.h>

typedef struct {
    FuriMutex* core2_mtx; // Mutex to control access to Core2 resources, ensuring safe access.
    bool is_scanning; // Boolean flag to track if the app is currently scanning for packets.
    uint8_t rssi; // Holds the RSSI (Received Signal Strength Indicator) of the most recent packet.
    uint8_t
        packet_data[255]; // Array to store raw packet data, assuming max packet size of 255 bytes.
    uint16_t packet_count; // Counter to track the number of packets received.
    FuriString* status_string; // String to hold the display status (like RSSI and packet count).
    ViewDispatcher* view_dispatcher; // Manages display views and updates for GUI interaction.
} BlePacketScannerApp; // Defines the structure type as "BlePacketScannerApp"

#define EVENT_STOP 1

void ble_packet_scanner_start(BlePacketScannerApp* app) {
    if(!app->is_scanning) { // Only start scanning if it’s not already active.
        app->is_scanning = true; // Set scanning state to true.

        // Start receiving packets on BLE advertising channel 37 with data rate set to 1.
        furi_hal_bt_start_packet_rx(37, 1);
    }
}

void ble_packet_scanner_stop(BlePacketScannerApp* app) {
    if(app->is_scanning) { // Only stop scanning if it’s currently active.
        furi_hal_bt_stop_rx(); // Stop packet reception on the current BLE channel.
        app->is_scanning = false; // Set scanning state to false.
    }
}

void ble_packet_scanner_process_packet(BlePacketScannerApp* app) {
    app->rssi =
        furi_hal_bt_get_rssi(); // Read the RSSI of the most recent packet and store it in the app structure.
    app->packet_count =
        furi_hal_bt_stop_packet_test(); // Stop packet test mode and get the count of received packets.

    // Log RSSI and packet count for debugging purposes.
    FURI_LOG_I(
        "BlePacketScanner",
        "Packet received with RSSI: %d, Total packets: %d",
        app->rssi,
        app->packet_count);
}

#define EVENT_UPDATE_DISPLAY 2 // Define a custom event code

void ble_packet_scanner_update_display(BlePacketScannerApp* app) {
    // Format the display string with RSSI and packet count data.
    furi_string_printf(
        app->status_string, "RSSI: %d dBm\nPackets: %d", app->rssi, app->packet_count);

    // Pass an integer event code instead of app pointer
    view_dispatcher_send_custom_event(app->view_dispatcher, EVENT_UPDATE_DISPLAY);
}

static bool ble_packet_scanner_custom_event_callback(void* context, uint32_t event) {
    BlePacketScannerApp* app = context; // Retrieve the app context from the event callback.

    if(event == EVENT_STOP) { // Check if the event is to stop scanning.
        ble_packet_scanner_stop(app); // Stop scanning if the stop event is received.
    }
    return true; // Return true to indicate the event was handled.
}

void ble_packet_scanner_cleanup(BlePacketScannerApp* app) {
    furi_string_free(app->status_string); // Free the memory allocated for the status string.
    view_dispatcher_free(
        app->view_dispatcher); // Free the view dispatcher used for GUI management.
    free(app); // Free the app structure.
}

BlePacketScannerApp* ble_packet_scanner_app_init(void) {
    BlePacketScannerApp* app =
        malloc(sizeof(BlePacketScannerApp)); // Allocate memory for the app structure.
    app->is_scanning = false; // Set scanning to false initially, as scanning hasn’t started yet.
    app->packet_count = 0; // Initialize packet count to zero.
    app->rssi = 0; // Initialize RSSI value to zero.

    furi_hal_bt_init(); // Initialize the Bluetooth hardware and stack.
    furi_hal_bt_start_radio_stack(); // Start the Bluetooth radio stack, enabling BLE functionality.

    app->status_string =
        furi_string_alloc(); // Allocate memory for a string to display status information.
    app->view_dispatcher =
        view_dispatcher_alloc(); // Allocate memory for the view dispatcher, managing GUI views.
    view_dispatcher_set_event_callback_context(
        app->view_dispatcher, app); // Set the context for event callbacks to the app instance.
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, ble_packet_scanner_custom_event_callback);

    return app; // Return a pointer to the initialized app structure.
}

int32_t ble_packet_scanner_run(void* args) {
    UNUSED(args); // Mark args as unused since it’s not needed here.
    BlePacketScannerApp* app =
        ble_packet_scanner_app_init(); // Initialize the BLE packet scanner app.

    ble_packet_scanner_start(app); // Start scanning for BLE packets.

    while(app->is_scanning) { // Continue looping while scanning is active.
        ble_packet_scanner_process_packet(app); // Process received packets and capture RSSI.
        ble_packet_scanner_update_display(
            app); // Update the display with the latest packet information.
        furi_delay_ms(500); // Delay for 500 ms between updates to reduce processing frequency.
    }

    ble_packet_scanner_stop(app); // Stop scanning when the loop exits.
    free(app); // Free the allocated memory for the app structure.
    return 0; // Return 0 to indicate success.
}

int32_t ble_reciever_app(void* p) {
    UNUSED(p); // Suppress unused parameter warning.
    return ble_packet_scanner_run(NULL); // Call the main run function for the packet scanner app.
}
