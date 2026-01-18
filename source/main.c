// Include the most common headers from the C standard library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include the main libnx system header, for Switch development
#include <switch.h>

#include <jansson.h>

#define NS_APPLICATION_RECORD_SIZE 8192 // the switch currently has about 4000 games
#define SECONDS_PER_HOUR 3600
#define NANOSECONDS_PER_SECOND 1000000000
#define SECONDS_PER_MINUTE 60

typedef struct {
    u64 hours;
    u64 minutes;
    u64 seconds;
} PlaytimeConverted;

Result getInstalledApplications(NsApplicationRecord* nsApplicationRecord, s32* outEntrycount) {
    Result result = nsListApplicationRecord(nsApplicationRecord, NS_APPLICATION_RECORD_SIZE, 0, outEntrycount);
    if (R_FAILED(result)) {
        printf("Failed getting list of installed applications. Result: %d\n", result);
    }
    return result;
}

Result selectAccount(AccountUid* accountUid, PselUserSelectionSettings* pselUserSelectionSettings) {
    Result result = pselShowUserSelector(accountUid, pselUserSelectionSettings);
    if (R_FAILED(result)) {
        printf("Failed getting user id. Result: %d\n", result);
    }
    return result;
}

Result getApplicationData(u64 applicationId, AccountUid accountUid, PdmPlayStatistics* pdmPlayStatistics, NsApplicationControlData* nsApplicationControlData, size_t* actualSize, NacpLanguageEntry** nacpLanguageEntry) {
    // get play statistics of application
    Result result = pdmqryQueryPlayStatisticsByApplicationIdAndUserAccountId(applicationId, accountUid, false, pdmPlayStatistics);
    if (R_FAILED(result)) {
        printf("Failed getting time of application. Result: %d\n", result);
        return result;
    }

    // get data of application
    result = nsGetApplicationControlData(NsApplicationControlSource_Storage, applicationId, nsApplicationControlData, sizeof(NsApplicationControlData), actualSize);
    if (R_FAILED(result)) {
        printf("Failed getting data of application. Result: %d\n", result);
        return result;
    }

    // get name of application
    result = nacpGetLanguageEntry(&nsApplicationControlData->nacp, nacpLanguageEntry);
    if (R_FAILED(result)) {
        printf("Failed getting name of application. Result: %d\n", result);
        return result;
    }
    
    return result;
}

PlaytimeConverted convertPlaytime(PdmPlayStatistics pdmPlayStatistics) {
    PlaytimeConverted playtime;
    u64 totalSeconds = pdmPlayStatistics.playtime / NANOSECONDS_PER_SECOND;
    playtime.hours = totalSeconds / SECONDS_PER_HOUR;
    totalSeconds -= playtime.hours * SECONDS_PER_HOUR;
    playtime.minutes = totalSeconds / SECONDS_PER_MINUTE;
    playtime.seconds = totalSeconds - playtime.minutes * SECONDS_PER_MINUTE;
    return playtime;
}

void printGameTime(NacpLanguageEntry* nacpLanguageEntry, PlaytimeConverted playtime) {
    printf("%s - %02ld:%02ld:%02ld\n", nacpLanguageEntry->name, playtime.hours, playtime.minutes, playtime.seconds);
}

json_t* createJsonEntry(NacpLanguageEntry* nacpLanguageEntry, PdmPlayStatistics* pdmPlayStatistics) {
    // create json object for title
    json_t *stat_obj = json_object();
    if (!stat_obj) {
        printf("Warning: Failed to create stat object.\n");
        return NULL;
    }

    // fill json object with data
    json_object_set_new(stat_obj, "name", json_string(nacpLanguageEntry->name));
    json_object_set_new(stat_obj, "author", json_string(nacpLanguageEntry->author));
    json_object_set_new(stat_obj, "program_id", json_integer(pdmPlayStatistics->program_id));
    json_object_set_new(stat_obj, "first_entry_index", json_integer(pdmPlayStatistics->first_entry_index));
    json_object_set_new(stat_obj, "first_timestamp_user", json_integer(pdmPlayStatistics->first_timestamp_user));
    json_object_set_new(stat_obj, "first_timestamp_network", json_integer(pdmPlayStatistics->first_timestamp_network));
    json_object_set_new(stat_obj, "last_entry_index", json_integer(pdmPlayStatistics->last_entry_index));
    json_object_set_new(stat_obj, "last_timestamp_user", json_integer(pdmPlayStatistics->last_timestamp_user));
    json_object_set_new(stat_obj, "last_timestamp_network", json_integer(pdmPlayStatistics->last_timestamp_network));
    json_object_set_new(stat_obj, "playtime", json_integer(pdmPlayStatistics->playtime));
    json_object_set_new(stat_obj, "total_launches", json_integer(pdmPlayStatistics->total_launches));

    return stat_obj;
}

void writeJsonFile(json_t* root_array) {
    // convert root array into json string
    char * json_output_string = json_dumps(root_array, JSON_INDENT(4));

    // write json string to file
    if (json_output_string) {
        FILE *file = fopen("sdmc:/play_statistics.json", "w");
        if (file) {
            fprintf(file, "%s", json_output_string);
            fclose(file);
            printf("\nSuccessfully wrote JSON file.\n");
        } else {
            printf("\nError: Could not open sdmc:/play_statistics.json for writing.\n");
        }

        free(json_output_string);
    }
}

Result printPlayTime() {
    Result result = 0;
    NsApplicationRecord nsApplicationRecord[NS_APPLICATION_RECORD_SIZE] = {0};
    s32 outEntrycount = -1;
    AccountUid accountUid;
    PselUserSelectionSettings pselUserSelectionSettings;
    u64 applicationId = 0;
    PdmPlayStatistics pdmPlayStatistics = {0};
    NsApplicationControlData nsApplicationControlData;
    size_t actualSize = -1;
    NacpLanguageEntry * nacpLanguageEntry = NULL;
    json_t * root_array = json_array();

    if (!root_array) {
        printf("Failed creating json object");
        return -1;
    }


    memset(&nsApplicationControlData, 0, sizeof(nsApplicationControlData));
    memset(&accountUid, 0, sizeof(accountUid));
    memset(&pselUserSelectionSettings, 0, sizeof(pselUserSelectionSettings));

    // get all installed applications
    result = getInstalledApplications(nsApplicationRecord, &outEntrycount);
    if (R_FAILED(result)) {
        return result;
    }

    // select account
    result = selectAccount(&accountUid, &pselUserSelectionSettings);
    if (R_FAILED(result)) {
        return result;
    }

    // iterate over all applications
    for(int i = 0; i < outEntrycount; i++) {
        applicationId = nsApplicationRecord[i].application_id;
        
        result = getApplicationData(applicationId, accountUid, &pdmPlayStatistics, &nsApplicationControlData, &actualSize, &nacpLanguageEntry);
        if (R_FAILED(result)) {
            return result;
        }

        PlaytimeConverted playtime = convertPlaytime(pdmPlayStatistics);
        printGameTime(nacpLanguageEntry, playtime);

        json_t* jsonEntry = createJsonEntry(nacpLanguageEntry, &pdmPlayStatistics);
        if (jsonEntry) {
            json_array_append_new(root_array, jsonEntry);
        }
    }

    writeJsonFile(root_array);
    // frees root_array
    json_decref(root_array);

    return result;
}

bool initialize(PadState* pad) {
    consoleInit(NULL);

    // Configure our supported input layout: a single player with standard controller styles
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    // Initialize the default gamepad (which reads handheld mode inputs as well as the first connected controller)
    padInitializeDefault(pad);

    Result result = nsInitialize();
    if (R_FAILED(result)) {
        printf("Failed initialising ns.\n");
        return false;
    }
    result = pdmqryInitialize();
    if (R_FAILED(result)) {
        printf("Failed initialising pdmqry.\n");
        return false;

    }
    result = accountInitialize(AccountServiceType_Administrator);
    if (R_FAILED(result)) {
        printf("Failed initialising account.\n");
        return false;
    }
    return true;
}

void deinitialize() {
    // Deinitialize and clean up resources used by the console (important!)
    nsExit();
    pdmqryExit();
    accountExit();
    consoleExit(NULL);
}

// Main program entrypoint
int main(int argc, char* argv[]) {
    Result result = 0;
    PadState pad;

    bool initSuccessful = initialize(&pad);

    if (initSuccessful) {
        result = printPlayTime();
        if (R_FAILED(result)) {
            printf("Failed printing play time.\n");
        }
    }
    printf("\nPress + to exit.\n");

    // Main loop
    while (appletMainLoop())
    {
        // Scan the gamepad. This should be done once for each frame
        padUpdate(&pad);

        // padGetButtonsDown returns the set of buttons that have been
        // newly pressed in this frame compared to the previous one
        u64 kDown = padGetButtonsDown(&pad);

        if (kDown & HidNpadButton_Plus)
            break; // break in order to return to hbmenu

        // Update the console, sending a new frame to the display
        consoleUpdate(NULL);
    }

    deinitialize();
    return 0;
}
