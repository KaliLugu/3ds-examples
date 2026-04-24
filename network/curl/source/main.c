#include <stdio.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <3ds.h>

typedef struct {
    char *string;
    size_t size;
} Response;

size_t write_chunk(void *data, size_t size, size_t nmemb, void *userdata);

int main() {
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);
    romfsInit();

    // Initialise le service réseau de la 3DS
    Result ret = httpcInit(0);
    if (R_FAILED(ret)) {
        fprintf(stderr, "httpcInit failed: 0x%08lX\n", ret);
        return -1;
    }
    socInit((u32*)memalign(0x1000, 0x100000), 0x100000); // socket memory

    CURL *curl;
    CURLcode result;

    curl = curl_easy_init();
    if (curl == NULL) {
        fprintf(stderr, "HTTP request failed.\n");
        return -1;
    }

    Response response;
    response.string = malloc(1);
    response.size = 0;

    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.github.com/repos/KaliLugu/Minicraft3DS/tags");
    curl_easy_setopt(curl, CURLOPT_CAINFO, "romfs:/cacert.pem"); // <-- romfs: si tu utilises le romfs
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_chunk);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);

    result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        fprintf(stderr, "Error: %s\n", curl_easy_strerror(result));
        curl_easy_cleanup(curl);
        free(response.string);
        return -1;
    }

    printf("%s\n", response.string);
    curl_easy_cleanup(curl);
    free(response.string);

    while (aptMainLoop()) {
        gspWaitForVBlank();
        gfxSwapBuffers();
        hidScanInput();

        u32 kDown = hidKeysDown();
        if (kDown & KEY_START)
            break;
        if (kDown & KEY_A) {
            aptSetChainloader(0x0004001000022400LL, 0);
            break;
        }
    }

    romfsExit();
    gfxExit();
    return 0;
}

size_t write_chunk(void *data, size_t size, size_t nmemb, void *userdata) {
    size_t real_size = size * nmemb;
    Response *response = (Response *)userdata;

    char *ptr = realloc(response->string, response->size + real_size + 1);
    if (ptr == NULL)
        return CURL_WRITEFUNC_ERROR;

    response->string = ptr;
    memcpy(&(response->string[response->size]), data, real_size);
    response->size += real_size;
    response->string[response->size] = '\0';
    return real_size;
}