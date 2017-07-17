#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include <sys/un.h>



#include <owamp/owamp.h>
#include <I2util/util.h>

#include "./owtest_utils.h"


#define TMPNAME_FMT "owtest.XXXXXX"

FILE *tmpSessionDataFile(const char *hex) {

    char *filename = (char *) malloc(sizeof TMPNAME_FMT);
    size_t nbytes = strlen(hex)/2;
    uint8_t *bytes = (uint8_t *) malloc(nbytes);
    FILE *fp = NULL;

    strcpy(filename, TMPNAME_FMT);
    int fd = mkstemp(filename);
    if (fd < 0) {
        printf("mkstemp error: %m\n");
        goto tmp_file_error;
    }

    fp = fdopen(fd, "w+b");
    if (!fp) {
        printf("fdopen error: %m\n");
        goto tmp_file_error;
    }

    if(unlink(filename)) {
        printf("unlink error: %m\n");
        goto tmp_file_error;
    }

    if(!I2HexDecode(hex, bytes, nbytes)) {
        printf("I2HexDecode error\n");
        goto tmp_file_error;
    }

    if(fwrite(bytes, 1, nbytes, fp) != nbytes) {
        printf("error writing test data\n");
        goto tmp_file_error;
    }

    rewind(fp);
    free(bytes);
    free(filename);
    return fp;

tmp_file_error:
    if (fp) {
        fclose(fp);
    }
    free(bytes);
    free(filename);
    exit(1);
}



// warning: I2ErrLogImmediate is static, effectively global
OWPContext tmpContext(char **argv) {
    char *progname;
    progname = (progname = strrchr(argv[0], '/')) ? progname+1 : *argv;

    static I2LogImmediateAttr ia;
    ia.line_info = I2NAME | I2MSG | I2LINE | I2FILE;
    ia.fp = stderr;
    I2ErrHandle eh = I2ErrOpen(progname, I2ErrLogImmediate, &ia, NULL, NULL);
    if (!eh) {
        printf("I2ErrOpen failed\n");
        exit(1);
    }

    OWPContext ctx = OWPContextCreate(eh);
    if(!ctx) {
        printf("OWPContextCreate returned NULL\n");
        exit(1);
    }
    return ctx;
}



void *server_proc(void *context) {
    int fd;

    struct _server_params *server_params
        = (struct _server_params *) context; 

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    strncpy(
        addr.sun_path,
        server_params->socket_path,
        sizeof addr.sun_path - 1);

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("error creating server socket");
        return NULL;
    }

    if(bind(fd, (struct sockaddr *) &addr, sizeof addr) == -1) {
        close(fd);
        perror("bind error");
        return NULL;
    }

   if (listen(fd, 1) == -1) {
        close(fd);
        perror("listen error");
        return NULL; 
    }

    int keep_serving = 1;
    while(keep_serving) {
        int cfd = accept(fd, NULL, NULL);
        if (cfd == -1) {
            close(fd);
            perror("accept error");
            return NULL;
        }

        keep_serving = server_params->client_proc(cfd, server_params->test_context);

        close(cfd);
    }

    close(fd);
    return NULL;
}


