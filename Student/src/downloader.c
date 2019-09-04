#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#include "http.h"
#include "queue.h"

#define FILE_SIZE 256

typedef struct {
    char *url;
    int min_range;
    int max_range;
    Buffer *result;
}  Task;


typedef struct {
    Queue *todo;
    Queue *done;

    pthread_t *threads;
    int num_workers;

} Context;

void create_directory(const char *dir) {
    struct stat st = { 0 };

    if (stat(dir, &st) == -1) {
        int rc = mkdir(dir, 0700);
        if (rc == -1) {
            perror("mkdir");
            exit(EXIT_FAILURE);
        }
    }
}


void *worker_thread(void *arg) {
    Context *context = (Context *)arg;

    Task *task = (Task *)queue_get(context->todo);
    char *range = (char *)malloc(1024 * sizeof(char));
    
    while (task) {
        snprintf(range, 1024 * sizeof(char), "%d-%d", task->min_range, 
        task->max_range);
    
        task->result = http_url(task->url, range);

        queue_put(context->done, task);
        task = (Task *)queue_get(context->todo);
    }
    
    free(range);
    return NULL;
}


Context *spawn_workers(int num_workers) {
    Context *context = (Context*)malloc(sizeof(Context));

    context->todo = queue_alloc(num_workers * 2);
    context->done = queue_alloc(num_workers * 2);

    context->num_workers = num_workers;

    context->threads = (pthread_t*)malloc(sizeof(pthread_t) * num_workers);
    int i = 0;

    for (i = 0; i < num_workers; ++i) {
        if (pthread_create(&context->threads[i], NULL, worker_thread, context) != 0) {
            perror("pthread_create");
            exit(1);
        }
    }

    return context;
}

void free_workers(Context *context) {
    int num_workers = context->num_workers;
    int i = 0;

    for (i = 0; i < num_workers; ++i) {
        queue_put(context->todo, NULL);
    }

    for (i = 0; i < num_workers; ++i) {
        if (pthread_join(context->threads[i], NULL) != 0) {
            perror("pthread_join");
            exit(1);
        }
    }

    queue_free(context->todo);
    queue_free(context->done);

    free(context->threads);
    free(context);
}


Task *new_task(char *url, int min_range, int max_range) {
    Task *task = malloc(sizeof(Task));
    task->result = NULL;
    task->url = malloc(strlen(url) + 1);
    task->min_range = min_range;
    task->max_range = max_range;

    strcpy(task->url, url);

    return task;
}

void free_task(Task *task) {

    if (task->result) {
        free(task->result->data);
        free(task->result);
    }

    free(task->url);
    free(task);
}


void wait_task(const char *download_dir, Context *context) {
    char filename[FILE_SIZE], url_file[FILE_SIZE];
    Task *task = (Task*)queue_get(context->done);

    if (task->result) {

        snprintf(url_file, FILE_SIZE * sizeof(char), "%d", task->min_range);
        size_t len = strlen(url_file);
        for (int i = 0; i < len; ++i) {
            if (url_file[i] == '/') {
                url_file[i] = '|';
            }
        }


        snprintf(filename, FILE_SIZE, "%s/%s", download_dir, url_file);
        FILE *fp = fopen(filename, "w");

        if (fp == NULL) {
            fprintf(stderr, "error writing to: %s\n", filename);
            exit(EXIT_FAILURE);
        }

        char *data = http_get_content(task->result);
        if (data) {
            size_t length = task->result->length - (data - task->result->data);

            fwrite(data, 1, length, fp);
            fclose(fp);

            printf("downloaded %d bytes from %s\n", (int)length, task->url);
        }
        else {
            printf("error in response from %s\n", task->url);
        }

    }
    else {

        fprintf(stderr, "error downloading: %s\n", task->url);

    }

    free_task(task);
}


/**
 * Merge all files in from src to file with name dest synchronously
 * by reading each file, and writing its contents to the dest file.
 * @param src - char pointer to src directory holding files to merge
 * @param dest - char pointer to name of file resulting from merge
 * @param bytes - The maximum byte size downloaded
 * @param tasks - The tasks needed for the multipart download
 */
void merge_files(char *src, char *dest, int bytes, int tasks) {
    assert(0 && "not implemented yet!");
}


/**
 * Remove files caused by chunk downloading
 * @param dir - The directory holding the chunked files
 * @param bytes - The maximum byte size per file. Assumed to be filename
 * @param files - The number of chunked files to remove.
 */
void remove_chunk_files(char *dir, int bytes, int files) {
   assert(0 && "not implemented yet!");
}


int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: ./downloader url_file num_workers download_dir\n");
        exit(1);
    }

    char *url_file = argv[1];
    int num_workers = atoi(argv[2]);
    char *download_dir = argv[3];

    create_directory(download_dir);
    FILE *fp = fopen(url_file, "r");
    char *line = NULL;
    size_t len = 0;

    if (fp == NULL) {
        exit(EXIT_FAILURE);
    }

    // spawn threads and create work queue(s)
    Context *context = spawn_workers(num_workers);

    int work = 0, bytes = 0, num_tasks = 0;
    while ((len = getline(&line, &len, fp)) != -1) {

        if (line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        num_tasks = get_num_tasks(line, num_workers);
        bytes = get_max_chunk_size();
        
        for (int i  = 0; i < num_tasks; i ++) {
            ++work;
            queue_put(context->todo, new_task(line, i * bytes, (i+1) * bytes));
        }
      
        // Get results back
        while (work > 0) {
            --work;
            wait_task(download_dir, context);
        }
        
        /* Merge the files -- simple synchronous method
         * Then remove the chunked download files
         * Beware, this is not an efficient method
         */
        merge_files(download_dir, line, bytes, num_tasks);
        remove_chunk_files(download_dir, bytes, num_tasks);
    }
   

    //cleanup
    fclose(fp);
    free(line);

    free_workers(context);

    return 0;
}
