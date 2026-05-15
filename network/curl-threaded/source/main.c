#include <stdio.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <3ds.h>

#define STACKSIZE (8 * 1024)

typedef struct {
    char *string;
    size_t size;
} Response;

void fix_time() {
    time_t t = (time_t)(osGetTime() / 1000ULL);
    struct timespec ts = { .tv_sec = t, .tv_nsec = 0 };
    clock_settime(CLOCK_REALTIME, &ts);
}

size_t write_chunk(void *data, size_t size, size_t nmemb, void *userdata);

void networkThread(void *arg) {
    CURL     *curl;
    CURLcode  result;

    Response response;
    response.string = malloc(1);
    if (response.string == NULL) return;
    response.size = 0;

    curl = curl_easy_init();
    if (curl == NULL) {
        fprintf(stderr, "curl_easy_init failed.\n");
        free(response.string);
        return;
    }

    FILE *f = fopen("romfs:/cacert.pem", "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        rewind(f);
        char *cert = malloc(len);
        fread(cert, 1, len, f);
        fclose(f);

        struct curl_blob blob;
        blob.data  = cert;
        blob.len   = len;
        blob.flags = CURL_BLOB_COPY;
        curl_easy_setopt(curl, CURLOPT_CAINFO_BLOB, &blob);
        free(cert);
    }

    curl_easy_setopt(curl, CURLOPT_USERAGENT,     "libcurl-agent/1.0");
    curl_easy_setopt(curl, CURLOPT_URL,
                     "https://api.github.com/repos/KaliLugu/Minicraft3DS/tags");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_chunk);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     (void *)&response);

    printf("send...\n");

    result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        fprintf(stderr, "curl error: %s\n", curl_easy_strerror(result));
        curl_easy_cleanup(curl);
        free(response.string);
        return;
    }

    // parsing json pour récupérer la version
    char *pos = strstr(response.string, "\"name\"");
    if (pos != NULL) {
        char *start = strchr(pos + 6, '"');
        if (start != NULL) {
            start++;
            char *end = strchr(start, '"');
            if (end != NULL) {
                *end = '\0';
                printf("Version trouvee : %s\n", start);
                *end = '"';
            }
        }
    } else {
        printf("Version non trouvee.\n");
    }

    curl_easy_cleanup(curl);
    free(response.string);
}

int main() {
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    // fixe l'heure du processus
    fix_time();

    // Init des services
    if (R_FAILED(romfsInit())) {
        fprintf(stderr, "romfsInit failed\n");
        gfxExit();
        return -1;
    }

    if (R_FAILED(httpcInit(0))) {
        fprintf(stderr, "httpcInit failed\n");
        romfsExit();
        gfxExit();
        return -1;
    }

    u32 *socBuf = (u32 *)memalign(0x1000, 0x100000);
    if (socBuf == NULL) {
        fprintf(stderr, "memalign failed\n");
        httpcExit();
        romfsExit();
        gfxExit();
        return -1;
    }

    if (R_FAILED(socInit(socBuf, 0x100000))) {
        fprintf(stderr, "socInit failed\n");
        free(socBuf);
        httpcExit();
        romfsExit();
        gfxExit();
        return -1;
    }

    // Priorité du thread principal
    s32 prio = 0;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);

    Thread netThread = threadCreate(networkThread, NULL, STACKSIZE, prio - 1, -1, false);
    if (netThread == NULL) {
        fprintf(stderr, "threadCreate failed\n");
        socExit();
        free(socBuf);
        httpcExit();
        romfsExit();
        gfxExit();
        return -1;
    }

    printf("Thread reseau lance, appuyez sur START pour quitter.\n");

    while (aptMainLoop()) {
        gspWaitForVBlank();
        gfxSwapBuffers();
        hidScanInput();

        if (hidKeysDown() & KEY_START) break;
    }

    // Attent la fin du thread réseau
    threadJoin(netThread, U64_MAX);
    threadFree(netThread);

    httpcExit();
    socExit();
    free(socBuf);
    romfsExit();
    gfxExit();
    return 0;
}

size_t write_chunk(void *data, size_t size, size_t nmemb, void *userdata) {
    size_t    real_size = size * nmemb;
    Response *response  = (Response *)userdata;

    char *ptr = realloc(response->string, response->size + real_size + 1);
    if (ptr == NULL)
        return CURL_WRITEFUNC_ERROR;

    response->string = ptr;
    memcpy(&(response->string[response->size]), data, real_size);
    response->size += real_size;
    response->string[response->size] = '\0';
    return real_size;
}