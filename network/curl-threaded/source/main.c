#include <stdio.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <3ds.h>

#define NUMTHREADS 3
#define STACKSIZE (4 * 1024)

volatile bool runThreads = true;

typedef struct {
    char *string;
    size_t size;
} Response;

size_t write_chunk(void *data, size_t size, size_t nmemb, void *userdata);

int threadMain(void *arg) {
    romfsInit(); // pour avoir accès au certificat SSL
    // initialise le service réseau de la 3DS
    Result ret = httpcInit(0);
    if (R_FAILED(ret)) {
        return -1;
    }
    u32 *socBuf = (u32*)memalign(0x1000, 0x100000);
    if (socBuf == NULL) {
        fprintf(stderr, "memalign failed\n");
        httpcExit();
        return -1;
    }
    Result socRet = socInit(socBuf, 0x100000); // socket memory
    if (R_FAILED(socRet)) {
        fprintf(stderr, "socInit failed: 0x%08lX\n", socRet);
        free(socBuf);
        httpcExit();
        return -1;
    }

    CURL *curl;
    CURLcode result;

    curl = curl_easy_init();
    if (curl == NULL) {
        fprintf(stderr, "HTTP request failed.\n");
        socExit();
        httpcExit();
        return -1;
    }

    Response response;
    response.string = malloc(1);
    response.size = 0;

    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.github.com/repos/KaliLugu/Minicraft3DS/tags");
    curl_easy_setopt(curl, CURLOPT_CAINFO, "romfs:/cacert.pem");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_chunk);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);

    result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        fprintf(stderr, "Error: %s\n", curl_easy_strerror(result));
        curl_easy_cleanup(curl);
        free(response.string);
        return -2;
    }

    printf("%s\n", response.string);
    char *pos = strstr(response.string, "name");
    if (pos != NULL) {
        printf("version du jeu a la position %ld et les 5 prochain caractere \n", pos - response.string);
    } else {
        printf("Non trouve\n");
    }

    curl_easy_cleanup(curl);
    free(response.string);

    socExit();
    httpcExit();
    romfsExit();
}

int main() {
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    Thread threads[NUMTHREADS];
	int i;
	s32 prio = 0;
	svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
	printf("Main thread prio: 0x%lx\n", prio);

	for (i = 0; i < NUMTHREADS; i ++)
	{
		// The priority of these child threads must be higher (aka the value is lower) than that
		// of the main thread, otherwise there is thread starvation due to stdio being locked.
		threads[i] = threadCreate(threadMain, (void*)((i+1)*250), STACKSIZE, prio-1, -2, false);
		printf("created thread %d: %p\n", i, threads[i]);
	}

    while (aptMainLoop()) {
        gspWaitForVBlank();
        gfxSwapBuffers();
        hidScanInput();

        u32 kDown = hidKeysDown();
        if (kDown & KEY_START) break;
    }

    // tell threads to exit & wait for them to exit
	runThreads = false;
	for (i = 0; i < NUMTHREADS; i ++)
	{
		threadJoin(threads[i], U64_MAX);
		threadFree(threads[i]);
	}

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