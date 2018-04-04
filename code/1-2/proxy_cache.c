/**
 * System Programming Assignment #1-1 (proxy server)
 * @file proxy_cache.c
 * @author name       : Jung Dong Ho
 * @n      email      : dongho971220@gmail.com
 * @n      student id : 2016722092
 * @date Wed Mar 28 00:08:27 KST 2018
 * @warning Implemented for only a MISS case yet.
 */

#include <dirent.h>     // format of directory entries
#include <errno.h>      // system error numbers
#include <fcntl.h>      // file control options
#include <libgen.h>
#include <pwd.h>        // password structure
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>     // standard symbolic constants and types

#include <openssl/sha.h>// to use SHA1

#include <sys/stat.h>   // data returned by the stat() function
#include <sys/types.h>  // data types

/**
 * Constants in proxy_cache to avoid magic numbers.
 * @see MAX_URL   : See https://stackoverflow.com/a/417184/7899226
 * @see MAN_PATH  : Not official but refer to the linux/limits.h and wikipedia.
 */
typedef enum {
PROXY_LEN_PREFIX = 3,    ///< A length of the front hash.
PROXY_LEN_HASH   = 41,   ///< A length of the full hash(in the SHA1).
PROXY_MAX_URL    = 2048, ///< A maximum length of an url.
PROXY_MAX_PATH   = 4096, ///< A maximum length of a path.

PROXY_HIT  = 8000,
PROXY_MISS = 8001
} Proxy_constants;

/**
 * A given function for getting the home path.
 * @param home A char pointer that the current user's home path'll be assigned into.
 * @return The home path is also returned as a return value.
 */
char *getHomeDir(char *home){
    struct passwd *usr_info = getpwuid(getuid());   // get the user info into the usr_info
    strcpy(home, usr_info->pw_dir);                 // extract the home path

    return home;
}

/**
 * A given funtion for hashing an input URL with SHA1.
 * @param input_url A char pointer pointing an input URL to be hashed.
 * @param hashed_url A char pointer that the hashed URL'll be assigned into.
 * @return The hashed URL is also returned as a return value.
 * @warning There is no exception handling, note the size of pointers.
 */
char *sha1_hash(char *input_url, char *hashed_url){
    unsigned char hashed_160bits[20];   // since the SHA1 has a 160 bits size and a character is 8 bits
    char hashed_hex[41];                // since a hashed string is consists of hexadecimal characters
    unsigned int i;

    SHA1((unsigned char*)input_url, strlen(input_url), hashed_160bits); // do hashing

    // convert from binary format to hex
    for(i=0; i<sizeof(hashed_160bits); i++)
        sprintf(hashed_hex + i*2, "%02x", hashed_160bits[i]);

    strcpy(hashed_url, hashed_hex);

    return hashed_url;
}

int write_log(const char *path, const char *type, const char *msg, bool time_){
    FILE *fp        = NULL;

    char msg_time[21] = {0};

    char *msg_total       = NULL;
    size_t msg_total_size = 0;

    char *path_tmp = NULL;

    struct tm *ltp = NULL;
    time_t now;

    // try creating a file which contatins a dummy data to the path of subcache
    fp = fopen(path, "a+");
    if(fp == NULL){
        if(errno == ENOENT){
            path_tmp = (char*)malloc(sizeof(char)*strlen(path)+1);
            strcpy(path_tmp, path);
            mkdir(dirname(path_tmp), S_IRWXU | S_IRWXG | S_IRWXO);       
            fp = fopen(path, "a+");
            free(path_tmp);
        } else {
            printf("[!] %s : %s\n", path, strerror(errno));
            return EXIT_FAILURE;
        }
    }

    msg_total_size = sizeof(char) * (strlen(type) + strlen(msg) + 32);
    msg_total = (char*)malloc(msg_total_size);
    
    if(time_){
        time(&now);
        ltp = localtime(&now);
        strftime(msg_time, 32, "-[%Y/%m/%d, %T]", ltp);
    } else { 
    }

    snprintf(msg_total, msg_total_size, "[%s]%s%s\n", type, msg, msg_time);

    fwrite(msg_total, 1, strlen(msg_total), fp);
    fclose(fp);

    free(msg_total);
    return EXIT_SUCCESS;
}

/**
 * Find subcache(a back part of the hashed URL)
 * @param path_subcache A const char pointer to the path containing subcaches.
 * @param hash_back A const char pointer to be used as a subcache name.
 * @todo Implement the HIT case.
 * @return [int] HIT:0, MISS:1, FAIL:-1
 */
int find_subcache(const char *path_subcache, const char *hash_back){
    struct dirent *pFile = NULL;
    DIR           *pDir  = NULL;

    char   *path_fullcache      = NULL;
    size_t  path_fullcache_size = 0;

    // find the subcache while traversing the path
    pDir = opendir(path_subcache);
    for(pFile=readdir(pDir); pFile; pFile=readdir(pDir))
        if(strcmp(hash_back, pFile->d_name) == 0)
            break;
    closedir(pDir);

    if(pFile == NULL){  // for MISS case, should create a file to the path of subcache
        // make the full path of the subcache file
        path_fullcache_size = sizeof(char)*strlen(path_subcache) + sizeof(char)*strlen(hash_back) + 8; // 8 for a safety
        path_fullcache = (char*)malloc(path_fullcache_size);
        snprintf(path_fullcache, path_fullcache_size, "%s/%s", path_subcache, hash_back);

        write_log(path_fullcache, "TEST","DUMMY",false);

        free(path_fullcache);
        return PROXY_MISS;
    }else{              // for HIT case
        // TODO
        return PROXY_HIT;
    }
}

/**
 * Find primecache(A front part of the hashed URL).
 * @param path_primecache A const char pointer to the path containing primecaches.
 * @param hash_full A const char pointer to be used as a part of primecache name.
 * @todo Implement the HIT case.
 * @return [int] HIT:0, MISS:1, FAIL:-1
 */
int find_primecache(const char *path_primecache, const char *hash_full){
    struct dirent *pFile  = NULL;
    DIR           *pDir   = NULL;
    struct stat path_stat;

    char hash_front[PROXY_LEN_PREFIX + 1]                 = {0};
    char hash_back[PROXY_LEN_HASH - PROXY_LEN_PREFIX + 1] = {0};

    char *path_subcache       = NULL;
    size_t path_subcache_size = 0;

    int result = 0;

    // divide the entire hash into two parts
    strncpy(hash_front, hash_full, PROXY_LEN_PREFIX);
    strncpy(hash_back, hash_full + PROXY_LEN_PREFIX, PROXY_LEN_HASH - PROXY_LEN_PREFIX);

    // check whether the path of primecache exist or not
    pDir = opendir(path_primecache);
    if(pDir == NULL){   // if not exist,
        // create that path
        mkdir(path_primecache, S_IRWXU | S_IRWXG | S_IRWXO);

        pDir = opendir(path_primecache); // and try opening it again
    }
    
    // make the full path of the directory which contains subcaches
    path_subcache_size = sizeof(char)*strlen(path_primecache) + PROXY_LEN_PREFIX + 8;
    path_subcache = (char*)malloc(path_subcache_size);
    snprintf(path_subcache, path_subcache_size, "%s/%s", path_primecache, hash_front);

    // find the primecache while traversing the path
    for(pFile=readdir(pDir); pFile; pFile=readdir(pDir)){

        // if the primecache was found
        if(strcmp(hash_front, pFile->d_name) == 0){

            // check whether it is a directory or not
            stat(path_subcache, &path_stat);
            if(S_ISDIR(path_stat.st_mode)){
                break;
            }else{ // if the file was regular, then something's wrong in the cache directory
                printf("[!] find_primecache fail\n");
                printf("[+] the cache directory is corrupted\n");
                return EXIT_FAILURE;
            }
        }
    }
    closedir(pDir);
    
    // if there isn't the path of subcache, then create that path
    if(pFile == NULL)
        mkdir(path_subcache, S_IRWXU | S_IRWXG | S_IRWXO);
    
    // find the subcache in the path of the current primecache
    result = find_subcache(path_subcache, hash_back);
    free(path_subcache);
    
    return result;
}

int main(int argc, char* argv[]){

    char url_input[PROXY_MAX_URL];
    char url_hash[PROXY_LEN_HASH + 1];

    char path_cache[PROXY_MAX_PATH];
    char path_log[PROXY_MAX_PATH];

    int res = 0;

    size_t count_hit  = 0;
    size_t count_miss = 0;

    time_t time_start, time_end;

    char buf[BUFSIZ] = {0};

    time(&time_start);

    // set full permission for the current process.
    umask(0);

    // try getting current user's home path and concatenate a cache path
    if(getHomeDir(path_cache) == NULL || getHomeDir(path_log) == NULL){
        printf("[!] getHomeDir fail\n");
        return EXIT_FAILURE;
    }
    strcat(path_cache, "/cache/");
    strcat(path_log, "/logfile/logfile.txt");

    // receive an input till the input is 'bye'
    while(1){
        printf("input url> ");
        scanf("%s", url_input);

        // if input is 'bye' then break loop
        if(strcmp("bye", url_input) == 0){
            break;
        }

        // hash the input URL and find the cache with it
        sha1_hash(url_input, url_hash);
        res = find_primecache(path_cache, url_hash);

        switch(res){
            case PROXY_HIT:
                count_hit += 1;
                memmove(url_hash + 3 + 1,
                        url_hash + 3,
                        PROXY_LEN_HASH - 3);
                url_hash[3] = '/';
                write_log(path_log, "Hit", url_hash, true);
                write_log(path_log, "Hit", url_input, false);
                break;
            
            case PROXY_MISS:
                count_miss += 1;
                write_log(path_log, "Miss",url_input, true);
                break;

            default:
                break;
        }
    }
    time(&time_end);
    snprintf(buf, BUFSIZ, " run time: %f sec. #request hit : %lu, miss : %lu", difftime(time_end, time_start), count_hit, count_miss);
    write_log(path_log, "Terminated", buf, false);

    return EXIT_SUCCESS;
}
