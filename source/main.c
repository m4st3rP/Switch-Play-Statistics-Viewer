// Include the most common headers from the C standard library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include the main libnx system header, for Switch development
#include <switch.h>

#define NS_APPLICATION_RECORD_SIZE 8192 // the switch currently has about 4000 games

Result printPlayTime() {
    Result result = 0;
    NsApplicationRecord nsApplicationRecord[NS_APPLICATION_RECORD_SIZE] = {0};
    s32 outEntrycount = -1;
    AccountUid accountUid;
    PselUserSelectionSettings pselUserSelectionSettings;
    u64 applicationId = 0;
    PdmPlayStatistics pdmPlayStatistics[1] = {0};
    NsApplicationControlData nsApplicationControlData;
    size_t actualSize = -1;
    NacpLanguageEntry * nacpLanguageEntry = NULL;

    memset(&nsApplicationControlData, 0, sizeof(nsApplicationControlData));
    memset(&accountUid, 0, sizeof(accountUid));
    memset(&pselUserSelectionSettings, 0, sizeof(pselUserSelectionSettings));

    // get all installed applications
    result = nsListApplicationRecord(nsApplicationRecord, NS_APPLICATION_RECORD_SIZE, 0, &outEntrycount);
    if (R_FAILED(result)) {
        printf("Failed getting list of installed applications. Result: %d\n", result);
        return result;
    }

    // select account
    result = pselShowUserSelector(&accountUid, &pselUserSelectionSettings);
    if (R_FAILED(result)) {
        printf("Failed getting user id. Result: %d\n", result);
        return result;
    }

    // iterate over all applications
    for(int i = 0; i < outEntrycount; i++) {
        applicationId = nsApplicationRecord[i].application_id;
        
        // get play statistics of application
        result = pdmqryQueryPlayStatisticsByApplicationIdAndUserAccountId(applicationId, accountUid, false, pdmPlayStatistics);
        if (R_FAILED(result)) {
            printf("Failed getting time of application. Result: %d\n", result);
            return result;
        }

        // get data of application
        result = nsGetApplicationControlData(NsApplicationControlSource_Storage, applicationId, &nsApplicationControlData, sizeof(NsApplicationControlData), &actualSize);
        if (R_FAILED(result)) {
            printf("Failed getting data of application. Result: %d\n", result);
            return result;
        }

        // get name of application
        result = nacpGetLanguageEntry(&nsApplicationControlData.nacp, &nacpLanguageEntry);
        if (R_FAILED(result)) {
            printf("Failed getting name of application. Result: %d\n", result);
            return result;
        }

        // print application information
        u32 playtimeMinutes = pdmPlayStatistics->playtimeMinutes;
        printf("%s - %dh %dm\n", nacpLanguageEntry->name, playtimeMinutes/60, playtimeMinutes%60);
    }
    return result;
}

// Main program entrypoint
int main(int argc, char* argv[]) {
    bool initSuccessfull = true;
    Result result = 0;

    consoleInit(NULL);

    // Configure our supported input layout: a single player with standard controller styles
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    // Initialize the default gamepad (which reads handheld mode inputs as well as the first connected controller)
    PadState pad;
    padInitializeDefault(&pad);

    result = nsInitialize();
    if (R_FAILED(result)) {
        printf("Failed initialising ns.\n");
        initSuccessfull = false;
    }
    result = pdmqryInitialize();
    if (R_FAILED(result)) {
        printf("Failed initialising pdmqry.\n");
        initSuccessfull = false;

    }
    result = accountInitialize(AccountServiceType_Administrator);
    if (R_FAILED(result)) {
        printf("Failed initialising account.\n");
        initSuccessfull = false;
    }

    if (initSuccessfull) {
        result = printPlayTime();
        if (R_FAILED(result)) {
            printf("Failed printing play time.\n");
        }
    }
    printf("Press + to exit.\n");

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

    // Deinitialize and clean up resources used by the console (important!)
    nsExit();
    pdmqryExit();
    accountExit();
    consoleExit(NULL);
    return 0;
}
